/*
 * test_snocone_parse_g.c — GOAL-SNOCONE-LANG-SPACE LS-4.g acceptance test
 *
 * Verifies that the new Bison-based Snocone parser handles:
 *   do { S } while (C);
 *   for (init; cond; step) S
 *
 * Lowering shapes (all verified below):
 *
 *   do { S } while (C)   ->   Ltop ; <S> ; C:S(Ltop) ; Lend
 *   for (I; C; T) S      ->   <I> ; Ltop ; C:F(Lend) ; <S> ; <T> ; :(Ltop) ; Lend
 *
 * Note: do/until removed per Lon directive session 2026-04-30 #12 —
 * Snocone follows C's loop forms exactly (while and do/while only).
 * Tests that previously used do/until syntax now verify parse failure.
 *
 * Build:
 *   cc -Wall -o test_snocone_parse_g test_snocone_parse_g.c \
 *       ../../src/frontend/snocone/snocone_parse.tab.c \
 *       ../../src/frontend/snocone/snocone_lex.c \
 *       -I ../../src/frontend/snocone \
 *       -I ../../src/frontend/snobol4 \
 *       -I ../../src
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
 * Commit identity: LCherryholmes / lcherryh@yahoo.com  (RULES.md)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IR_DEFINE_NAMES
#include "scrip_cc.h"

Program *snocone_parse_program(const char *src, const char *filename);

/* ---- harness ---- */
static int g_pass = 0, g_fail = 0;

#define ASSERT(cond, fmt, ...) do { \
    if (cond) { g_pass++; } \
    else { fprintf(stderr, "FAIL %s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__); g_fail++; } \
} while(0)

/* Walk the linked-list and collect a STMT_t* array (caller must free). */
static STMT_t **collect_stmts(Program *prog, int *out_n) {
    int n = 0;
    for (STMT_t *s = prog->head; s; s = s->next) n++;
    STMT_t **arr = calloc(n, sizeof *arr);
    int i = 0;
    for (STMT_t *s = prog->head; s; s = s->next) arr[i++] = s;
    *out_n = n;
    return arr;
}

/* True if stmt is a bare label-only landing pad (no subject). */
static int is_label_pad(STMT_t *s) { return s->label && !s->subject; }
/* True if stmt is a goto-only statement (no subject). */
static int is_goto_uncond(STMT_t *s) { return s->go && s->go->uncond && !s->subject; }
/* True if stmt has a cond-fail branch (subject + onfailure). */
static int is_cond_fail(STMT_t *s) { return s->subject && s->go && s->go->onfailure; }
/* True if stmt has a cond-success branch (subject + onsuccess). */
static int is_cond_succ(STMT_t *s) { return s->subject && s->go && s->go->onsuccess; }
/* True if stmt is a bare expression (subject, no goto). */
static int is_bare_expr(STMT_t *s) { return s->subject && !s->go; }

/* =========================================================================
 * Test 1 — do/while basic shape
 *   do { x = 1; } while (GT(x, 0));
 *   Expected: Ltop  ;  x=1(bare)  ;  GT(x,0):S(Ltop)  ;  Lend
 * ========================================================================= */
static void test_do_while_basic(void) {
    Program *prog = snocone_parse_program(
        "do { x = 1; } while (GT(x, 0));", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 4, "expected 4 stmts, got %d", n);
    if (n >= 1) ASSERT(is_label_pad(stmts[0]),              "stmt[0] should be Ltop pad");
    if (n >= 2) ASSERT(is_bare_expr(stmts[1]),              "stmt[1] should be x=1 bare");
    if (n >= 3) ASSERT(is_cond_succ(stmts[2]),              "stmt[2] should be cond:S(Ltop)");
    if (n >= 3) ASSERT(stmts[2]->go->onsuccess &&
                       strcmp(stmts[2]->go->onsuccess, stmts[0]->label) == 0,
                       "cond:S target should equal Ltop");
    if (n >= 4) ASSERT(is_label_pad(stmts[3]),              "stmt[3] should be Lend pad");
    free(stmts);
}

/* =========================================================================
 * Test 2 — do/until is now a syntax error (removed per Lon directive #12)
 *   Snocone follows C's loop forms: while and do/while only.
 * ========================================================================= */
static void test_do_until_basic(void) {
    Program *prog = snocone_parse_program(
        "do { x = x + 1; } until (GT(x, 10));", "test");
    ASSERT(prog == NULL, "do/until must be a syntax error after removal");
}

/* =========================================================================
 * Test 3 — for basic shape
 *   for (i = 0; LT(i, 10); i = i + 1) x = x + i;
 *   Expected: i=0(bare) ; Ltop ; LT:F(Lend) ; x+=i(bare) ; i++(bare) ; :(Ltop) ; Lend
 * ========================================================================= */
static void test_for_basic(void) {
    Program *prog = snocone_parse_program(
        "for (i = 0; LT(i, 10); i = i + 1) x = x + i;", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 7, "expected 7 stmts, got %d", n);
    if (n >= 1) ASSERT(is_bare_expr(stmts[0]),              "stmt[0] should be init (i=0)");
    if (n >= 2) ASSERT(is_label_pad(stmts[1]),              "stmt[1] should be Ltop pad");
    if (n >= 3) ASSERT(is_cond_fail(stmts[2]),              "stmt[2] should be cond:F(Lend)");
    if (n >= 4) ASSERT(is_bare_expr(stmts[3]),              "stmt[3] should be body (x=x+i)");
    if (n >= 5) ASSERT(is_bare_expr(stmts[4]),              "stmt[4] should be step (i=i+1)");
    if (n >= 6) ASSERT(is_goto_uncond(stmts[5]),            "stmt[5] should be :(Ltop)");
    if (n >= 6) ASSERT(stmts[5]->go->uncond &&
                       strcmp(stmts[5]->go->uncond, stmts[1]->label) == 0,
                       "goto target should equal Ltop");
    if (n >= 7) ASSERT(is_label_pad(stmts[6]),              "stmt[6] should be Lend pad");
    if (n >= 3 && n >= 7)
                ASSERT(stmts[2]->go->onfailure &&
                       strcmp(stmts[2]->go->onfailure, stmts[6]->label) == 0,
                       "cond:F target should equal Lend");
    free(stmts);
}

/* =========================================================================
 * Test 4 — for with block body
 *   for (i = 0; LT(i, 5); i = i + 1) { a = a + i; b = b + 1; }
 *   Expected: init ; Ltop ; cond:F(Lend) ; a+=i ; b+=1 ; step ; :(Ltop) ; Lend
 *   = 8 stmts
 * ========================================================================= */
static void test_for_block_body(void) {
    Program *prog = snocone_parse_program(
        "for (i = 0; LT(i, 5); i = i + 1) { a = a + i; b = b + 1; }", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 8, "expected 8 stmts, got %d", n);
    if (n >= 1) ASSERT(is_bare_expr(stmts[0]), "stmt[0] init");
    if (n >= 2) ASSERT(is_label_pad(stmts[1]), "stmt[1] Ltop");
    if (n >= 3) ASSERT(is_cond_fail(stmts[2]), "stmt[2] cond:F");
    if (n >= 4) ASSERT(is_bare_expr(stmts[3]), "stmt[3] body a");
    if (n >= 5) ASSERT(is_bare_expr(stmts[4]), "stmt[4] body b");
    if (n >= 6) ASSERT(is_bare_expr(stmts[5]), "stmt[5] step");
    if (n >= 7) ASSERT(is_goto_uncond(stmts[6]), "stmt[6] :(Ltop)");
    if (n >= 8) ASSERT(is_label_pad(stmts[7]), "stmt[7] Lend");
    free(stmts);
}

/* =========================================================================
 * Test 5 — do/while with block body (multiple stmts)
 *   do { a = 1; b = 2; } while (LT(a, 10));
 *   Expected: Ltop ; a=1 ; b=2 ; cond:S(Ltop) ; Lend  (5 stmts)
 * ========================================================================= */
static void test_do_while_block(void) {
    Program *prog = snocone_parse_program(
        "do { a = 1; b = 2; } while (LT(a, 10));", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 5, "expected 5 stmts, got %d", n);
    if (n >= 1) ASSERT(is_label_pad(stmts[0]),   "stmt[0] Ltop");
    if (n >= 2) ASSERT(is_bare_expr(stmts[1]),   "stmt[1] a=1");
    if (n >= 3) ASSERT(is_bare_expr(stmts[2]),   "stmt[2] b=2");
    if (n >= 4) ASSERT(is_cond_succ(stmts[3]),   "stmt[3] cond:S(Ltop)");
    if (n >= 5) ASSERT(is_label_pad(stmts[4]),   "stmt[4] Lend");
    free(stmts);
}

/* =========================================================================
 * Test 6 — for with empty body (semicolon stmt)
 *   for (i = 0; LT(i, 10); i = i + 1) ;
 *   Expected: init ; Ltop ; cond:F(Lend) ; step ; :(Ltop) ; Lend (6 stmts)
 *   (empty stmt produces nothing)
 * ========================================================================= */
static void test_for_empty_body(void) {
    Program *prog = snocone_parse_program(
        "for (i = 0; LT(i, 10); i = i + 1) ;", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 6, "expected 6 stmts (no body stmt), got %d", n);
    if (n >= 1) ASSERT(is_bare_expr(stmts[0]),   "stmt[0] init");
    if (n >= 2) ASSERT(is_label_pad(stmts[1]),   "stmt[1] Ltop");
    if (n >= 3) ASSERT(is_cond_fail(stmts[2]),   "stmt[2] cond:F");
    if (n >= 4) ASSERT(is_bare_expr(stmts[3]),   "stmt[3] step");
    if (n >= 5) ASSERT(is_goto_uncond(stmts[4]), "stmt[4] :(Ltop)");
    if (n >= 6) ASSERT(is_label_pad(stmts[5]),   "stmt[5] Lend");
    free(stmts);
}

/* =========================================================================
 * Test 7 — label identity for/while — unique labels per construct
 *   Two consecutive for loops — each pair of Ltop/Lend labels must be distinct.
 * ========================================================================= */
static void test_unique_labels(void) {
    Program *prog = snocone_parse_program(
        "for (i = 0; LT(i, 5); i = i + 1) x = 1;"
        "for (j = 0; LT(j, 5); j = j + 1) y = 1;", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 14, "expected 14 stmts (7 each), got %d", n);
    /* First loop: stmts[1]=Ltop1, stmts[6]=Lend1 */
    /* Second loop: stmts[8]=Ltop2, stmts[13]=Lend2 */
    if (n >= 14) {
        ASSERT(stmts[1]->label && stmts[8]->label &&
               strcmp(stmts[1]->label, stmts[8]->label) != 0,
               "Ltop labels must be distinct: %s vs %s",
               stmts[1]->label ? stmts[1]->label : "(null)",
               stmts[8]->label ? stmts[8]->label : "(null)");
        ASSERT(stmts[6]->label && stmts[13]->label &&
               strcmp(stmts[6]->label, stmts[13]->label) != 0,
               "Lend labels must be distinct");
    }
    free(stmts);
}

/* =========================================================================
 * Test 8 — while inside for (nesting integrity)
 *   for (i = 0; LT(i, 3); i = i + 1) while (GT(x, 0)) x = x + -1;
 *   Outer for: init Ltop_for cond_for <inner-while-group> step :(Ltop_for) Lend_for
 *   Inner while: Ltop_w cond_w body :(Ltop_w) Lend_w
 *   Total: 1+1+1 + (1+1+1+1+1) + 1+1+1 = 10 stmts
 * ========================================================================= */
static void test_for_containing_while(void) {
    Program *prog = snocone_parse_program(
        "for (i = 0; LT(i, 3); i = i + 1) while (GT(x, 0)) x = x + -1;", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 11, "expected 11 stmts, got %d", n);
    /* Spot-check structure */
    if (n >= 1)  ASSERT(is_bare_expr(stmts[0]),   "stmt[0] for-init");
    if (n >= 2)  ASSERT(is_label_pad(stmts[1]),   "stmt[1] Ltop_for");
    if (n >= 3)  ASSERT(is_cond_fail(stmts[2]),   "stmt[2] cond_for:F");
    if (n >= 4)  ASSERT(is_label_pad(stmts[3]),   "stmt[3] Ltop_while");
    if (n >= 5)  ASSERT(is_cond_fail(stmts[4]),   "stmt[4] cond_while:F");
    if (n >= 6)  ASSERT(is_bare_expr(stmts[5]),   "stmt[5] while-body");
    if (n >= 7)  ASSERT(is_goto_uncond(stmts[6]), "stmt[6] :(Ltop_while)");
    if (n >= 8)  ASSERT(is_label_pad(stmts[7]),   "stmt[7] Lend_while");
    if (n >= 9)  ASSERT(is_bare_expr(stmts[8]),   "stmt[8] step");
    if (n >= 10) ASSERT(is_goto_uncond(stmts[9]), "stmt[9] :(Ltop_for)");
    if (n >= 11) ASSERT(is_label_pad(stmts[10]),  "stmt[10] Lend_for");
    free(stmts);
}

/* =========================================================================
 * Test 8b — correction: count includes Lend_for, total is 11
 * ========================================================================= */
static void test_for_containing_while_count(void) {
    Program *prog = snocone_parse_program(
        "for (i = 0; LT(i, 3); i = i + 1) while (GT(x, 0)) x = x + -1;", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    /* for: init(1) Ltop(1) cond(1) [while: Ltop(1) cond(1) body(1) goto(1) Lend(1)] step(1) goto(1) Lend(1) = 11 */
    ASSERT(n == 11, "expected 11 stmts (for+while nested), got %d", n);
    if (n >= 11) ASSERT(is_label_pad(stmts[10]), "stmt[10] Lend_for");
    free(stmts);
}

/* =========================================================================
 * Test 9 — do/until is a syntax error (removed per Lon directive #12)
 * ========================================================================= */
static void test_do_until_fnc_cond(void) {
    Program *prog = snocone_parse_program(
        "do { line = line 'x'; } until (GT(SIZE(line), 5));", "test");
    ASSERT(prog == NULL, "do/until must be a syntax error after removal");
}

/* =========================================================================
 * Test 10 — for inside if (matched/unmatched nesting)
 *   if (GT(x, 0)) for (i = 0; LT(i, x); i = i + 1) a = a + i;
 *   Expected: cond:F(Lend_if) ; init ; Ltop ; cond_for:F(Lend_for) ; body ; step ; :(Ltop) ; Lend_for ; Lend_if
 *   = 9 stmts
 * ========================================================================= */
static void test_for_inside_if(void) {
    Program *prog = snocone_parse_program(
        "if (GT(x, 0)) for (i = 0; LT(i, x); i = i + 1) a = a + i;", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 9, "expected 9 stmts, got %d", n);
    if (n >= 1) ASSERT(is_cond_fail(stmts[0]),   "stmt[0] if-cond:F(Lend_if)");
    if (n >= 2) ASSERT(is_bare_expr(stmts[1]),   "stmt[1] for-init");
    if (n >= 3) ASSERT(is_label_pad(stmts[2]),   "stmt[2] Ltop");
    if (n >= 4) ASSERT(is_cond_fail(stmts[3]),   "stmt[3] for-cond:F(Lend_for)");
    if (n >= 5) ASSERT(is_bare_expr(stmts[4]),   "stmt[4] for-body");
    if (n >= 6) ASSERT(is_bare_expr(stmts[5]),   "stmt[5] step");
    if (n >= 7) ASSERT(is_goto_uncond(stmts[6]), "stmt[6] :(Ltop)");
    if (n >= 8) ASSERT(is_label_pad(stmts[7]),   "stmt[7] Lend_for");
    if (n >= 9) ASSERT(is_label_pad(stmts[8]),   "stmt[8] Lend_if");
    free(stmts);
}

/* =========================================================================
 * Test 11 — do/while followed by if (splice integrity — post-stmts)
 *   do { x = 1; } while (LT(x, 5));
 *   if (GT(x, 0)) y = 1;
 *   Expected: Ltop ; x=1 ; cond:S(Ltop) ; Lend ; if-cond:F(Lend2) ; y=1 ; Lend2
 *   = 7 stmts
 * ========================================================================= */
static void test_do_while_then_if(void) {
    Program *prog = snocone_parse_program(
        "do { x = 1; } while (LT(x, 5));"
        "if (GT(x, 0)) y = 1;", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 7, "expected 7 stmts, got %d", n);
    if (n >= 4) ASSERT(is_label_pad(stmts[3]), "stmt[3] do-Lend");
    if (n >= 5) ASSERT(is_cond_fail(stmts[4]), "stmt[4] if-cond");
    if (n >= 7) ASSERT(is_label_pad(stmts[6]), "stmt[6] if-Lend");
    free(stmts);
}

/* =========================================================================
 * Test 12 — for: init/cond/step can be arbitrary expressions
 *   for (a = f(0); LT(a, n); a = a * 2) ;
 *   Parses without error; init is AST_ASSIGN (a = call), step is AST_ASSIGN (a = mul).
 * ========================================================================= */
static void test_for_exprs(void) {
    Program *prog = snocone_parse_program(
        "for (a = 0; LT(a, n); a = a * 2) ;", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 6, "expected 6 stmts, got %d", n);
    if (n >= 1) {
        /* init stmt: sc_append_stmt decomposes AST_ASSIGN — subject is the LHS (AST_VAR 'a'),
         * replacement is the RHS.  has_eq==1 distinguishes from a bare subject. */
        ASSERT(stmts[0]->has_eq,
               "init should be decomposed assignment (has_eq=1), has_eq=%d", stmts[0]->has_eq);
    }
    if (n >= 4) {
        /* step stmt: emitted by sc_finalize_for directly (not via sc_append_stmt),
         * so the AST_ASSIGN node is NOT decomposed — subject holds the full AST_ASSIGN expr. */
        ASSERT(stmts[3]->subject && stmts[3]->subject->kind == AST_ASSIGN,
               "step subject should be AST_ASSIGN (not decomposed), got kind=%d",
               stmts[3]->subject ? (int)stmts[3]->subject->kind : -1);
    }
    free(stmts);
}

/* =========================================================================
 * Test 13 — do/while Ltop/Lend labels are distinct from adjacent while
 *   while (GT(x, 0)) x = x + -1;
 *   do { y = 1; } while (LT(y, 5));
 *   All four synthetic labels (2 from while, 2 from do) must be distinct.
 * ========================================================================= */
static void test_distinct_labels_do_while(void) {
    Program *prog = snocone_parse_program(
        "while (GT(x, 0)) x = x + -1;"
        "do { y = 1; } while (LT(y, 5));", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    /* while: Ltop(1) cond(1) body(1) goto(1) Lend(1) = 5
     * do:   Ltop(1) y=1(1) cond:S(1) Lend(1) = 4
     * total = 9 */
    ASSERT(n == 9, "expected 9 stmts, got %d", n);
    if (n >= 9) {
        const char *l0 = stmts[0]->label;   /* while Ltop */
        const char *l4 = stmts[4]->label;   /* while Lend */
        const char *l5 = stmts[5]->label;   /* do Ltop */
        const char *l8 = stmts[8]->label;   /* do Lend */
        ASSERT(l0 && l4 && l5 && l8, "all label pads must have labels");
        ASSERT(strcmp(l0, l5) != 0, "while-Ltop != do-Ltop");
        ASSERT(strcmp(l4, l8) != 0, "while-Lend != do-Lend");
    }
    free(stmts);
}

/* =========================================================================
 * Test 14 — do/until is a syntax error (removed per Lon directive #12)
 * ========================================================================= */
static void test_do_until_failure_loops(void) {
    Program *prog = snocone_parse_program(
        "do { x = x + 1; } until (GT(x, 5));", "test");
    ASSERT(prog == NULL, "do/until must be a syntax error after removal");
}

/* =========================================================================
 * Test 15 — for: cond:F(Lend) and :(Ltop) cross-reference integrity
 *   The cond-stmt's onfailure must equal Lend's label.
 *   The goto-top's uncond must equal Ltop's label.
 * ========================================================================= */
static void test_for_label_integrity(void) {
    Program *prog = snocone_parse_program(
        "for (i = 0; LT(i, 10); i = i + 1) x = 1;", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 7, "expected 7 stmts, got %d", n);
    if (n >= 7) {
        const char *ltop = stmts[1]->label;
        const char *lend = stmts[6]->label;
        ASSERT(stmts[2]->go && stmts[2]->go->onfailure &&
               strcmp(stmts[2]->go->onfailure, lend) == 0,
               "cond:F must target Lend");
        ASSERT(stmts[5]->go && stmts[5]->go->uncond &&
               strcmp(stmts[5]->go->uncond, ltop) == 0,
               ":(Ltop) must target Ltop");
    }
    free(stmts);
}

/* ---- main ---- */
int main(void) {
    test_do_while_basic();
    test_do_until_basic();
    test_for_basic();
    test_for_block_body();
    test_do_while_block();
    test_for_empty_body();
    test_unique_labels();
    test_for_containing_while();
    test_for_containing_while_count();
    test_do_until_fnc_cond();
    test_for_inside_if();
    test_do_while_then_if();
    test_for_exprs();
    test_distinct_labels_do_while();
    test_do_until_failure_loops();
    test_for_label_integrity();

    printf("PASS=%d FAIL=%d TOTAL=%d\n", g_pass, g_fail, g_pass + g_fail);
    return g_fail ? 1 : 0;
}
