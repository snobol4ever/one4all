#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "snobol4.h"
#include "emit_bb.h"
#include "bb_pool.h"
#include "emit.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void   SNO_INIT_fn(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void   bb_pool_init(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t pat_lit(const char *s);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static PATND_t *patnd_from_descr(DESCR_t d) {
    return (PATND_t *)d.p;
}
static int n_pass = 0, n_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { n_pass++; } \
    else { n_fail++; fprintf(stderr, "FAIL: %s\n", (msg)); } \
} while (0)
static struct { const char *s; char lbl[32]; } g_lits[64];
static int g_nlit = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *test_intern_str(const char *s) {
    for (int i = 0; i < g_nlit; i++)
        if (g_lits[i].s == s) return g_lits[i].lbl;
    if (g_nlit >= 64) return ".Llit_ovf";
    snprintf(g_lits[g_nlit].lbl, 32, ".Llit%d", g_nlit);
    g_lits[g_nlit].s = s;
    return g_lits[g_nlit++].lbl;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_lit_data(FILE *out) {
    if (!g_nlit) return;
    fprintf(out, "    .section .rodata\n");
    for (int i = 0; i < g_nlit; i++) {
        const char *p = g_lits[i].s ? g_lits[i].s : "";
        fprintf(out, "%s:\n    .asciz \"%s\"\n", g_lits[i].lbl, p);
    }
    fprintf(out, "    .text\n    .intel_syntax noprefix\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int main(int argc, char **argv) {
    (void)argc;
    const char *out_path = (argc > 1) ? argv[1] : "/tmp/em7b_test.s";
    SNO_INIT_fn();
    bb_pool_init();
    DESCR_t  pd = pat_lit("hello");
    PATND_t *p  = patnd_from_descr(pd);
    CHECK(p != NULL, "pat_lit produced NULL PATND_t");
    int invar = patnd_is_fully_invariant(p);
    CHECK(invar == 1, "pat_lit(\"hello\") should be fully invariant");
    emit_flat_set_intern_str(test_intern_str);
    FILE *out = fopen(out_path, "w");
    CHECK(out != NULL, "fopen output .s file");
    if (!out) return 1;
    fprintf(out, "    .intel_syntax noprefix\n");
    fprintf(out, "    .text\n");
    int rc = bb_build_flat_text(p, out, "pat_42_0");
    CHECK(rc == 0, "bb_build_flat_text returned 0 for invariant pattern");
    emit_lit_data(out);
    fclose(out);
    char buf[16384];
    FILE *fr = fopen(out_path, "r");
    CHECK(fr != NULL, "reopen .s for verification");
    if (!fr) return 1;
    size_t nread = fread(buf, 1, sizeof(buf) - 1, fr);
    buf[nread] = '\0';
    fclose(fr);
    CHECK(strstr(buf, ".global") != NULL && strstr(buf, "pat_42_0_α") != NULL,
          ".global _pat_42_0_α missing");
    CHECK(strstr(buf, "pat_42_0_β") != NULL,
          ".global _pat_42_0_β missing");
    CHECK(strstr(buf, "pat_42_0_γ") != NULL,
          ".global _pat_42_0_γ missing");
    CHECK(strstr(buf, "pat_42_0_ω") != NULL,
          ".global _pat_42_0_ω missing");
    CHECK(strstr(buf, "pat_42_0_α:") != NULL,
          "pat_42_0_α definition missing");
    CHECK(strstr(buf, "pat_42_0_β:")  != NULL,
          "pat_42_0_β definition missing");
    CHECK(strstr(buf, "pat_42_0_γ:") != NULL,
          "pat_42_0_γ definition missing");
    CHECK(strstr(buf, "pat_42_0_ω:") != NULL,
          "pat_42_0_ω definition missing");
    CHECK(strstr(buf, "lea") != NULL && strstr(buf, "r10, [rip + ") != NULL,
          "expected 'lea r10, [rip + Δ]' mnemonic (EM-7c-symbolic) not found");
    CHECK(strstr(buf, ".byte") == NULL,
          "unexpected .byte directive in TEXT output (should be mnemonics)");
    CHECK(strstr(buf, "memcmp@PLT") != NULL,
          "expected 'call memcmp@PLT' (EM-7c-symbolic) not found");
    CHECK(strstr(buf, "[rip + ") != NULL,
          "expected at least one [rip + sym] reference (EM-7c-symbolic) not found");
    bb_box_fn fn = bb_build_flat(p);
    CHECK(fn != NULL, "bb_build_flat (BINARY) for same pattern");
    fprintf(stderr, "PASS=%d FAIL=%d\n", n_pass, n_fail);
    return n_fail == 0 ? 0 : 1;
}
