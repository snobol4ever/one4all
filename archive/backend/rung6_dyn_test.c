/*
 * rung6_dyn_test.c — M-DYN-5 Rung 6 corpus gate
 *
 * Drives exec_stmt() against PATND_t trees built from the real
 * snobol4_pattern.c API.  Exercises the XDSAR (*VAR) and XVAR paths
 * through the dynamic engine — the key DYN-4/DYN-5 correctness check.
 *
 * Each test mirrors a corpus/crosscheck/patterns/*.sno program exactly:
 *   - variables set via NV_SET_fn
 *   - patterns built via pat_lit / pat_cat / pat_alt / pat_ref / pat_assign_*
 *   - statement executed via exec_stmt()
 *   - OUTPUT observed via captured output_lines[]
 *
 * Build (from one4all/):
 *   gcc -Wall -Wno-unused-label -Wno-unused-variable -g -O0 \
 *       -I src/runtime/dyn \
 *       -I src/runtime/snobol4 \
 *       -I src/runtime \
 *       -I src/frontend/snobol4 \
 *       src/runtime/dyn/bb_lit.c   \
 *       src/runtime/dyn/bb_alt.c   \
 *       src/runtime/dyn/bb_seq.c   \
 *       src/runtime/dyn/bb_arbno.c \
 *       src/runtime/dyn/bb_pos.c   \
 *       src/runtime/dyn/bb_tab.c   \
 *       src/runtime/dyn/bb_fence.c \
 *       src/runtime/dyn/stmt_exec.c \
 *       src/runtime/snobol4/snobol4.c \
 *       src/runtime/snobol4/snobol4_pattern.c \
 *       src/runtime/mock/mock_includes.c \
 *       src/runtime/engine/engine.c \
 *       src/runtime/engine/runtime.c \
 *       src/runtime/dyn/rung6_dyn_test.c \
 *       -lgc -lm -o rung6_dyn_test
 *
 * Gate: ALL PASS
 */

#include "bb_box.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── runtime headers (full build, not STANDALONE) ─────────────────────── */
#include "snobol4.h"
/* pat_lit, pat_cat, pat_alt, pat_ref, pat_assign_cond etc. declared in snobol4.h */

/* ── subject globals (extern in stmt_exec.c full build) ───────────────── */
const char *Σ = NULL;
int         Ω = 0;
int         Δ = 0;

/* ── per-test reset ───────────────────────────────────────────────────── */
static void runtime_reset(void) {
    SNO_INIT_fn();
    inc_init();
    Σ = NULL; Ω = 0; Δ = 0;
}
static int failures = 0;
static int tests    = 0;

/* Intercept OUTPUT writes for assertion */
#define MAX_OUTPUT_LINES 64
static char  output_lines[MAX_OUTPUT_LINES][256];
static int   output_count = 0;

/* Hook: snobol4.c calls this (weak-linked or we redirect NV_SET for OUTPUT) */
/* We capture by watching NV_SET_fn("OUTPUT", ...) side effects via the
 * real output_val path in snobol4.c, which calls puts().
 * Redirect stdout to a pipe for capture — or simpler: inspect NV_GET_fn
 * after each stmt.  Cleanest: redirect OUTPUT to a capture buffer.
 *
 * Strategy: wrap output via dup2 / pipe before run, restore after.
 * This lets us use the real snobol4 OUTPUT machinery unmodified.
 */
#include <unistd.h>

static int   pipe_fds[2];
static int   saved_stdout;

static void capture_start(void) {
    output_count = 0;
    pipe(pipe_fds);
    saved_stdout = dup(STDOUT_FILENO);
    dup2(pipe_fds[1], STDOUT_FILENO);
    close(pipe_fds[1]);
}

static void capture_end(void) {
    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    /* drain pipe */
    char buf[4096];
    ssize_t n = read(pipe_fds[0], buf, sizeof(buf)-1);
    close(pipe_fds[0]);
    if (n <= 0) return;
    buf[n] = '\0';
    /* split on newlines */
    char *p = buf;
    while (*p && output_count < MAX_OUTPUT_LINES) {
        char *nl = strchr(p, '\n');
        if (!nl) { strncpy(output_lines[output_count++], p, 255); break; }
        *nl = '\0';
        strncpy(output_lines[output_count++], p, 255);
        p = nl + 1;
    }
}

#define EXPECT_OUTPUT(idx, want) do { \
    tests++; \
    if (output_count <= (idx)) { \
        printf("  FAIL  output line %d missing (want \"%s\")\n", (idx), (want)); \
        failures++; \
    } else if (strcmp(output_lines[(idx)], (want)) != 0) { \
        printf("  FAIL  output[%d]: got \"%s\", want \"%s\"\n", \
               (idx), output_lines[(idx)], (want)); \
        failures++; \
    } else { \
        printf("  PASS  output[%d] == \"%s\"\n", (idx), (want)); \
    } \
} while(0)

#define EXPECT_MATCH(result, want, label) do { \
    tests++; \
    if ((result) != (want)) { \
        printf("  FAIL  %s: got %d, want %d\n", (label), (result), (want)); \
        failures++; \
    } else { \
        printf("  PASS  %s\n", (label)); \
    } \
} while(0)

/* Helper: make a DT_S DESCR_t from a C string */
static DESCR_t str_val(const char *s) {
    DESCR_t d;
    d.v    = DT_S;
    d.s    = (char *)s;
    d.slen = (uint32_t)strlen(s);
    return d;
}

/* ── T1: 056_pat_star_deref — *PAT indirect reference ─────────────────
 *
 *   SNOBOL4 source:
 *     PAT = 'hello'
 *     X = 'say hello world'
 *     X *PAT . V    :S(YES)
 *     OUTPUT = 'fail'
 *     :(END)
 *   YES  OUTPUT = V
 *   END
 *
 *   Expected output: hello
 */
static void test_056_star_deref(void) {
    printf("\nT1: 056_pat_star_deref — *PAT indirect reference\n");
    runtime_reset();

    /* PAT = 'hello' */
    NV_SET_fn("PAT", str_val("hello"));

    /* X = 'say hello world' */
    NV_SET_fn("X", str_val("say hello world"));

    DESCR_t pat_star = pat_ref("PAT");
    DESCR_t var_v    = str_val("V");
    DESCR_t pat      = pat_assign_cond(pat_star, var_v);

    /* capture_start AFTER all setup printf, BEFORE the statement */
    fflush(stdout);
    capture_start();
    int matched = exec_stmt("X", NULL, pat, NULL, 0);
    if (matched) {
        DESCR_t v = NV_GET_fn("V");
        NV_SET_fn("OUTPUT", v);
    } else {
        NV_SET_fn("OUTPUT", str_val("fail"));
    }
    capture_end();

    EXPECT_MATCH(matched, 1, "*PAT matched 'hello' in subject");
    EXPECT_OUTPUT(0, "hello");
}

/* ── T2: *PAT with mutated PAT between calls ───────────────────────────
 *
 *   Verifies XDSAR resolves at match time (DYN-4 correctness):
 *     PAT = 'foo'
 *     X = 'foo bar baz'
 *     X *PAT . V   → should match 'foo'
 *     PAT = 'baz'
 *     X *PAT . V   → should match 'baz' (not 'foo')
 */
static void test_xdsar_mutation(void) {
    printf("\nT2: XDSAR resolves at match time (mutation between calls)\n");
    runtime_reset();

    NV_SET_fn("X", str_val("foo bar baz"));

    /* First match: PAT = 'foo' */
    NV_SET_fn("PAT", str_val("foo"));
    DESCR_t pat  = pat_assign_cond(pat_ref("PAT"), str_val("V"));
    int r1 = exec_stmt("X", NULL, pat, NULL, 0);
    DESCR_t v1 = NV_GET_fn("V");
    EXPECT_MATCH(r1, 1, "first match (PAT='foo')");
    tests++;
    if (v1.v == DT_S && v1.s && strcmp(v1.s, "foo") == 0) {
        printf("  PASS  V == 'foo'\n");
    } else {
        printf("  FAIL  V == '%s', want 'foo'\n", v1.s ? v1.s : "(null)");
        failures++;
    }

    /* Mutate PAT, rebuild pattern (new XDSAR — deferred resolution) */
    NV_SET_fn("PAT", str_val("baz"));
    DESCR_t pat2 = pat_assign_cond(pat_ref("PAT"), str_val("V"));
    int r2 = exec_stmt("X", NULL, pat2, NULL, 0);
    DESCR_t v2 = NV_GET_fn("V");
    EXPECT_MATCH(r2, 1, "second match (PAT='baz')");
    tests++;
    if (v2.v == DT_S && v2.s && strcmp(v2.s, "baz") == 0) {
        printf("  PASS  V == 'baz'\n");
    } else {
        printf("  FAIL  V == '%s', want 'baz'\n", v2.s ? v2.s : "(null)");
        failures++;
    }
}

/* ── T3: pat_ref in ALT — *P1 | *P2 ───────────────────────────────────
 *
 *   PAT1 = 'Bird'
 *   PAT2 = 'Fish'
 *   X = 'BlueGoldBirdFish'
 *   X (*PAT1 | *PAT2) . V   → matches 'Bird' (first alt wins)
 */
static void test_xdsar_in_alt(void) {
    printf("\nT3: *PAT1 | *PAT2 — XDSAR inside ALT\n");
    runtime_reset();

    NV_SET_fn("PAT1", str_val("Bird"));
    NV_SET_fn("PAT2", str_val("Fish"));
    NV_SET_fn("X",    str_val("BlueGoldBirdFish"));

    DESCR_t alt = pat_alt(pat_ref("PAT1"), pat_ref("PAT2"));
    DESCR_t pat = pat_assign_cond(alt, str_val("V"));

    int r = exec_stmt("X", NULL, pat, NULL, 0);
    DESCR_t v = NV_GET_fn("V");
    EXPECT_MATCH(r, 1, "(*PAT1|*PAT2) matched");
    tests++;
    if (v.v == DT_S && v.s && strcmp(v.s, "Bird") == 0) {
        printf("  PASS  V == 'Bird' (first alt)\n");
    } else {
        printf("  FAIL  V == '%s', want 'Bird'\n", v.s ? v.s : "(null)");
        failures++;
    }
}

/* ── T4: pat_assign_cond (XNME) ordering — . capture only on success ───
 *
 *   X = 'hello world'
 *   X ('hello' . V) 'NOMATCH'   → :F, V must remain unset
 *
 *   If XNME eagerly commits, V gets 'hello' even on overall failure.
 *   Correct: V stays empty.
 */
static void test_xnme_conditional(void) {
    printf("\nT4: XNME conditional — capture only on overall success\n");
    runtime_reset();

    NV_SET_fn("X", str_val("hello world"));
    /* Clear V */
    NV_SET_fn("V", str_val(""));

    /* Pattern: ('hello' . V) 'NOMATCH' — will fail overall */
    DESCR_t cap  = pat_assign_cond(pat_lit("hello"), str_val("V"));
    DESCR_t pat  = pat_cat(cap, pat_lit("NOMATCH"));

    int r = exec_stmt("X", NULL, pat, NULL, 0);
    EXPECT_MATCH(r, 0, "('hello'.V)'NOMATCH' fails overall");

    /* V must be empty — capture was conditional, not committed */
    DESCR_t v = NV_GET_fn("V");
    tests++;
    int v_empty = (v.v == DT_S && (!v.s || strcmp(v.s, "") == 0));
    if (v_empty) {
        printf("  PASS  V not set (conditional capture correct)\n");
    } else {
        printf("  FAIL  V == '%s', want '' (eager capture bug)\n",
               (v.v == DT_S && v.s) ? v.s : "(non-string)");
        failures++;
    }
}

/* ── T5: *PAT where PAT holds a string (not pattern) ──────────────────
 *
 *   SNOBOL4: a variable holding a string value used as *V should
 *   match that string literally.
 *
 *   PAT = 'world'   (DT_S, not DT_P)
 *   X = 'hello world'
 *   X *PAT . V   → matches 'world'
 */
static void test_xdsar_string_val(void) {
    printf("\nT5: *PAT where PAT holds a string (DT_S literal match)\n");
    runtime_reset();

    /* PAT is a plain string, not a pattern object */
    NV_SET_fn("PAT", str_val("world"));
    NV_SET_fn("X",   str_val("hello world"));

    DESCR_t pat = pat_assign_cond(pat_ref("PAT"), str_val("V"));
    int r = exec_stmt("X", NULL, pat, NULL, 0);
    DESCR_t v = NV_GET_fn("V");
    EXPECT_MATCH(r, 1, "*PAT(DT_S) matched 'world'");
    tests++;
    if (v.v == DT_S && v.s && strcmp(v.s, "world") == 0) {
        printf("  PASS  V == 'world'\n");
    } else {
        printf("  FAIL  V == '%s', want 'world'\n", v.s ? v.s : "(null)");
        failures++;
    }
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
    printf("rung6_dyn_test — M-DYN-5 Rung 6 corpus gate (XDSAR/XVAR)\n");

    test_056_star_deref();
    test_xdsar_mutation();
    test_xdsar_in_alt();
    test_xnme_conditional();
    test_xdsar_string_val();

    printf("\n");
    if (failures == 0) {
        printf("PASS  (%d/%d)\n", tests, tests);
    } else {
        printf("FAIL  (%d failures / %d tests)\n", failures, tests);
    }
    return failures ? 1 : 0;
}
