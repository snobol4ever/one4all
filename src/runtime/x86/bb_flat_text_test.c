/*
 * bb_flat_text_test.c — EM-7b unit test for bb_build_flat_text
 *
 * Authors: Lon Jones Cherryholmes, Claude Sonnet
 * Date:    2026-05-07 (session #75)
 *
 * Verifies:
 *   1. bb_build_flat_text emits a .s file for a simple invariant
 *      pattern (pat_lit("hello")).
 *   2. The .s contains .global directives for the four top-level
 *      labels (_pat_inv_<pid>_<sid>_α/_β/_γ/_ω).
 *   3. The .s assembles cleanly via `gcc -c`.
 *
 * Runs as part of test_smoke_jit_emit_x64.sh (Test 12, EM-7b).
 *
 * This test does NOT (yet) verify byte-stream equivalence between
 * BINARY and TEXT modes — that would require parsing .byte directives
 * out of the .s and comparing to the bb_pool blob.  EM-7c will need
 * that capability for its "binary == text bytes" sanity check; for
 * EM-7b, asm-level assembly success + label visibility is enough.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "snobol4.h"
#include "bb_flat.h"
#include "bb_pool.h"
#include "bb_emit.h"

/* Forward decls for symbols inside libscrip_rt.so ---------------------- */
extern void   SNO_INIT_fn(void);
extern void   bb_pool_init(void);

/* pat_* constructors live in libscrip_rt.so via snobol4_pattern.c ----- */
extern DESCR_t pat_lit(const char *s);

/* PATND_t lives behind DESCR_t.p in pattern descriptors --------------- */
static PATND_t *patnd_from_descr(DESCR_t d) {
    return (PATND_t *)d.p;
}

/* ─────────────────────────────────────────────────────────────────── */

static int n_pass = 0, n_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { n_pass++; } \
    else { n_fail++; fprintf(stderr, "FAIL: %s\n", (msg)); } \
} while (0)

/* ─────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    (void)argc;
    /* output .s path optionally provided by harness */
    const char *out_path = (argc > 1) ? argv[1] : "/tmp/em7b_test.s";

    SNO_INIT_fn();
    bb_pool_init();

    /* ── 1. Build a simple invariant pattern ── */
    DESCR_t  pd = pat_lit("hello");
    PATND_t *p  = patnd_from_descr(pd);

    CHECK(p != NULL, "pat_lit produced NULL PATND_t");

    /* Whole-tree invariance check from EM-7a */
    int invar = patnd_is_fully_invariant(p);
    CHECK(invar == 1, "pat_lit(\"hello\") should be fully invariant");

    /* ── 2. Emit TEXT-mode flat blob ── */
    FILE *out = fopen(out_path, "w");
    CHECK(out != NULL, "fopen output .s file");
    if (!out) return 1;

    /* Wrapper scaffolding so the .s assembles standalone:
     * .intel_syntax noprefix + .text section header + the chunk body.
     * The bb_emit.c text-mode helpers emit Intel syntax (mov rax, ...,
     * call rax, etc.); GAS default is AT&T which interprets bare "rax"
     * as an external symbol — wrong.  This .intel_syntax directive is
     * exactly the same one that sm_codegen_x64_emit.c emits at the top
     * of the .s files it produces. */
    fprintf(out, "    .intel_syntax noprefix\n");
    fprintf(out, "    .text\n");

    int rc = bb_build_flat_text(p, out, "_pat_inv_42_0");
    CHECK(rc == 0, "bb_build_flat_text returned 0 for invariant pattern");

    fclose(out);

    /* ── 3. Verify .global directives present in output ── */
    char buf[16384];
    FILE *fr = fopen(out_path, "r");
    CHECK(fr != NULL, "reopen .s for verification");
    if (!fr) return 1;
    size_t nread = fread(buf, 1, sizeof(buf) - 1, fr);
    buf[nread] = '\0';
    fclose(fr);

    CHECK(strstr(buf, ".global _pat_inv_42_0_α") != NULL,
          ".global _pat_inv_42_0_α missing");
    CHECK(strstr(buf, ".global _pat_inv_42_0_β")  != NULL,
          ".global _pat_inv_42_0_β missing");
    CHECK(strstr(buf, ".global _pat_inv_42_0_γ") != NULL,
          ".global _pat_inv_42_0_γ missing");
    CHECK(strstr(buf, ".global _pat_inv_42_0_ω") != NULL,
          ".global _pat_inv_42_0_ω missing");

    /* ── 4. Verify each label is also DEFINED (not just declared) ── */
    CHECK(strstr(buf, "_pat_inv_42_0_α:") != NULL,
          "_pat_inv_42_0_α definition missing");
    CHECK(strstr(buf, "_pat_inv_42_0_β:")  != NULL,
          "_pat_inv_42_0_β definition missing");
    CHECK(strstr(buf, "_pat_inv_42_0_γ:") != NULL,
          "_pat_inv_42_0_γ definition missing");
    CHECK(strstr(buf, "_pat_inv_42_0_ω:") != NULL,
          "_pat_inv_42_0_ω definition missing");

    /* ── 5. Verify readable-mnemonic emission (EM-7b'': no .byte walls) ── */
    /* EM-7c-symbolic: r10 now loaded via 'lea r10, [rip + Δ]' (symbolic). */
    CHECK(strstr(buf, "lea     r10, [rip + ") != NULL,
          "expected 'lea r10, [rip + Δ]' mnemonic (EM-7c-symbolic) not found");
    CHECK(strstr(buf, ".byte") == NULL,
          "unexpected .byte directive in TEXT output (should be mnemonics)");

    /* ── 5b. EM-7c-symbolic: PLT call to memcmp + symbolic Σlen ref ── */
    CHECK(strstr(buf, "memcmp@PLT") != NULL,
          "expected 'call memcmp@PLT' (EM-7c-symbolic) not found");
    CHECK(strstr(buf, "[rip + ") != NULL,
          "expected at least one [rip + sym] reference (EM-7c-symbolic) not found");

    /* ── 6. Build a fully-invariant ALT pattern (pure literal alt) ── */
    /* Skip this in EM-7b; pat_lit single-leaf is sufficient.  EM-7c
     * will exercise XCAT/XOR partitioning in its own tests. */

    /* ── 7. (Sanity) The same pattern via BINARY mode also works ── */
    bb_box_fn fn = bb_build_flat(p);
    CHECK(fn != NULL, "bb_build_flat (BINARY) for same pattern");

    fprintf(stderr, "PASS=%d FAIL=%d\n", n_pass, n_fail);
    return n_fail == 0 ? 0 : 1;
}
