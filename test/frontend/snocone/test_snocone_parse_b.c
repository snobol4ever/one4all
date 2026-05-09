/*
 * test_snocone_parse_b.c — GOAL-SNOCONE-LANG-SPACE LS-4.b acceptance test
 *
 * The LS-4.b gate: the 14 comparison/identity operators
 *   ==  !=  <  >  <=  >=                  (numeric:  EQ NE LT GT LE GE)
 *   :==:  :!=:  :<:  :>:  :<=:  :>=:      (lexical:  LEQ LNE LLT LGT LLE LGE)
 *   ::  :!:                               (identity: IDENT DIFFER)
 * each lower to an AST_FNC named call.  And the T_CALL call-form
 * lowers `EQ(2+2, 4)` to AST_FNC("EQ", AST_ADD(2,2), 4).  Plus the precedence
 * relation: comparisons sit BELOW arithmetic add/sub, so
 * `a + b == c + d` parses as `(a + b) == (c + d)`.
 *
 * Goal-file gate per LS-4.b: "Parses `OUTPUT = EQ(2+2, 4)`."  This file
 * verifies that and the surrounding semantic shape.
 *
 * Build:
 *   cc -Wall -o test_snocone_parse_b test_snocone_parse_b.c \
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
#include <stdarg.h>

#define IR_DEFINE_NAMES
#include "scrip_cc.h"

/* Public entry from snocone_parse.tab.c */
Program *snocone_parse_program(const char *src, const char *filename);

/* Mini IR printer — same format as parse_a so dumps are uniform. */
static void dump_expr(const AST_t *e, FILE *out) {
    if (!e) { fputs("<nil>", out); return; }
    fputs(ast_e_name[e->kind] ? ast_e_name[e->kind] : "?", out);
    switch (e->kind) {
        case AST_VAR:
        case AST_KEYWORD:
        case AST_QLIT:
            fprintf(out, "(\"%s\")", e->sval ? e->sval : "");
            break;
        case AST_FNC:
            /* function name AND children */
            fprintf(out, "(\"%s\"", e->sval ? e->sval : "");
            for (int i = 0; i < e->nchildren; i++) {
                fputs(", ", out);
                dump_expr(e->children[i], out);
            }
            fputc(')', out);
            break;
        case AST_ILIT:
            fprintf(out, "(%lld)", e->ival);
            break;
        case AST_FLIT:
            fprintf(out, "(%g)", e->dval);
            break;
        default:
            fputc('(', out);
            for (int i = 0; i < e->nchildren; i++) {
                if (i) fputs(", ", out);
                dump_expr(e->children[i], out);
            }
            fputc(')', out);
            break;
    }
}

static void dump_program(const Program *prog, FILE *out) {
    if (!prog) { fputs("(null Program)\n", out); return; }
    fprintf(out, "Program nstmts=%d\n", prog->nstmts);
    int i = 0;
    for (STMT_t *s = prog->head; s; s = s->next, i++) {
        fprintf(out, "  stmt[%d]: stno=%d has_eq=%d\n", i, s->stno, s->has_eq);
        if (s->subject) {
            fputs("    subject     = ", out);
            dump_expr(s->subject, out);
            fputc('\n', out);
        }
        if (s->replacement) {
            fputs("    replacement = ", out);
            dump_expr(s->replacement, out);
            fputc('\n', out);
        }
    }
}

/* ---- one tiny test runner ---- */
static int g_pass = 0, g_fail = 0;

static void check(const char *label, int cond, const char *fmt, ...) {
    if (cond) {
        printf("  PASS %s\n", label);
        g_pass++;
    } else {
        va_list ap; va_start(ap, fmt);
        printf("  FAIL %s: ", label);
        vprintf(fmt, ap);
        putchar('\n');
        va_end(ap);
        g_fail++;
    }
}

/* ---- shape checker: replacement is AST_FNC("FNAME", lhs_kind, rhs_kind) ---- */
static AST_t *parse_or_null(const char *src) {
    Program *prog = snocone_parse_program(src, "<test>");
    if (!prog || !prog->head || !prog->head->replacement) return NULL;
    return prog->head->replacement;
}

static void check_fnc_2arg(const char *src, const char *fname,
                            EKind expected_l, EKind expected_r) {
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    char label[256];
    snprintf(label, sizeof label, "parses (%s)", src);
    check(label, r != NULL, "NULL");
    if (!r) return;
    snprintf(label, sizeof label, "top is AST_FNC (%s)", fname);
    check(label, r->kind == AST_FNC, "got kind=%d", (int)r->kind);
    if (r->kind != AST_FNC) return;
    snprintf(label, sizeof label, "fname == \"%s\"", fname);
    check(label, r->sval && strcmp(r->sval, fname) == 0,
          "got '%s'", r->sval ? r->sval : "(nil)");
    snprintf(label, sizeof label, "nchildren == 2 (%s)", fname);
    check(label, r->nchildren == 2, "got %d", r->nchildren);
    if (r->nchildren < 2) return;
    snprintf(label, sizeof label, "left  kind == %d", (int)expected_l);
    check(label, r->children[0]->kind == expected_l,
          "got %d", (int)r->children[0]->kind);
    snprintf(label, sizeof label, "right kind == %d", (int)expected_r);
    check(label, r->children[1]->kind == expected_r,
          "got %d", (int)r->children[1]->kind);
}

/* ---- 1. Each of the 14 comparison/identity operators lowers correctly ---- */
static void test_numeric_comparisons(void) {
    check_fnc_2arg("X = A == B;",  "EQ",  AST_VAR, AST_VAR);
    check_fnc_2arg("X = A != B;",  "NE",  AST_VAR, AST_VAR);
    check_fnc_2arg("X = A < B;",   "LT",  AST_VAR, AST_VAR);
    check_fnc_2arg("X = A > B;",   "GT",  AST_VAR, AST_VAR);
    check_fnc_2arg("X = A <= B;",  "LE",  AST_VAR, AST_VAR);
    check_fnc_2arg("X = A >= B;",  "GE",  AST_VAR, AST_VAR);
}

static void test_lexical_comparisons(void) {
    check_fnc_2arg("X = A :==: B;", "LEQ", AST_VAR, AST_VAR);
    check_fnc_2arg("X = A :!=: B;", "LNE", AST_VAR, AST_VAR);
    check_fnc_2arg("X = A :<: B;",  "LLT", AST_VAR, AST_VAR);
    check_fnc_2arg("X = A :>: B;",  "LGT", AST_VAR, AST_VAR);
    check_fnc_2arg("X = A :<=: B;", "LLE", AST_VAR, AST_VAR);
    check_fnc_2arg("X = A :>=: B;", "LGE", AST_VAR, AST_VAR);
}

static void test_identity_comparisons(void) {
    check_fnc_2arg("X = A :: B;",  "IDENT",  AST_VAR, AST_VAR);
    check_fnc_2arg("X = A :!: B;", "DIFFER", AST_VAR, AST_VAR);
}

/* ---- 2. The headline gate from the goal file: OUTPUT = EQ(2+2, 4) ---- */
static void test_function_call_form(void) {
    const char *src = "OUTPUT = EQ(2 + 2, 4);";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_FNC", r->kind == AST_FNC, "got kind=%d", (int)r->kind);
    if (r->kind != AST_FNC) return;
    check("fname == EQ",
          r->sval && strcmp(r->sval, "EQ") == 0,
          "got '%s'", r->sval ? r->sval : "(nil)");
    check("nchildren == 2", r->nchildren == 2, "got %d", r->nchildren);
    if (r->nchildren < 2) return;
    AST_t *l = r->children[0];
    AST_t *rt = r->children[1];
    check("arg0 is AST_ADD", l->kind == AST_ADD, "got %d", (int)l->kind);
    if (l->kind == AST_ADD && l->nchildren == 2) {
        check("arg0.left  is AST_ILIT(2)",
              l->children[0]->kind == AST_ILIT && l->children[0]->ival == 2,
              "got kind=%d ival=%lld",
              (int)l->children[0]->kind, l->children[0]->ival);
        check("arg0.right is AST_ILIT(2)",
              l->children[1]->kind == AST_ILIT && l->children[1]->ival == 2,
              "got kind=%d ival=%lld",
              (int)l->children[1]->kind, l->children[1]->ival);
    }
    check("arg1 is AST_ILIT(4)",
          rt->kind == AST_ILIT && rt->ival == 4,
          "got kind=%d ival=%lld", (int)rt->kind, rt->ival);
}

/* ---- 3. Precedence: a + b == c + d  →  EQ(a+b, c+d) ---- */
static void test_comparison_below_arithmetic(void) {
    const char *src = "X = A + B == C + D;";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_FNC(EQ)",
          r->kind == AST_FNC && r->sval && strcmp(r->sval, "EQ") == 0,
          "got kind=%d sval=%s",
          (int)r->kind, r->sval ? r->sval : "(nil)");
    if (r->kind != AST_FNC || r->nchildren != 2) return;
    check("left  is AST_ADD (a+b grouped under EQ)",
          r->children[0]->kind == AST_ADD,
          "got %d", (int)r->children[0]->kind);
    check("right is AST_ADD (c+d grouped under EQ)",
          r->children[1]->kind == AST_ADD,
          "got %d", (int)r->children[1]->kind);
}

/* ---- 4. Function call with zero args: F() ---- */
static void test_zero_arg_call(void) {
    const char *src = "X = TIME();";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_FNC", r->kind == AST_FNC, "got kind=%d", (int)r->kind);
    check("fname == TIME",
          r->sval && strcmp(r->sval, "TIME") == 0,
          "got '%s'", r->sval ? r->sval : "(nil)");
    check("nchildren == 0", r->nchildren == 0, "got %d", r->nchildren);
}

/* ---- 5. Function call with three args: F(a, b, c) ---- */
static void test_three_arg_call(void) {
    const char *src = "X = SUBSTR(S, 1, 3);";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_FNC", r->kind == AST_FNC, "got kind=%d", (int)r->kind);
    check("fname == SUBSTR",
          r->sval && strcmp(r->sval, "SUBSTR") == 0,
          "got '%s'", r->sval ? r->sval : "(nil)");
    check("nchildren == 3", r->nchildren == 3, "got %d", r->nchildren);
    if (r->nchildren < 3) return;
    check("arg0 is AST_VAR(S)",
          r->children[0]->kind == AST_VAR &&
              strcmp(r->children[0]->sval, "S") == 0,
          "got kind=%d sval=%s",
          (int)r->children[0]->kind,
          r->children[0]->sval ? r->children[0]->sval : "(nil)");
    check("arg1 is AST_ILIT(1)",
          r->children[1]->kind == AST_ILIT && r->children[1]->ival == 1,
          "got kind=%d ival=%lld",
          (int)r->children[1]->kind, r->children[1]->ival);
    check("arg2 is AST_ILIT(3)",
          r->children[2]->kind == AST_ILIT && r->children[2]->ival == 3,
          "got kind=%d ival=%lld",
          (int)r->children[2]->kind, r->children[2]->ival);
}

/* ---- 6. Nested call: F(G(x)) ---- */
static void test_nested_call(void) {
    const char *src = "X = SIZE(TRIM(S));";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("outer is AST_FNC(SIZE)",
          r->kind == AST_FNC && r->sval &&
              strcmp(r->sval, "SIZE") == 0,
          "got kind=%d sval=%s",
          (int)r->kind, r->sval ? r->sval : "(nil)");
    if (r->kind != AST_FNC || r->nchildren != 1) return;
    AST_t *inner = r->children[0];
    check("inner is AST_FNC(TRIM)",
          inner->kind == AST_FNC && inner->sval &&
              strcmp(inner->sval, "TRIM") == 0,
          "got kind=%d sval=%s",
          (int)inner->kind, inner->sval ? inner->sval : "(nil)");
    if (inner->kind != AST_FNC || inner->nchildren != 1) return;
    check("inner arg0 is AST_VAR(S)",
          inner->children[0]->kind == AST_VAR &&
              strcmp(inner->children[0]->sval, "S") == 0,
          "got kind=%d sval=%s",
          (int)inner->children[0]->kind,
          inner->children[0]->sval ? inner->children[0]->sval : "(nil)");
}

/* ---- 7. Left-associative chaining of comparisons: a == b == c ---- */
static void test_comparison_chaining(void) {
    /* Andrew's bconv[] doesn't specify assoc for comparisons (fn=1 ops);
     * Bison left-recursion gives left-assoc, so a == b == c parses as
     * EQ(EQ(a,b), c).  This is unusual code but the shape must be defined. */
    const char *src = "X = A == B == C;";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    Program *prog = snocone_parse_program(src, "<test>");
    if (prog) dump_program(prog, stdout);
    check("top is AST_FNC(EQ)",
          r->kind == AST_FNC && r->sval && strcmp(r->sval, "EQ") == 0,
          "kind=%d sval=%s",
          (int)r->kind, r->sval ? r->sval : "(nil)");
    if (r->kind != AST_FNC || r->nchildren != 2) return;
    check("left-associative: outer.left is AST_FNC(EQ)",
          r->children[0]->kind == AST_FNC && r->children[0]->sval &&
              strcmp(r->children[0]->sval, "EQ") == 0,
          "got kind=%d sval=%s",
          (int)r->children[0]->kind,
          r->children[0]->sval ? r->children[0]->sval : "(nil)");
    check("outer.right is AST_VAR(C)",
          r->children[1]->kind == AST_VAR &&
              strcmp(r->children[1]->sval, "C") == 0,
          "got kind=%d sval=%s",
          (int)r->children[1]->kind,
          r->children[1]->sval ? r->children[1]->sval : "(nil)");
}

/* ---- 8. Mixed: GT(a, 0) inside an assignment (the bare-statement
       variant from the goal file's "succeed-and-side-effect" idiom is
       deferred to LS-4.f when control flow lands; here we just check
       the call-form parses).
 */
static void test_call_with_int_literal(void) {
    const char *src = "X = GT(A, 0);";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_FNC(GT)",
          r->kind == AST_FNC && r->sval && strcmp(r->sval, "GT") == 0,
          "kind=%d sval=%s",
          (int)r->kind, r->sval ? r->sval : "(nil)");
    if (r->kind != AST_FNC || r->nchildren != 2) return;
    check("arg0 is AST_VAR(A)",
          r->children[0]->kind == AST_VAR &&
              strcmp(r->children[0]->sval, "A") == 0,
          "got kind=%d", (int)r->children[0]->kind);
    check("arg1 is AST_ILIT(0)",
          r->children[1]->kind == AST_ILIT && r->children[1]->ival == 0,
          "got kind=%d ival=%lld",
          (int)r->children[1]->kind, r->children[1]->ival);
}

int main(void) {
    test_numeric_comparisons();
    test_lexical_comparisons();
    test_identity_comparisons();
    test_function_call_form();
    test_comparison_below_arithmetic();
    test_zero_arg_call();
    test_three_arg_call();
    test_nested_call();
    test_comparison_chaining();
    test_call_with_int_literal();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
