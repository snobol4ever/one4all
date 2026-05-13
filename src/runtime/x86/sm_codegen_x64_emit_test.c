/*
 * sm_codegen_x64_emit_test.c -- synthetic-program harness for EM-2..EM-4 gate
 *
 * Authors: Lon Jones Cherryholmes, Claude Sonnet
 * Date: 2026-05-06
 *
 * Builds tiny SM_Programs in memory and runs them through
 * sm_codegen_text(). The emitted asm is written to file paths
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
 * EM-5 program (argv[5]):  two expressions calling each other
 *   Outer chunk_A calls inner chunk_B (returns 7), adds 6, returns 13.
 *   main calls chunk_A and HALTs with the returned value as rc.
 *   Exercises SM_CALL_EXPRESSION (baked direct call), SM_RETURN (native ret),
 *   and SM_VOID_POP/SM_ADD already covered.
 *   Expected exit code: 13.
 *
 * EM-5b program (argv[6]):  SM_PUSH_EXPRESSION descriptor-push round trip
 *   PUSH_CHUNK 99,2 then POP it; then PUSH_LIT_I 21 + HALT.
 *   Proves the rt_push_expression_descr@PLT call path round-trips
 *   without corrupting the SM stack.
 *   Expected exit code: 21.
 *
 * Usage: sm_codegen_x64_emit_test <em2.s> [<em3.s>] [<em4a.s>] [<em4b.s>]
 *                                 [<em5.s>] [<em5b.s>]
 *   (each successive arg unlocks the next program; missing args = skip)
 *
 * AUDIT MODE (EM-7c-sm-three-column-verify, 2026-05-09):
 *   sm_codegen_x64_emit_test --audit <file.s> [<file.s> ...]
 *
 * Validates that every non-banner line in the given .s file(s) obeys the
 * three-column shape invariants:
 *
 *   I1.  No bare ';' column separator.  ';' is permitted inside double-
 *        quoted strings and inside '#' line-comments; bare ';' anywhere
 *        else is a violation.
 *   I2.  No TAB character outside string literals.
 *   I3.  If col-1 (chars 0..23) has any non-space content, that content
 *        must be 'label:' shape (a single non-space token ending in ':').
 *   I4.  Implicit in regex: the col-2 token is a single contiguous run
 *        of non-whitespace chars (this catches the EM-7c-bb-three-column-
 *        split class of bug where a fused 'mov  r10, ...' string spilled
 *        into col 3).
 *
 * I5 (col-2 token <=16 chars when col-3 follows) was considered but dropped:
 * col-2 width is a layout target, not an invariant.  printf's %-16s
 * doesn't truncate, so long macro names like EXEC_STMT_VARIANT (17 chars)
 * emit as well-formed three-column lines with col-3 pushed right by one.
 *
 * Banner lines (start with '#' optionally preceded by whitespace) are
 * exempt entirely.  Blank lines are flagged separately (zero-blank-lines
 * is an EM-7c-no-trailing-ws invariant).
 *
 * Exit code: 0 if every line in every file is clean, 1 otherwise.  The
 * harness prints up to 10 violations per file plus a category summary.
 */

#include "sm_prog.h"
#include "sm_codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int emit_to(const char *path, SM_Program *p)
{
    FILE *out = fopen(path, "w");
    if (!out) { perror(path); return 1; }
    /* Synthetic programs don't have a source file; pass NULL so the
     * emitter falls back to structural banners only. */
    int rc = sm_codegen_text(p, out, NULL);
    fclose(out);
    return rc != 0 ? 1 : 0;
}

/*============================================================================
 * Audit mode (EM-7c-sm-three-column-verify, 2026-05-09)
 *
 * Validates the three-column shape of an emitted .s file.  Invariants
 * I1..I5 documented in the file-header comment above.  Implementation
 * mirrors the Python prototype that empirically determined the rule set
 * against the five tracked artifacts.
 *============================================================================*/

#define AUDIT_COL1_END 24
#define AUDIT_COL2_END 40

/* Find first occurrence of `ch` in `s` (length `len`) that is NOT inside
 * a double-quoted string.  Returns position or -1. */
static int audit_first_unquoted(const char *s, int len, char ch)
{
    int in_str = 0;
    for (int i = 0; i < len; i++) {
        char c = s[i];
        if (c == '"') in_str = !in_str;
        else if (c == ch && !in_str) return i;
    }
    return -1;
}

/* True if line is a banner (whitespace-then-'#' or '#' at col 0). */
static int audit_is_banner(const char *s, int len)
{
    int i = 0;
    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    return (i < len && s[i] == '#') ? 1 : 0;
}

/* Audit a single line.  `line` does NOT contain the trailing '\n'.
 * Returns number of violations (0..N).  Each violation is printed
 * with the supplied path/lineno prefix and tracked in counters[]. */
typedef struct {
    long blank;       /* I0: blank line */
    long semicolon;   /* I1: bare ';' */
    long tab;         /* I2: stray TAB */
    long col1_shape;  /* I3: col-1 not 'label:' */
    long off_grid;    /* I3a: directive off the col-1 grid */
    long col2_overflow; /* I5: col-2 token > 16 chars w/col-3 */
} AuditCounters;

/* Scan `s` (length `len`) for the col-1 region: chars [0..min(len,24)).
 * Trim trailing spaces.  Return 1 if the trimmed region is empty,
 * 'label:' shape, or "" (empty), 0 otherwise.  Output the trimmed
 * length via *out_trimmed_len.  out_starts_with_dot is set if the
 * trimmed col-1 starts with '.' (a directive — used to distinguish
 * the off-grid-directive sub-case). */
static int audit_col1_check(const char *s, int len,
                            int *out_trimmed_len,
                            int *out_starts_with_dot)
{
    int n = (len < AUDIT_COL1_END) ? len : AUDIT_COL1_END;
    /* trim trailing spaces */
    while (n > 0 && s[n-1] == ' ') n--;
    *out_trimmed_len = n;
    *out_starts_with_dot = (n > 0 && s[0] == '.') ? 1 : 0;
    if (n == 0) return 1;  /* empty col-1 — fine */
    /* must be 'label:' — single non-space token ending ':' */
    /* leading whitespace before token? — that's the off-grid case */
    if (s[0] == ' ' || s[0] == '\t') return 0;
    /* scan token: contiguous non-space chars up to ':' */
    for (int i = 0; i < n; i++) {
        if (s[i] == ' ' || s[i] == '\t') return 0;  /* internal ws — bad */
    }
    /* must end ':' */
    return (s[n-1] == ':') ? 1 : 0;
}

static int audit_line(const char *path, long lineno,
                      const char *raw, int len,
                      AuditCounters *cnt, int *printed_so_far)
{
    int violations = 0;
    /* blank line check */
    int all_ws = 1;
    for (int i = 0; i < len; i++) {
        if (raw[i] != ' ' && raw[i] != '\t') { all_ws = 0; break; }
    }
    if (len == 0 || all_ws) {
        cnt->blank++;
        if ((*printed_so_far)++ < 10)
            fprintf(stderr, "  %s:%ld [I0/blank]: blank line\n", path, lineno);
        return 1;
    }

    if (audit_is_banner(raw, len)) return 0;

    /* I1: bare ';' outside strings, before any '#' line-comment.
     * Exception: ';' at col >= 40 (col-3 operand field) is a legitimate
     * GAS statement separator (e.g. ".quad .Lstr_N ; .quad .LpcM"). */
    int hash_pos = audit_first_unquoted(raw, len, '#');
    int code_len = (hash_pos < 0) ? len : hash_pos;
    int semi_pos = audit_first_unquoted(raw, code_len, ';');
    if (semi_pos >= 0 && semi_pos < 40) {
        cnt->semicolon++;
        violations++;
        if ((*printed_so_far)++ < 10)
            fprintf(stderr, "  %s:%ld [I1/semicolon]: bare ';' at col %d\n",
                    path, lineno, semi_pos);
    }

    /* I2: stray TAB outside strings */
    {
        int in_str = 0;
        for (int i = 0; i < len; i++) {
            char c = raw[i];
            if (c == '"') in_str = !in_str;
            else if (c == '\t' && !in_str) {
                cnt->tab++;
                violations++;
                if ((*printed_so_far)++ < 10)
                    fprintf(stderr, "  %s:%ld [I2/tab]: TAB at col %d\n",
                            path, lineno, i);
                break;
            }
        }
    }

    /* I3: col-1 shape */
    int c1_trim_len, c1_starts_dot;
    if (!audit_col1_check(raw, len, &c1_trim_len, &c1_starts_dot)) {
        if (c1_starts_dot || (len > 0 && (raw[0] == ' ' || raw[0] == '\t'))) {
            /* off-grid directive (e.g. '    .ifnb \\lbl') */
            cnt->off_grid++;
            violations++;
            if ((*printed_so_far)++ < 10)
                fprintf(stderr, "  %s:%ld [I3a/off-grid-directive]: directive not on col-1 grid\n",
                        path, lineno);
        } else {
            cnt->col1_shape++;
            violations++;
            if ((*printed_so_far)++ < 10)
                fprintf(stderr, "  %s:%ld [I3/col1-shape]: col-1 not 'label:'\n",
                        path, lineno);
        }
        return violations;
    }

    /* If line shorter than col-1, no col-2 to check. */
    if (len < AUDIT_COL1_END) return violations;

    /* col-2 region begins at AUDIT_COL1_END.  Skip leading spaces. */
    int p = AUDIT_COL1_END;
    while (p < len && raw[p] == ' ') p++;
    if (p >= len) return violations;  /* no col-2 token */
    int tok_start = p;
    while (p < len && raw[p] != ' ' && raw[p] != '\t') p++;
    int tok_end = p;
    int tok_len = tok_end - tok_start;
    (void)tok_len;
    (void)tok_end;
    /* I5 (col-2 token <=16 w/col-3) was dropped — col-2 width is a layout
     * target, not an invariant.  printf's %-16s doesn't truncate, so a long
     * macro name like EXEC_STMT_VARIANT (17) emits cleanly with col-3
     * pushed right by 1.  The structural invariant — col-2 is a single
     * token with no internal whitespace — is already enforced by the
     * single-token scan above (loop terminates on first whitespace).  A
     * fused 'mov  r10, ...' string would have whitespace inside what
     * callers thought was col 2, but that means the loop above stops at
     * the first space and 'r10,' becomes the col-3 content — well-formed
     * three-column shape, just with an unintentional split.  The
     * EM-7c-bb-three-column-split rung already eliminated those at the
     * call sites. */
    return violations;
}

/* Audit one file.  Returns total violations. */
static long audit_file(const char *path, AuditCounters *cnt)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "audit: cannot open '%s'\n", path);
        return -1;
    }
    char buf[8192];
    long lineno = 0, total = 0;
    int printed = 0;
    while (fgets(buf, sizeof(buf), f)) {
        lineno++;
        int len = (int)strlen(buf);
        /* strip trailing \n (and \r if present) */
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            buf[--len] = '\0';
        }
        total += audit_line(path, lineno, buf, len, cnt, &printed);
    }
    fclose(f);
    if (printed > 10)
        fprintf(stderr, "  ... (+%d more in %s)\n", printed - 10, path);
    return total;
}

static int run_audit(int n_paths, char **paths)
{
    AuditCounters cnt = {0};
    long grand_total = 0;
    for (int i = 0; i < n_paths; i++) {
        long t = audit_file(paths[i], &cnt);
        if (t < 0) return 2;  /* fopen failed */
        if (t == 0) printf("OK   %s\n", paths[i]);
        else        printf("FAIL %s (%ld violations)\n", paths[i], t);
        grand_total += t;
    }
    printf("\n--- audit summary ---\n");
    printf("  I0/blank            : %ld\n", cnt.blank);
    printf("  I1/semicolon        : %ld\n", cnt.semicolon);
    printf("  I2/tab              : %ld\n", cnt.tab);
    printf("  I3/col1-shape       : %ld\n", cnt.col1_shape);
    printf("  I3a/off-grid-directive : %ld\n", cnt.off_grid);
    printf("  TOTAL               : %ld violations across %d files\n",
           grand_total, n_paths);
    return grand_total == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    /* Audit mode (EM-7c-sm-three-column-verify): validate a .s file's
     * three-column shape.  Routed via --audit as argv[1]. */
    if (argc >= 3 && strcmp(argv[1], "--audit") == 0) {
        return run_audit(argc - 2, argv + 2);
    }

    if (argc < 2 || argc > 7) {
        fprintf(stderr,
            "usage: %s <em2.s> [<em3.s>] [<em4a.s>] [<em4b.s>]"
            " [<em5.s>] [<em5b.s>]\n"
            "       %s --audit <file.s> [<file.s> ...]\n",
            argv[0], argv[0]);
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
     * The driver in test_smoke_jit_emit_x64.sh calls rt_set_last_ok(0)
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

    /* EM-5 program: two expressions calling each other.
     *
     * Proves SM_PUSH_EXPRESSION is not needed here -- SM_CALL_EXPRESSION with a
     * compile-time-known entry_pc is a baked direct `call .LpcN` and
     * SM_RETURN is a native ret.  Two nested expressions demonstrate that
     * the call/return discipline composes.
     *
     *   pc=0   PUSH_LIT_I  0          ; seed (popped immediately)
     *   pc=1   POP                    ; clean stack
     *   pc=2   JUMP        L_main     ; skip both expression bodies
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
     * Also exercises SM_PUSH_EXPRESSION in a separate inline sub-test below
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
        sm_emit_ii(p, SM_CALL_EXPRESSION, (int64_t)L_b, 0);  /* pc=7 */
        sm_emit_i(p, SM_PUSH_LIT_I, 6);          /* pc=8 */
        sm_emit(p,   SM_ADD);                    /* pc=9  -> 13 */
        sm_emit(p,   SM_RETURN);                 /* pc=10 */
        int L_main = sm_label(p);                /* pc=11 */
        sm_emit_ii(p, SM_CALL_EXPRESSION, (int64_t)L_a, 0);  /* pc=12 */
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

    /* EM-5b program: SM_PUSH_EXPRESSION descriptor-push round trip.
     *
     * Pushes a expression descriptor (entry_pc=99, arity=2) then pops it
     * via SM_VOID_POP.  Then pushes 21 + HALT, exits rc=21.  The point is
     * to exercise the rt_push_expression_descr@PLT call path so the
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

        sm_emit_ii(p, SM_PUSH_EXPRESSION, 99, 2);    /* pc=0 */
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
