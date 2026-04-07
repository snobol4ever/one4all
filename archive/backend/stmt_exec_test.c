/*
 * stmt_exec_test.c — M-DYN-3 gate test for exec_stmt_args()
 *
 * Standalone build: no GC, no snobol4 runtime, no NASM.
 * Uses STMT_EXEC_STANDALONE to compile stmt_exec.c without snobol4.h.
 *
 * Build (from one4all/):
 *   gcc -Wall -Wno-unused-label -Wno-unused-variable -Wno-misleading-indentation -g -O0 \
 *       -DSTMT_EXEC_STANDALONE \
 *       -I src/runtime/dyn \
 *       src/runtime/dyn/bb_lit.c   \
 *       src/runtime/dyn/bb_alt.c   \
 *       src/runtime/dyn/bb_seq.c   \
 *       src/runtime/dyn/bb_arbno.c \
 *       src/runtime/dyn/bb_pos.c   \
 *       src/runtime/dyn/bb_tab.c   \
 *       src/runtime/dyn/bb_fence.c \
 *       src/runtime/dyn/stmt_exec.c \
 *       src/runtime/dyn/stmt_exec_test.c \
 *       -o stmt_exec_test
 *
 * Gate: ALL PASS
 */

#include "bb_box.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ── define globals owned here ──────────────────────────────────────────── */
const char *Σ = NULL;
int         Δ = 0;
int         Ω = 0;

/* ── minimal DESCR_t for standalone ────────────────────────────────────── */
typedef enum { DT_SNUL=0, DT_S=1, DT_P=3, DT_I=6, DT_FAIL=99 } DTYPE_t;
typedef struct {
    DTYPE_t  v;
    uint32_t slen;
    union { char *s; int64_t i; void *ptr; void *p; };
} DESCR_t;

/* ── stubs ──────────────────────────────────────────────────────────────── */
DESCR_t NV_GET_fn(const char *name) {
    (void)name;
    DESCR_t d; d.v = DT_SNUL; d.slen = 0; d.s = NULL;         return d;
}
void NV_SET_fn(const char *name, DESCR_t val) {
    if (val.v == DT_S && val.s) {
        int n = val.slen ? (int)val.slen : (int)strlen(val.s);
        printf("  capture: %s = \"%.*s\"\n", name, n, val.s);
    }
}
char *VARVAL_fn(DESCR_t d) {
    if (d.v == DT_S)                                          return d.s;
                                                              return NULL;
}

/* forward declarations */
void cache_reset(void);
void cache_stats(int *hits, int *misses);
int  cache_test_run(const char *lit, int n_iters);
int  deferred_var_test(void);
int  anchor_test(void);
int exec_stmt_args(const char *subject, const char *pattern,
                      const char *repl_str, char **out_subject);

/* ── test harness ───────────────────────────────────────────────────────── */
static int failures = 0;

#define CHECK(cond, msg) do { \
    if (cond) printf("  PASS  %s\n", msg); \
    else { printf("  FAIL  %s\n", msg); failures++; } \
} while(0)

int main(void)
{
    printf("stmt_exec_test — M-DYN-3 five-phase executor\n\n");

    /* T1: simple literal match */
    printf("T1: literal match 'Bird' in 'BlueGoldBirdFish'\n");
    CHECK(exec_stmt_args("BlueGoldBirdFish","Bird",NULL,NULL)==1,"T1: match");

    /* T2: literal no-match */
    printf("T2: literal no-match 'Cat'\n");
    CHECK(exec_stmt_args("BlueGoldBirdFish","Cat",NULL,NULL)==0,"T2: no match");

    /* T3: replacement in middle */
    printf("T3: replace 'Bird' -> 'EAGLE'\n");
    { char *out=NULL;
      int r=exec_stmt_args("BlueGoldBirdFish","Bird","EAGLE",&out);
      CHECK(r==1,"T3: match");
      CHECK(out&&strcmp(out,"BlueGoldEAGLEFish")==0,"T3: replacement");
      if(out) printf("  result: \"%s\"\n",out); }

    /* T4: spec_empty pattern = epsilon, always matches */
    printf("T4: spec_empty pattern -> epsilon\n");
    CHECK(exec_stmt_args("hello","",NULL,NULL)==1,"T4: epsilon matches");

    /* T5: match at start */
    printf("T5: match 'Blue' at start\n");
    CHECK(exec_stmt_args("BlueGoldBirdFish","Blue",NULL,NULL)==1,"T5: start match");

    /* T6: match at end */
    printf("T6: match 'Fish' at end\n");
    CHECK(exec_stmt_args("BlueGoldBirdFish","Fish",NULL,NULL)==1,"T6: end match");

    /* T7: replace at start */
    printf("T7: replace 'Blue' -> 'RED'\n");
    { char *out=NULL;
      int r=exec_stmt_args("BlueGoldBirdFish","Blue","RED",&out);
      CHECK(r==1,"T7: match");
      CHECK(out&&strcmp(out,"REDGoldBirdFish")==0,"T7: replacement");
      if(out) printf("  result: \"%s\"\n",out); }

    /* T8: replace at end */
    printf("T8: replace 'Fish' -> 'WHALE'\n");
    { char *out=NULL;
      int r=exec_stmt_args("BlueGoldBirdFish","Fish","WHALE",&out);
      CHECK(r==1,"T8: match");
      CHECK(out&&strcmp(out,"BlueGoldBirdWHALE")==0,"T8: replacement");
      if(out) printf("  result: \"%s\"\n",out); }

    /* T9: delete (spec_empty replacement) */
    printf("T9: delete 'Gold'\n");
    { char *out=NULL;
      int r=exec_stmt_args("BlueGoldBirdFish","Gold","",&out);
      CHECK(r==1,"T9: match");
      CHECK(out&&strcmp(out,"BlueBirdFish")==0,"T9: deletion");
      if(out) printf("  result: \"%s\"\n",out); }

    /* T10: no match, out unchanged */
    printf("T10: no match\n");
    { char *out=NULL;
      int r=exec_stmt_args("BlueGoldBirdFish","ZEBRA","X",&out);
      CHECK(r==0,"T10: :F");
      CHECK(out==NULL,"T10: out not set"); }

    /* T11: exact full-subject match */
    printf("T11: exact full-subject match\n");
    CHECK(exec_stmt_args("BlueGoldBirdFish","BlueGoldBirdFish",NULL,NULL)==1,"T11");

    /* T12: single char */
    printf("T12: single-char match\n");
    CHECK(exec_stmt_args("X","X",NULL,NULL)==1,"T12");

    /* T13: replace whole subject */
    printf("T13: replace whole subject\n");
    { char *out=NULL;
      int r=exec_stmt_args("HELLO","HELLO","WORLD",&out);
      CHECK(r==1,"T13: match");
      CHECK(out&&strcmp(out,"WORLD")==0,"T13: replacement");
      if(out) printf("  result: \"%s\"\n",out); }

    /* T14: M-DYN-OPT — invariant pattern cache
     * bb_build a _XCHR node 10 times via the same static _PND_t pointer.
     * First call = miss, calls 2-10 = hits (9 hits expected).
     * cache_test_run() returns the hit count. */
    printf("T14: M-DYN-OPT invariant cache — 10x bb_build same _PND_t node\n");
    { int hits = cache_test_run("hello", 10);
      printf("  cache hits=%d (expected >= 9)\n", hits);
      CHECK(hits >= 9, "T14: cache hit at least 9 of 10 builds"); }

    /* T15: DYN-4 deferred *VAR dispatch — variable changes between calls.
     * bb_deferred_var must re-resolve NV_GET_fn on every alpha, not just first.
     * We drive it via deferred_var_test(name, val1, val2):
     *   call 1: NV stores val1 → match against val1 should succeed
     *   call 2: NV stores val2 → match against val2 should succeed, val1 fail */
    printf("T15: DYN-4 deferred *VAR — re-resolve on every alpha\n");
    { int r = deferred_var_test();
      CHECK(r == 1, "T15: deferred *VAR re-resolves live value on each alpha"); }

    /* T16: kw_anchor — anchored match must not scan past position 0 */
    printf("T16: kw_anchor — anchored match only at position 0\n");
    { int r = anchor_test();
      CHECK(r == 1, "T16: kw_anchor gates scan to position 0 only"); }

    printf("\n%s  (%d failure%s)\n",
           failures==0?"PASS":"FAIL",
           failures, failures==1?"":"s");
                                                              return failures==0?0:1;
}
