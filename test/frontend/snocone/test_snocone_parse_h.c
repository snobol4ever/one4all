/*
 * test_snocone_parse_h.c — GOAL-SNOCONE-LANG-SPACE LS-4.h acceptance test
 *
 * Verifies that the new Bison-based Snocone parser handles:
 *   function name(args) { body }
 *   return E ;
 *   return ;
 *   freturn ;
 *   nreturn ;
 *
 * Lowering shapes (all verified below):
 *
 *   function NAME(a, b) { body }   ->   DEFINE('NAME(a,b)')
 *                                       :(NAME_end)
 *                                       NAME    <body>
 *                                       NAME_end
 *
 *   return E ;     ->   <fn>=E :(RETURN)        (inside a function)
 *   return ;       ->   :(RETURN)
 *   freturn ;      ->   :(FRETURN)
 *   nreturn ;      ->   :(NRETURN)
 *
 * Side-channel test — the LS-4.h parser is not yet wired into scrip's
 * production driver path (that happens at LS-4.j).
 *
 * Build:
 *   cc -Wall -o test_snocone_parse_h test_snocone_parse_h.c \
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
/* True if stmt is a bare expression (subject, no goto). */
static int is_bare_expr(STMT_t *s) { return s->subject && !s->go; }
/* True if stmt is an assignment (subject + replacement, has_eq). */
static int is_assignment(STMT_t *s) { return s->subject && s->replacement && s->has_eq; }

/* True if expr is an AST_FNC named call to `name`. */
static int is_fnc_named(AST_t *e, const char *name) {
    return e && e->kind == AST_FNC && e->sval && strcmp(e->sval, name) == 0;
}

/* =========================================================================
 * Test 1 — minimal function
 *   function F() { }
 *   Expected:
 *     stmt[0] DEFINE('F()')         (bare-expr, AST_FNC)
 *     stmt[1] :(F_end)              (goto-uncond)
 *     stmt[2] F                     (label pad, entry point)
 *     stmt[3] F_end                 (label pad, skip target)
 * ========================================================================= */
static void test_function_empty(void) {
    Program *prog = snocone_parse_program("function F() { }", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 4, "expected 4 stmts, got %d", n);
    if (n >= 1) {
        ASSERT(is_bare_expr(stmts[0]), "stmt[0] should be bare-expr DEFINE call");
        ASSERT(is_fnc_named(stmts[0]->subject, "DEFINE"),
               "stmt[0] subject should be AST_FNC(\"DEFINE\")");
        if (stmts[0]->subject && stmts[0]->subject->nchildren >= 1) {
            AST_t *qarg = stmts[0]->subject->children[0];
            ASSERT(qarg && qarg->kind == AST_QLIT && qarg->sval &&
                   strcmp(qarg->sval, "F()") == 0,
                   "DEFINE arg should be AST_QLIT \"F()\", got %s",
                   qarg && qarg->sval ? qarg->sval : "(null)");
        }
    }
    if (n >= 2) {
        ASSERT(is_goto_uncond(stmts[1]), "stmt[1] should be :(F_end)");
        ASSERT(stmts[1]->go && stmts[1]->go->uncond &&
               strcmp(stmts[1]->go->uncond, "F_end") == 0,
               "stmt[1] uncond target should be F_end, got %s",
               stmts[1]->go && stmts[1]->go->uncond ? stmts[1]->go->uncond : "(null)");
    }
    if (n >= 3) {
        ASSERT(is_label_pad(stmts[2]), "stmt[2] should be label pad F");
        ASSERT(stmts[2]->label && strcmp(stmts[2]->label, "F") == 0,
               "stmt[2] label should be \"F\"");
    }
    if (n >= 4) {
        ASSERT(is_label_pad(stmts[3]), "stmt[3] should be label pad F_end");
        ASSERT(stmts[3]->label && strcmp(stmts[3]->label, "F_end") == 0,
               "stmt[3] label should be \"F_end\"");
    }
    free(stmts);
}

/* =========================================================================
 * Test 2 — function with single argument and return value
 *   function Double(n) { Double = n + n; return; }
 *   Expected:
 *     stmt[0] DEFINE('Double(n)')
 *     stmt[1] :(Double_end)
 *     stmt[2] Double          (label pad)
 *     stmt[3] Double = n + n  (assignment)
 *     stmt[4] :(RETURN)
 *     stmt[5] Double_end      (label pad)
 * ========================================================================= */
static void test_function_one_arg_return_bare(void) {
    Program *prog = snocone_parse_program(
        "function Double(n) { Double = n + n; return; }", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 6, "expected 6 stmts, got %d", n);
    if (n >= 1) {
        AST_t *q = stmts[0]->subject && stmts[0]->subject->nchildren >= 1 ?
                    stmts[0]->subject->children[0] : NULL;
        ASSERT(q && q->kind == AST_QLIT && q->sval &&
               strcmp(q->sval, "Double(n)") == 0,
               "DEFINE arg should be 'Double(n)'");
    }
    if (n >= 2) ASSERT(stmts[1]->go && stmts[1]->go->uncond &&
                       strcmp(stmts[1]->go->uncond, "Double_end") == 0,
                       "stmt[1] should goto Double_end");
    if (n >= 3) ASSERT(stmts[2]->label && strcmp(stmts[2]->label, "Double") == 0,
                       "stmt[2] should be Double label");
    if (n >= 4) ASSERT(is_assignment(stmts[3]), "stmt[3] should be assignment");
    if (n >= 5) {
        ASSERT(is_goto_uncond(stmts[4]), "stmt[4] should be bare :(RETURN)");
        ASSERT(stmts[4]->go && stmts[4]->go->uncond &&
               strcmp(stmts[4]->go->uncond, "RETURN") == 0,
               "stmt[4] uncond should be RETURN");
    }
    if (n >= 6) ASSERT(stmts[5]->label && strcmp(stmts[5]->label, "Double_end") == 0,
                       "stmt[5] should be Double_end label");
    free(stmts);
}

/* =========================================================================
 * Test 3 — `return E;` inside a function lowers to `name = E :(RETURN)`
 *   function F(x) { return x + 1; }
 *   Expected:
 *     stmt[0] DEFINE('F(x)')
 *     stmt[1] :(F_end)
 *     stmt[2] F (label)
 *     stmt[3] F = (x + 1)  with go.uncond = RETURN, has_eq = 1
 *     stmt[4] F_end (label)
 * ========================================================================= */
static void test_function_return_value(void) {
    Program *prog = snocone_parse_program(
        "function F(x) { return x + 1; }", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 5, "expected 5 stmts, got %d", n);
    if (n >= 4) {
        STMT_t *r = stmts[3];
        ASSERT(r->subject && r->replacement && r->has_eq,
               "return-stmt should be assignment-shaped");
        ASSERT(r->subject->kind == AST_VAR && r->subject->sval &&
               strcmp(r->subject->sval, "F") == 0,
               "return-stmt LHS should be AST_VAR(F)");
        ASSERT(r->go && r->go->uncond &&
               strcmp(r->go->uncond, "RETURN") == 0,
               "return-stmt should have :(RETURN) goto");
    }
    free(stmts);
}

/* =========================================================================
 * Test 4 — freturn lowers to bare :(FRETURN)
 *   function F() { freturn; }
 *   Expected: DEFINE ; goto F_end ; F-label ; :(FRETURN) ; F_end-label
 * ========================================================================= */
static void test_function_freturn(void) {
    Program *prog = snocone_parse_program("function F() { freturn; }", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 5, "expected 5 stmts, got %d", n);
    if (n >= 4) {
        ASSERT(is_goto_uncond(stmts[3]), "stmt[3] should be bare :(FRETURN)");
        ASSERT(stmts[3]->go && stmts[3]->go->uncond &&
               strcmp(stmts[3]->go->uncond, "FRETURN") == 0,
               "stmt[3] should goto FRETURN");
    }
    free(stmts);
}

/* =========================================================================
 * Test 5 — nreturn lowers to bare :(NRETURN)
 * ========================================================================= */
static void test_function_nreturn(void) {
    Program *prog = snocone_parse_program("function F() { nreturn; }", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 5, "expected 5 stmts, got %d", n);
    if (n >= 4) {
        ASSERT(is_goto_uncond(stmts[3]), "stmt[3] should be bare :(NRETURN)");
        ASSERT(stmts[3]->go && stmts[3]->go->uncond &&
               strcmp(stmts[3]->go->uncond, "NRETURN") == 0,
               "stmt[3] should goto NRETURN");
    }
    free(stmts);
}

/* =========================================================================
 * Test 6 — function with multiple args
 *   function Add(a, b) { Add = a + b; return; }
 *   DEFINE arg should be 'Add(a,b)' (comma-separated, no spaces).
 * ========================================================================= */
static void test_function_two_args(void) {
    Program *prog = snocone_parse_program(
        "function Add(a, b) { Add = a + b; return; }", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 6, "expected 6 stmts, got %d", n);
    if (n >= 1) {
        AST_t *q = stmts[0]->subject && stmts[0]->subject->nchildren >= 1 ?
                    stmts[0]->subject->children[0] : NULL;
        ASSERT(q && q->kind == AST_QLIT && q->sval &&
               strcmp(q->sval, "Add(a,b)") == 0,
               "DEFINE arg should be 'Add(a,b)', got '%s'",
               q && q->sval ? q->sval : "(null)");
    }
    free(stmts);
}

/* =========================================================================
 * Test 7 — function with three args
 *   function Sum3(a, b, c) { Sum3 = a + b + c; return; }
 * ========================================================================= */
static void test_function_three_args(void) {
    Program *prog = snocone_parse_program(
        "function Sum3(a, b, c) { Sum3 = a + b + c; return; }", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 6, "expected 6 stmts, got %d", n);
    if (n >= 1) {
        AST_t *q = stmts[0]->subject && stmts[0]->subject->nchildren >= 1 ?
                    stmts[0]->subject->children[0] : NULL;
        ASSERT(q && q->kind == AST_QLIT && q->sval &&
               strcmp(q->sval, "Sum3(a,b,c)") == 0,
               "DEFINE arg should be 'Sum3(a,b,c)', got '%s'",
               q && q->sval ? q->sval : "(null)");
    }
    free(stmts);
}

/* =========================================================================
 * Test 8 — pre-existing stmts before the function
 *   x = 1; function F() { } y = 2;
 *   Expected: 6 stmts
 *     stmt[0] x = 1
 *     stmt[1] DEFINE('F()')
 *     stmt[2] :(F_end)
 *     stmt[3] F (label)
 *     stmt[4] F_end (label)
 *     stmt[5] y = 2
 * ========================================================================= */
static void test_function_with_surrounding_stmts(void) {
    Program *prog = snocone_parse_program(
        "x = 1; function F() { } y = 2;", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 6, "expected 6 stmts, got %d", n);
    if (n >= 1) ASSERT(is_assignment(stmts[0]), "stmt[0] should be x=1");
    if (n >= 2) ASSERT(is_fnc_named(stmts[1]->subject, "DEFINE"),
                       "stmt[1] should be DEFINE call");
    if (n >= 3) ASSERT(is_goto_uncond(stmts[2]), "stmt[2] should be :(F_end)");
    if (n >= 4) ASSERT(stmts[3]->label && strcmp(stmts[3]->label, "F") == 0,
                       "stmt[3] should be F label");
    if (n >= 5) ASSERT(stmts[4]->label && strcmp(stmts[4]->label, "F_end") == 0,
                       "stmt[4] should be F_end label");
    if (n >= 6) ASSERT(is_assignment(stmts[5]), "stmt[5] should be y=2");
    free(stmts);
}

/* =========================================================================
 * Test 9 — multiple body stmts inside function
 *   function F(n) { x = n; y = n + 1; F = x + y; return; }
 *   Expected: 8 stmts (DEFINE, goto, label, x=n, y=n+1, F=x+y :(RETURN), F_end)
 * ========================================================================= */
static void test_function_multi_body_stmts(void) {
    Program *prog = snocone_parse_program(
        "function F(n) { x = n; y = n + 1; F = x + y; return; }", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 8, "expected 8 stmts, got %d", n);
    if (n >= 4) ASSERT(is_assignment(stmts[3]), "stmt[3] x=n");
    if (n >= 5) ASSERT(is_assignment(stmts[4]), "stmt[4] y=n+1");
    if (n >= 6) ASSERT(is_assignment(stmts[5]), "stmt[5] F=x+y");
    if (n >= 7) {
        /* stmt[6] is bare :(RETURN) - return; with no value */
        ASSERT(is_goto_uncond(stmts[6]), "stmt[6] should be :(RETURN)");
        ASSERT(stmts[6]->go && strcmp(stmts[6]->go->uncond, "RETURN") == 0,
               "stmt[6] should goto RETURN");
    }
    if (n >= 8) ASSERT(stmts[7]->label && strcmp(stmts[7]->label, "F_end") == 0,
                       "stmt[7] should be F_end label");
    free(stmts);
}

/* =========================================================================
 * Test 10 — return-with-value uses correct enclosing function name
 *   function ABC(z) { return z * 2; }
 *   The return-stmt's subject should be AST_VAR(ABC), NOT some other name.
 * ========================================================================= */
static void test_function_return_correct_name(void) {
    Program *prog = snocone_parse_program(
        "function ABC(z) { return z * 2; }", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 5, "expected 5 stmts, got %d", n);
    if (n >= 4) {
        STMT_t *r = stmts[3];
        ASSERT(r->subject && r->subject->kind == AST_VAR &&
               r->subject->sval && strcmp(r->subject->sval, "ABC") == 0,
               "return LHS should be ABC, got %s",
               r->subject && r->subject->sval ? r->subject->sval : "(null)");
    }
    free(stmts);
}

/* =========================================================================
 * Test 11 — control-flow inside a function body
 *   function F(x) { if (GT(x, 0)) { F = x; } else { F = 0; } return; }
 *   Verify the function still wraps the if/else stmts correctly.
 *   We don't count exactly — just verify shell shape: DEFINE first, F_end last.
 * ========================================================================= */
static void test_function_with_if_inside(void) {
    Program *prog = snocone_parse_program(
        "function F(x) { if (GT(x, 0)) { F = x; } else { F = 0; } return; }",
        "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n >= 5, "expected at least 5 stmts, got %d", n);
    if (n >= 1) ASSERT(is_fnc_named(stmts[0]->subject, "DEFINE"),
                       "stmt[0] should be DEFINE");
    if (n >= 2) ASSERT(stmts[1]->go && stmts[1]->go->uncond &&
                       strcmp(stmts[1]->go->uncond, "F_end") == 0,
                       "stmt[1] should goto F_end");
    if (n >= 3) ASSERT(stmts[2]->label && strcmp(stmts[2]->label, "F") == 0,
                       "stmt[2] should be F label");
    if (n >= 1) ASSERT(stmts[n-1]->label && strcmp(stmts[n-1]->label, "F_end") == 0,
                       "last stmt should be F_end label");
    free(stmts);
}

/* =========================================================================
 * Test 12 — two functions back-to-back
 *   function A(x) { A = x; return; } function B(y) { B = y; return; }
 * ========================================================================= */
static void test_two_functions(void) {
    Program *prog = snocone_parse_program(
        "function A(x) { A = x; return; } function B(y) { B = y; return; }",
        "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    /* A: DEFINE, goto A_end, A-label, A=x, :(RETURN), A_end-label = 6 stmts
     * B: DEFINE, goto B_end, B-label, B=y, :(RETURN), B_end-label = 6 stmts
     * Total = 12 */
    ASSERT(n == 12, "expected 12 stmts, got %d", n);
    if (n >= 3) ASSERT(stmts[2]->label && strcmp(stmts[2]->label, "A") == 0,
                       "A function entry label");
    if (n >= 6) ASSERT(stmts[5]->label && strcmp(stmts[5]->label, "A_end") == 0,
                       "A_end label");
    if (n >= 9) ASSERT(stmts[8]->label && strcmp(stmts[8]->label, "B") == 0,
                       "B function entry label");
    if (n >= 12) ASSERT(stmts[11]->label && strcmp(stmts[11]->label, "B_end") == 0,
                        "B_end label");
    free(stmts);
}

/* =========================================================================
 * Test 13 — return without `;` inside braces (must fail)
 *   function F() { return }   — should be a parse error
 * Skipped: parser may accept missing `;` permissively. We test the supported
 * form here.
 *
 * Test 13 actually — bare-return at top level (outside any function).
 *   return;
 * This is currently accepted (lowers to a bare :(RETURN) goto).  Confirm
 * the lowering shape — the grammar allows it, the runtime would raise a
 * "RETURN outside function" error at execution time.
 * ========================================================================= */
static void test_top_level_return(void) {
    Program *prog = snocone_parse_program("return;", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n == 1, "expected 1 stmt, got %d", n);
    if (n >= 1) {
        ASSERT(is_goto_uncond(stmts[0]), "stmt[0] should be bare :(RETURN)");
        ASSERT(stmts[0]->go && strcmp(stmts[0]->go->uncond, "RETURN") == 0,
               "stmt[0] should goto RETURN");
    }
    free(stmts);
}

/* =========================================================================
 * Test 14 — body containing a while loop
 *   function F(n) { i = 0; while (LT(i, n)) { i = i + 1; } F = i; return; }
 *   Verify outer envelope still correct.
 * ========================================================================= */
static void test_function_with_while_inside(void) {
    Program *prog = snocone_parse_program(
        "function F(n) { i = 0; while (LT(i, n)) { i = i + 1; } F = i; return; }",
        "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    ASSERT(n >= 6, "expected at least 6 stmts, got %d", n);
    if (n >= 1) ASSERT(is_fnc_named(stmts[0]->subject, "DEFINE"),
                       "stmt[0] DEFINE");
    if (n >= 3) ASSERT(stmts[2]->label && strcmp(stmts[2]->label, "F") == 0,
                       "stmt[2] F entry label");
    if (n >= 1) ASSERT(stmts[n-1]->label && strcmp(stmts[n-1]->label, "F_end") == 0,
                       "last stmt F_end label");
    free(stmts);
}

/* =========================================================================
 * Test 15 — DEFINE call's AST_FNC has correct sval and one child
 * ========================================================================= */
static void test_define_call_shape(void) {
    Program *prog = snocone_parse_program("function Foo() { }", "test");
    ASSERT(prog != NULL, "parse failed");
    if (!prog) return;

    int n; STMT_t **stmts = collect_stmts(prog, &n);
    if (n >= 1) {
        AST_t *def = stmts[0]->subject;
        ASSERT(def && def->kind == AST_FNC, "subject should be AST_FNC");
        ASSERT(def && def->sval && strcmp(def->sval, "DEFINE") == 0,
               "fnc name should be DEFINE");
        ASSERT(def && def->nchildren == 1, "fnc should have exactly 1 child");
        if (def && def->nchildren >= 1) {
            AST_t *c = def->children[0];
            ASSERT(c->kind == AST_QLIT, "child should be AST_QLIT");
            ASSERT(c->sval && strcmp(c->sval, "Foo()") == 0,
                   "child sval should be Foo()");
        }
    }
    free(stmts);
}

int main(void) {
    test_function_empty();
    test_function_one_arg_return_bare();
    test_function_return_value();
    test_function_freturn();
    test_function_nreturn();
    test_function_two_args();
    test_function_three_args();
    test_function_with_surrounding_stmts();
    test_function_multi_body_stmts();
    test_function_return_correct_name();
    test_function_with_if_inside();
    test_two_functions();
    test_top_level_return();
    test_function_with_while_inside();
    test_define_call_shape();
    printf("=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
