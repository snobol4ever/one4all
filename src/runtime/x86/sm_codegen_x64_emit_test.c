/*
 * sm_codegen_x64_emit_test.c -- synthetic-program harness for EM-2..EM-4 gate
 *
 * Authors: Lon Jones Cherryholmes, Claude Sonnet
 * Date: 2026-05-06
 *
 * Builds tiny SM_Programs in memory and runs them through
 * sm_codegen_x64_emit(). The emitted asm is written to file paths
 * provided by argv. The companion shell script assembles, links against
 * libscrip_rt.so, runs, and checks the rc.
 *
 * EM-2 program (argv[1]):
 *   pc=0  SM_PUSH_LIT_I  42
 *   pc=1  SM_HALT
 *   Expected exit code: 42.
 *
 * EM-3 program (argv[2]):  (2 + 3) * 4 = 20
 *   pc=0  SM_PUSH_LIT_I  2
 *   pc=1  SM_PUSH_LIT_I  3
 *   pc=2  SM_ADD
 *   pc=3  SM_PUSH_LIT_I  4
 *   pc=4  SM_MUL
 *   pc=5  SM_HALT
 *   Expected exit code: 20.
 *
 * EM-4a program (argv[3]):  forward jump + conditional shapes
 *   Demonstrates SM_JUMP, SM_JUMP_F (not taken when last_ok=1),
 *   SM_JUMP_S (taken when last_ok=1).  At process start last_ok=1
 *   so we can verify all three opcode shapes fire correctly without
 *   needing a runtime hook to flip last_ok.
 *   Expected exit code: 42.
 *
 * EM-4b program (argv[4]):  conditional backward loop body
 *   The asm produced is a small countdown loop:
 *     pc=0  PUSH_LIT_I 3                ; counter
 *     pc=1  LABEL                       ; loop top
 *     pc=2  PUSH_LIT_I 1
 *     pc=3  SUB                         ; counter -= 1
 *     pc=4  JUMP_F  1                   ; backward jump if !last_ok
 *     pc=5  HALT                        ; rc = remaining counter (0)
 *   Companion C driver flips last_ok to 0 before entering the SM body
 *   on iterations 1..3, then to 1 on the final iteration so the loop
 *   exits.  This proves the SM_JUMP_F backward-jump shape executes
 *   correctly.  See scripts/test_smoke_jit_emit_x64.sh test 6b.
 *   Expected exit code: 0.
 *
 * EM-5 program (argv[5]):  two chunks calling each other
 *   Outer chunk_A calls inner chunk_B (returns 7), adds 6, returns 13.
 *   main calls chunk_A and HALTs with the returned value as rc.
 *   Exercises SM_CALL_CHUNK (baked direct call), SM_RETURN (native ret),
 *   and SM_VOID_POP/SM_ADD already covered.
 *   Expected exit code: 13.
 *
 * EM-5b program (argv[6]):  SM_PUSH_CHUNK descriptor-push round trip
 *   PUSH_CHUNK 99,2 then POP it; then PUSH_LIT_I 21 + HALT.
 *   Proves the scrip_rt_push_chunk_descr@PLT call path round-trips
 *   without corrupting the SM stack.
 *   Expected exit code: 21.
 *
 * Usage: sm_codegen_x64_emit_test <em2.s> [<em3.s>] [<em4a.s>] [<em4b.s>]
 *                                 [<em5.s>] [<em5b.s>]
 *   (each successive arg unlocks the next program; missing args = skip)
 */

#include "sm_prog.h"
#include "sm_codegen_x64_emit.h"

#include <stdio.h>
#include <stdlib.h>

static int emit_to(const char *path, SM_Program *p)
{
    FILE *out = fopen(path, "w");
    if (!out) { perror(path); return 1; }
    /* Synthetic programs don't have a source file; pass NULL so the
     * emitter falls back to structural banners only. */
    int rc = sm_codegen_x64_emit(p, out, NULL);
    fclose(out);
    return rc != 0 ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 7) {
        fprintf(stderr,
            "usage: %s <em2.s> [<em3.s>] [<em4a.s>] [<em4b.s>]"
            " [<em5.s>] [<em5b.s>]\n", argv[0]);
        return 2;
    }

    /* EM-2 program: PUSH_LIT_I 42 + HALT */
    {
        SM_Program *p = sm_prog_new();
        if (!p) { fprintf(stderr, "sm_prog_new failed\n"); return 1; }

        sm_emit_i(p, SM_PUSH_LIT_I, 42);
        sm_emit(p, SM_HALT);

        int rc = emit_to(argv[1], p);
        sm_prog_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-2 program\n");
            return 1;
        }
    }

    if (argc < 3) return 0;

    /* EM-3 program: (2 + 3) * 4 = 20 */
    {
        SM_Program *p = sm_prog_new();
        if (!p) { fprintf(stderr, "sm_prog_new failed\n"); return 1; }

        sm_emit_i(p, SM_PUSH_LIT_I, 2);
        sm_emit_i(p, SM_PUSH_LIT_I, 3);
        sm_emit(p,  SM_ADD);
        sm_emit_i(p, SM_PUSH_LIT_I, 4);
        sm_emit(p,  SM_MUL);
        sm_emit(p,  SM_HALT);

        int rc = emit_to(argv[2], p);
        sm_prog_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-3 program\n");
            return 1;
        }
    }

    if (argc < 4) return 0;

    /* EM-4a program: forward jump + conditional jump shapes (last_ok=1).
     *
     * pc=0   PUSH_LIT_I  100      ; seed value on TOS
     * pc=1   JUMP    L_skip       ; forward jump skips dead code
     * pc=2   PUSH_LIT_I  99       ; DEAD
     * pc=3   HALT                 ; DEAD (would exit 99)
     * pc=4   LABEL  (L_skip)      ; landing pad
     * pc=5   PUSH_LIT_I  -58      ; -58
     * pc=6   ADD                  ; 100 + -58 = 42
     * pc=7   JUMP_F  L_dead       ; last_ok=1, NOT taken
     * pc=8   JUMP_S  L_final      ; last_ok=1, IS taken
     * pc=9   PUSH_LIT_I  77       ; DEAD-by-jump_s
     * pc=10  HALT                 ; DEAD-by-jump_s (would exit 77)
     * pc=11  LABEL  (L_dead)      ; would be JUMP_F target; unreached
     * pc=12  HALT                 ; unreached
     * pc=13  LABEL  (L_final)     ; JUMP_S landing
     * pc=14  HALT                 ; rc=42
     */
    {
        SM_Program *p = sm_prog_new();
        if (!p) { fprintf(stderr, "sm_prog_new failed\n"); return 1; }

        sm_emit_i(p, SM_PUSH_LIT_I, 100);    /* pc=0 */
        int j_skip = sm_emit_i(p, SM_JUMP, 0); /* pc=1 patch later */
        sm_emit_i(p, SM_PUSH_LIT_I, 99);     /* pc=2 dead */
        sm_emit(p,   SM_HALT);               /* pc=3 dead */
        int L_skip = sm_label(p);            /* pc=4 */
        sm_emit_i(p, SM_PUSH_LIT_I, -58);    /* pc=5 */
        sm_emit(p,   SM_ADD);                /* pc=6 -> TOS=42 */
        int j_dead = sm_emit_i(p, SM_JUMP_F, 0); /* pc=7 patch later */
        int j_final= sm_emit_i(p, SM_JUMP_S, 0); /* pc=8 patch later */
        sm_emit_i(p, SM_PUSH_LIT_I, 77);     /* pc=9 dead-by-jump_s */
        sm_emit(p,   SM_HALT);               /* pc=10 dead */
        int L_dead = sm_label(p);            /* pc=11 unreached */
        sm_emit(p,   SM_HALT);               /* pc=12 unreached */
        int L_final= sm_label(p);            /* pc=13 */
        sm_emit(p,   SM_HALT);               /* pc=14 rc=42 */

        sm_patch_jump(p, j_skip,  L_skip);
        sm_patch_jump(p, j_dead,  L_dead);
        sm_patch_jump(p, j_final, L_final);

        int rc = emit_to(argv[3], p);
        sm_prog_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-4a program\n");
            return 1;
        }
    }

    if (argc < 5) return 0;

    /* EM-4b program: conditional backward loop body.
     *
     * pc=0   PUSH_LIT_I 3              ; counter on TOS
     * pc=1   LABEL                     ; loop top
     * pc=2   PUSH_LIT_I 1
     * pc=3   SUB                       ; counter -= 1
     * pc=4   JUMP_F  1                 ; if !last_ok, backward to loop top
     * pc=5   HALT                      ; rc = final counter
     *
     * The driver in test_smoke_jit_emit_x64.sh calls scrip_rt_set_last_ok(0)
     * before entering this body to drive the loop, then sets it to 1 to exit.
     * This proves SM_JUMP_F backward-jump shape executes correctly.
     */
    {
        SM_Program *p = sm_prog_new();
        if (!p) { fprintf(stderr, "sm_prog_new failed\n"); return 1; }

        sm_emit_i(p, SM_PUSH_LIT_I, 3);      /* pc=0 */
        int L_top = sm_label(p);             /* pc=1 */
        sm_emit_i(p, SM_PUSH_LIT_I, 1);      /* pc=2 */
        sm_emit(p,   SM_SUB);                /* pc=3 -> counter-=1 */
        int j_back = sm_emit_i(p, SM_JUMP_F, 0); /* pc=4 */
        sm_emit(p,   SM_HALT);               /* pc=5 */

        sm_patch_jump(p, j_back, L_top);

        int rc = emit_to(argv[4], p);
        sm_prog_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-4b program\n");
            return 1;
        }
    }

    if (argc < 6) return 0;

    /* EM-5 program: two chunks calling each other.
     *
     * Proves SM_PUSH_CHUNK is not needed here -- SM_CALL_CHUNK with a
     * compile-time-known entry_pc is a baked direct `call .LpcN` and
     * SM_RETURN is a native ret.  Two nested chunks demonstrate that
     * the call/return discipline composes.
     *
     *   pc=0   PUSH_LIT_I  0          ; seed (popped immediately)
     *   pc=1   POP                    ; clean stack
     *   pc=2   JUMP        L_main     ; skip both chunk bodies
     *   pc=3   LABEL                  ; chunk_B entry_pc=3
     *   pc=4   PUSH_LIT_I  7
     *   pc=5   RETURN                 ; -> 7 on TOS, ret to caller
     *   pc=6   LABEL                  ; chunk_A entry_pc=6
     *   pc=7   CALL_CHUNK  3          ; call B, leaves 7 on TOS
     *   pc=8   PUSH_LIT_I  6
     *   pc=9   ADD                    ; 7 + 6 = 13
     *   pc=10  RETURN                 ; -> 13 on TOS, ret to caller
     *   pc=11  LABEL  (L_main)
     *   pc=12  CALL_CHUNK  6          ; call A, leaves 13 on TOS
     *   pc=13  HALT                   ; rc = 13
     *
     * Also exercises SM_PUSH_CHUNK in a separate inline sub-test below
     * (push the descriptor, then pop it -- proves the descriptor-push
     * path works even though the EM-5 gate's hot path bakes direct).
     */
    {
        SM_Program *p = sm_prog_new();
        if (!p) { fprintf(stderr, "sm_prog_new failed\n"); return 1; }

        sm_emit_i(p, SM_PUSH_LIT_I, 0);          /* pc=0  seed */
        sm_emit(p,   SM_VOID_POP);                    /* pc=1  clean stack */
        int j_main = sm_emit_i(p, SM_JUMP, 0);   /* pc=2  patch later */
        int L_b    = sm_label(p);                /* pc=3  chunk_B entry */
        sm_emit_i(p, SM_PUSH_LIT_I, 7);          /* pc=4 */
        sm_emit(p,   SM_RETURN);                 /* pc=5 */
        int L_a    = sm_label(p);                /* pc=6  chunk_A entry */
        sm_emit_ii(p, SM_CALL_CHUNK, (int64_t)L_b, 0);  /* pc=7 */
        sm_emit_i(p, SM_PUSH_LIT_I, 6);          /* pc=8 */
        sm_emit(p,   SM_ADD);                    /* pc=9  -> 13 */
        sm_emit(p,   SM_RETURN);                 /* pc=10 */
        int L_main = sm_label(p);                /* pc=11 */
        sm_emit_ii(p, SM_CALL_CHUNK, (int64_t)L_a, 0);  /* pc=12 */
        sm_emit(p,   SM_HALT);                   /* pc=13 rc=13 */

        sm_patch_jump(p, j_main, L_main);

        int rc = emit_to(argv[5], p);
        sm_prog_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-5 program\n");
            return 1;
        }
    }

    if (argc < 7) return 0;

    /* EM-5b program: SM_PUSH_CHUNK descriptor-push round trip.
     *
     * Pushes a chunk descriptor (entry_pc=99, arity=2) then pops it
     * via SM_VOID_POP.  Then pushes 21 + HALT, exits rc=21.  The point is
     * to exercise the scrip_rt_push_chunk_descr@PLT call path so the
     * gate's grep-shape check has something to look at.
     *
     *   pc=0   PUSH_CHUNK  99, 2
     *   pc=1   POP
     *   pc=2   PUSH_LIT_I  21
     *   pc=3   HALT
     */
    {
        SM_Program *p = sm_prog_new();
        if (!p) { fprintf(stderr, "sm_prog_new failed\n"); return 1; }

        sm_emit_ii(p, SM_PUSH_CHUNK, 99, 2);    /* pc=0 */
        sm_emit(p,   SM_VOID_POP);                    /* pc=1 */
        sm_emit_i(p, SM_PUSH_LIT_I, 21);         /* pc=2 */
        sm_emit(p,   SM_HALT);                   /* pc=3 rc=21 */

        int rc = emit_to(argv[6], p);
        sm_prog_free(p);
        if (rc != 0) {
            fprintf(stderr, "sm_codegen_x64_emit failed for EM-5b program\n");
            return 1;
        }
    }

    return 0;
}
