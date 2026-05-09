/*
 * test_snocone_parse_d.c — GOAL-SNOCONE-LANG-SPACE LS-4.d acceptance test
 *
 * The LS-4.d gate: postfix subscripting `a[i, j]` parses and lowers to
 * AST_IDX(subject, idx1, idx2, ...).  Mirrors the snobol4.y `expr15`
 * shape exactly (snobol4.y:183) — same priority, same n-ary IR,
 * same left-recursive chaining for `a[i][j]`.
 *
 *   a[i]              →  AST_IDX(AST_VAR(a), AST_VAR(i))
 *   a[i, j]           →  AST_IDX(AST_VAR(a), AST_VAR(i), AST_VAR(j))
 *   a[i][j]           →  AST_IDX(AST_IDX(AST_VAR(a), AST_VAR(i)), AST_VAR(j))
 *   a[]               →  AST_IDX(AST_VAR(a))                  // empty list arm
 *   T['key']          →  AST_IDX(AST_VAR(T), AST_QLIT(key))
 *   f(g(x))[i]        →  AST_IDX(AST_FNC(f, ...), AST_VAR(i))   // call-then-subscript
 *   a[1 + 2, 3 * 4]   →  AST_IDX(a, AST_ADD(1,2), AST_MUL(3,4)) // expressions in subscript
 *   a[i] += 1         →  AST_ASSIGN(a[i], AST_ADD(clone(a[i]), 1))
 *                                                          // compound-assign clones
 *
 * Goal-file gate per LS-4.d: "Parses `a[i, j]`."  This file verifies
 * that and the surrounding semantic shape — left-recursive chaining,
 * empty-list arm, expression children, integration with AST_FNC call
 * form, and clone-distinctness for compound-assigns whose LHS is a
 * subscript.
 *
 * Build:
 *   cc -Wall -o test_snocone_parse_d test_snocone_parse_d.c \
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

Program *snocone_parse_program(const char *src, const char *filename);

/* Mini IR printer */
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
            fprintf(out, "(\"%s\"", e->sval ? e->sval : "");
            for (int i = 0; i < e->nchildren; i++) {
                fputs(", ", out);
                dump_expr(e->children[i], out);
            }
            fputc(')', out);
            break;
        case AST_ILIT: fprintf(out, "(%lld)", e->ival); break;
        case AST_FLIT: fprintf(out, "(%g)",   e->dval); break;
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
        if (s->subject)     { fputs("    subject     = ", out); dump_expr(s->subject,     out); fputc('\n', out); }
        if (s->replacement) { fputs("    replacement = ", out); dump_expr(s->replacement, out); fputc('\n', out); }
    }
}

static int g_pass = 0, g_fail = 0;
static void check(const char *label, int cond, const char *fmt, ...) {
    if (cond) { printf("  PASS %s\n", label); g_pass++; }
    else {
        va_list ap; va_start(ap, fmt);
        printf("  FAIL %s: ", label); vprintf(fmt, ap); putchar('\n'); va_end(ap);
        g_fail++;
    }
}

static AST_t *parse_or_null(const char *src) {
    Program *prog = snocone_parse_program(src, "<test>");
    if (!prog || !prog->head || !prog->head->replacement) return NULL;
    return prog->head->replacement;
}

/* ============================================================ */
/* 1. The headline gate: a[i, j] — n-ary subscript               */
/* ============================================================ */

static void test_subscript_two_indices(void) {
    /* Headline gate: X = a[i, j]; → AST_IDX(a, i, j) */
    const char *src = "X = a[i, j];";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog) return;
    dump_program(prog, stdout);
    AST_t *r = prog->head ? prog->head->replacement : NULL;
    check("top is AST_IDX", r && r->kind == AST_IDX,
          "got kind=%d", r ? (int)r->kind : -1);
    if (!r || r->kind != AST_IDX) return;
    check("nchildren == 3 (subject + 2 indices)", r->nchildren == 3,
          "got %d", r->nchildren);
    if (r->nchildren != 3) return;
    check("child[0] is AST_VAR(a)",
          r->children[0]->kind == AST_VAR &&
              strcmp(r->children[0]->sval, "a") == 0,
          "got kind=%d sval=%s",
          (int)r->children[0]->kind,
          r->children[0]->sval ? r->children[0]->sval : "(nil)");
    check("child[1] is AST_VAR(i)",
          r->children[1]->kind == AST_VAR &&
              strcmp(r->children[1]->sval, "i") == 0,
          "got kind=%d", (int)r->children[1]->kind);
    check("child[2] is AST_VAR(j)",
          r->children[2]->kind == AST_VAR &&
              strcmp(r->children[2]->sval, "j") == 0,
          "got kind=%d", (int)r->children[2]->kind);
}

/* ============================================================ */
/* 2. Single index — the simple case                             */
/* ============================================================ */

static void test_subscript_single_index(void) {
    const char *src = "X = a[i];";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_IDX", r->kind == AST_IDX, "got kind=%d", (int)r->kind);
    check("nchildren == 2", r->nchildren == 2, "got %d", r->nchildren);
    if (r->nchildren < 2) return;
    check("child[0] is AST_VAR(a)",
          r->children[0]->kind == AST_VAR &&
              strcmp(r->children[0]->sval, "a") == 0,
          "got kind=%d", (int)r->children[0]->kind);
    check("child[1] is AST_VAR(i)",
          r->children[1]->kind == AST_VAR,
          "got kind=%d", (int)r->children[1]->kind);
}

/* ============================================================ */
/* 3. Empty subscript — a[]                                      */
/* ============================================================ */

static void test_subscript_empty(void) {
    /* Empty subscript: a[] uses the empty arm of `exprlist`, yielding
     * AST_IDX(a) with just the subject and no index children. */
    const char *src = "X = a[];";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_IDX", r->kind == AST_IDX, "got kind=%d", (int)r->kind);
    check("nchildren == 1 (just subject, no indices)",
          r->nchildren == 1, "got %d", r->nchildren);
    if (r->nchildren < 1) return;
    check("child[0] is AST_VAR(a)",
          r->children[0]->kind == AST_VAR &&
              strcmp(r->children[0]->sval, "a") == 0,
          "got kind=%d", (int)r->children[0]->kind);
}

/* ============================================================ */
/* 4. Chaining — a[i][j] left-recursive                          */
/* ============================================================ */

static void test_subscript_chained(void) {
    /* Left-recursive: a[i][j] parses as AST_IDX(AST_IDX(a, i), j). */
    const char *src = "X = a[i][j];";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_IDX", r->kind == AST_IDX, "got kind=%d", (int)r->kind);
    check("top nchildren == 2", r->nchildren == 2, "got %d", r->nchildren);
    if (r->nchildren < 2) return;
    /* child[0] is the inner AST_IDX(a, i) */
    check("child[0] is AST_IDX (inner)",
          r->children[0]->kind == AST_IDX,
          "got kind=%d", (int)r->children[0]->kind);
    if (r->children[0]->kind != AST_IDX) return;
    check("inner nchildren == 2", r->children[0]->nchildren == 2,
          "got %d", r->children[0]->nchildren);
    check("inner.child[0] is AST_VAR(a)",
          r->children[0]->children[0]->kind == AST_VAR &&
              strcmp(r->children[0]->children[0]->sval, "a") == 0,
          "got kind=%d", (int)r->children[0]->children[0]->kind);
    check("inner.child[1] is AST_VAR(i)",
          r->children[0]->children[1]->kind == AST_VAR,
          "got kind=%d", (int)r->children[0]->children[1]->kind);
    check("child[1] (outer index) is AST_VAR(j)",
          r->children[1]->kind == AST_VAR &&
              strcmp(r->children[1]->sval, "j") == 0,
          "got kind=%d", (int)r->children[1]->kind);
}

static void test_subscript_chained_three(void) {
    /* Three-deep chaining: a[i][j][k] →
     * AST_IDX(AST_IDX(AST_IDX(a, i), j), k). */
    const char *src = "X = a[i][j][k];";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_IDX", r->kind == AST_IDX, "got kind=%d", (int)r->kind);
    if (r->kind != AST_IDX) return;
    check("top.child[1] is AST_VAR(k)",
          r->children[1]->kind == AST_VAR &&
              strcmp(r->children[1]->sval, "k") == 0,
          "got kind=%d", (int)r->children[1]->kind);
    /* Walk down: top.child[0] should be AST_IDX(...,j) */
    AST_t *l1 = r->children[0];
    check("level-1 is AST_IDX", l1->kind == AST_IDX, "got %d", (int)l1->kind);
    if (l1->kind != AST_IDX) return;
    check("level-1.child[1] is AST_VAR(j)",
          l1->children[1]->kind == AST_VAR &&
              strcmp(l1->children[1]->sval, "j") == 0,
          "got kind=%d", (int)l1->children[1]->kind);
    /* level-2 should be AST_IDX(a, i) */
    AST_t *l2 = l1->children[0];
    check("level-2 is AST_IDX", l2->kind == AST_IDX, "got %d", (int)l2->kind);
    if (l2->kind != AST_IDX) return;
    check("level-2.child[0] is AST_VAR(a)",
          l2->children[0]->kind == AST_VAR &&
              strcmp(l2->children[0]->sval, "a") == 0,
          "got kind=%d", (int)l2->children[0]->kind);
    check("level-2.child[1] is AST_VAR(i)",
          l2->children[1]->kind == AST_VAR &&
              strcmp(l2->children[1]->sval, "i") == 0,
          "got kind=%d", (int)l2->children[1]->kind);
}

/* ============================================================ */
/* 5. Subscript with literal-string key — T['key']                */
/* ============================================================ */

static void test_subscript_string_key(void) {
    const char *src = "X = T['key'];";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_IDX", r->kind == AST_IDX, "got %d", (int)r->kind);
    if (r->nchildren < 2) return;
    check("child[0] is AST_VAR(T)",
          r->children[0]->kind == AST_VAR &&
              strcmp(r->children[0]->sval, "T") == 0,
          "got kind=%d", (int)r->children[0]->kind);
    check("child[1] is AST_QLIT(key)",
          r->children[1]->kind == AST_QLIT &&
              strcmp(r->children[1]->sval, "key") == 0,
          "got kind=%d sval=%s",
          (int)r->children[1]->kind,
          r->children[1]->sval ? r->children[1]->sval : "(nil)");
}

/* ============================================================ */
/* 6. Subscript with expression children — a[1 + 2, 3 * 4]       */
/* ============================================================ */

static void test_subscript_expr_children(void) {
    const char *src = "X = a[1 + 2, 3 * 4];";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_IDX", r->kind == AST_IDX, "got %d", (int)r->kind);
    check("nchildren == 3", r->nchildren == 3, "got %d", r->nchildren);
    if (r->nchildren != 3) return;
    check("child[1] is AST_ADD",
          r->children[1]->kind == AST_ADD,
          "got kind=%d", (int)r->children[1]->kind);
    check("child[2] is AST_MUL",
          r->children[2]->kind == AST_MUL,
          "got kind=%d", (int)r->children[2]->kind);
}

/* ============================================================ */
/* 7. Call-then-subscript — f(x)[i]                              */
/* ============================================================ */

static void test_call_then_subscript(void) {
    /* f(x)[i] should parse as AST_IDX(AST_FNC(f, x), i) — subscript binds
     * to the call result. */
    const char *src = "X = f(x)[i];";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_IDX", r->kind == AST_IDX, "got %d", (int)r->kind);
    if (r->nchildren < 2) return;
    check("child[0] is AST_FNC",
          r->children[0]->kind == AST_FNC &&
              strcmp(r->children[0]->sval, "f") == 0,
          "got kind=%d sval=%s",
          (int)r->children[0]->kind,
          r->children[0]->sval ? r->children[0]->sval : "(nil)");
    check("child[1] is AST_VAR(i)",
          r->children[1]->kind == AST_VAR,
          "got kind=%d", (int)r->children[1]->kind);
}

/* ============================================================ */
/* 8. Subscript-then-call — never via brackets, but consistent   */
/* ============================================================ */

static void test_paren_subscript(void) {
    /* (a + b)[i] — parens around expression, then subscript */
    const char *src = "X = (a + b)[i];";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_IDX", r->kind == AST_IDX, "got %d", (int)r->kind);
    if (r->nchildren < 2) return;
    check("child[0] is AST_ADD (the parenthesised expr)",
          r->children[0]->kind == AST_ADD,
          "got kind=%d", (int)r->children[0]->kind);
    check("child[1] is AST_VAR(i)",
          r->children[1]->kind == AST_VAR,
          "got kind=%d", (int)r->children[1]->kind);
}

/* ============================================================ */
/* 9. Compound-assign with subscript LHS — a[i] += 1             */
/* ============================================================ */

static int expr_deep_eq(const AST_t *a, const AST_t *b) {
    if (!a || !b) return a == b;
    if (a->kind != b->kind) return 0;
    if (a->ival != b->ival) return 0;
    if (a->dval != b->dval) return 0;
    int sa = a->sval ? 1 : 0, sb = b->sval ? 1 : 0;
    if (sa != sb) return 0;
    if (sa && strcmp(a->sval, b->sval) != 0) return 0;
    if (a->nchildren != b->nchildren) return 0;
    for (int i = 0; i < a->nchildren; i++)
        if (!expr_deep_eq(a->children[i], b->children[i])) return 0;
    return 1;
}

static int expr_share_any_node(const AST_t *a, const AST_t *b) {
    if (!a || !b) return 0;
    if (a == b) return 1;
    for (int i = 0; i < a->nchildren; i++)
        if (expr_share_any_node(a->children[i], b)) return 1;
    return 0;
}

static void test_compound_assign_subscript_lhs(void) {
    /* a[i] += 1 should lower to AST_ASSIGN(a[i], AST_ADD(clone(a[i]), 1)).
     * sc_append_stmt then unwraps the AST_ASSIGN: subject = a[i],
     * replacement = AST_ADD(clone(a[i]), 1).  We verify:
     *   1. has_eq is set
     *   2. subject is AST_IDX(a, i)
     *   3. replacement is AST_ADD whose child[0] is also AST_IDX(a, i)
     *   4. The two AST_IDX nodes are DIFFERENT pointers (clone, not alias)
     *   5. The deep tree shapes match
     */
    const char *src = "a[i] += 1;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    STMT_t *s = prog->head;
    check("has_eq is set", s->has_eq == 1, "got %d", s->has_eq);
    check("subject is AST_IDX", s->subject && s->subject->kind == AST_IDX,
          "got kind=%d", s->subject ? (int)s->subject->kind : -1);
    check("replacement is AST_ADD",
          s->replacement && s->replacement->kind == AST_ADD,
          "got kind=%d", s->replacement ? (int)s->replacement->kind : -1);
    if (!s->subject || !s->replacement ||
        s->subject->kind != AST_IDX || s->replacement->kind != AST_ADD ||
        s->replacement->nchildren < 2) return;
    AST_t *clone = s->replacement->children[0];
    check("RHS.child[0] is AST_IDX (clone of LHS)",
          clone->kind == AST_IDX, "got kind=%d", (int)clone->kind);
    check("LHS and clone are distinct pointers",
          s->subject != clone,
          "subject=%p clone=%p (aliasing — would double-free at cleanup)",
          (void*)s->subject, (void*)clone);
    check("LHS and clone are deeply equal in shape",
          expr_deep_eq(s->subject, clone),
          "deep shapes differ");
    check("LHS and clone share no nodes anywhere in tree",
          !expr_share_any_node(s->subject, clone),
          "node sharing detected (would corrupt cleanup)");
    check("RHS.child[1] is AST_ILIT(1)",
          s->replacement->children[1]->kind == AST_ILIT &&
              s->replacement->children[1]->ival == 1,
          "got kind=%d ival=%lld",
          (int)s->replacement->children[1]->kind,
          s->replacement->children[1]->ival);
}

/* ============================================================ */
/* 10. Subscript bound under concat — a[i] b                     */
/* ============================================================ */

static void test_subscript_in_concat(void) {
    /* a[i] b — subscript binds tighter than concat, so:
     *   AST_SEQ(AST_IDX(a, i), AST_VAR(b))
     */
    const char *src = "X = a[i] b;";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_SEQ", r->kind == AST_SEQ, "got %d", (int)r->kind);
    if (r->nchildren < 2) return;
    check("child[0] is AST_IDX",
          r->children[0]->kind == AST_IDX,
          "got kind=%d", (int)r->children[0]->kind);
    check("child[1] is AST_VAR(b)",
          r->children[1]->kind == AST_VAR &&
              strcmp(r->children[1]->sval, "b") == 0,
          "got kind=%d", (int)r->children[1]->kind);
}

/* ============================================================ */
/* 11. Subscript inside arithmetic — a[i] + b[j]                 */
/* ============================================================ */

static void test_subscript_in_arith(void) {
    /* a[i] + b[j] — both subscripts evaluate, then add. */
    const char *src = "X = a[i] + b[j];";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_ADD", r->kind == AST_ADD, "got %d", (int)r->kind);
    if (r->nchildren < 2) return;
    check("LHS is AST_IDX",
          r->children[0]->kind == AST_IDX,
          "got kind=%d", (int)r->children[0]->kind);
    check("RHS is AST_IDX",
          r->children[1]->kind == AST_IDX,
          "got kind=%d", (int)r->children[1]->kind);
}

/* ============================================================ */
/* 12. Subscript LHS of plain assignment — a[i] = 5              */
/* ============================================================ */

static void test_subscript_assign_lhs(void) {
    const char *src = "a[i] = 5;";
    printf("=== test: %s ===\n", src);
    Program *prog = snocone_parse_program(src, "<test>");
    check("parses", prog != NULL, "NULL");
    if (!prog || !prog->head) return;
    STMT_t *s = prog->head;
    check("has_eq is set", s->has_eq == 1, "got %d", s->has_eq);
    check("subject is AST_IDX",
          s->subject && s->subject->kind == AST_IDX,
          "got %d", s->subject ? (int)s->subject->kind : -1);
    check("replacement is AST_ILIT(5)",
          s->replacement && s->replacement->kind == AST_ILIT &&
              s->replacement->ival == 5,
          "got %d", s->replacement ? (int)s->replacement->kind : -1);
}

/* ============================================================ */
/* 13. Nested subscripts — a[b[c]]                               */
/* ============================================================ */

static void test_nested_subscripts(void) {
    /* a[b[c]] → AST_IDX(a, AST_IDX(b, c)) — the outer subscript has the
     * inner subscript as its index expression. */
    const char *src = "X = a[b[c]];";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_IDX", r->kind == AST_IDX, "got %d", (int)r->kind);
    if (r->nchildren < 2) return;
    check("child[0] is AST_VAR(a)",
          r->children[0]->kind == AST_VAR &&
              strcmp(r->children[0]->sval, "a") == 0,
          "got kind=%d", (int)r->children[0]->kind);
    check("child[1] is AST_IDX (inner b[c])",
          r->children[1]->kind == AST_IDX,
          "got kind=%d", (int)r->children[1]->kind);
    if (r->children[1]->kind != AST_IDX || r->children[1]->nchildren < 2) return;
    check("inner.child[0] is AST_VAR(b)",
          r->children[1]->children[0]->kind == AST_VAR &&
              strcmp(r->children[1]->children[0]->sval, "b") == 0,
          "got kind=%d", (int)r->children[1]->children[0]->kind);
    check("inner.child[1] is AST_VAR(c)",
          r->children[1]->children[1]->kind == AST_VAR &&
              strcmp(r->children[1]->children[1]->sval, "c") == 0,
          "got kind=%d", (int)r->children[1]->children[1]->kind);
}

/* ============================================================ */
/* 14. Subscript binds tighter than exponent — a[i] ^ 2          */
/* ============================================================ */

static void test_subscript_below_exponent(void) {
    /* a[i] ^ 2 → AST_POW(AST_IDX(a, i), 2).  Subscript at expr15
     * binds tighter than exponent at expr11. */
    const char *src = "X = a[i] ^ 2;";
    printf("=== test: %s ===\n", src);
    AST_t *r = parse_or_null(src);
    check("parses", r != NULL, "NULL");
    if (!r) return;
    check("top is AST_POW", r->kind == AST_POW, "got %d", (int)r->kind);
    if (r->nchildren < 2) return;
    check("LHS is AST_IDX",
          r->children[0]->kind == AST_IDX,
          "got kind=%d", (int)r->children[0]->kind);
    check("RHS is AST_ILIT(2)",
          r->children[1]->kind == AST_ILIT && r->children[1]->ival == 2,
          "got kind=%d ival=%lld",
          (int)r->children[1]->kind, r->children[1]->ival);
}

int main(void) {
    test_subscript_two_indices();        /* 1 — headline gate */
    test_subscript_single_index();       /* 2 */
    test_subscript_empty();              /* 3 */
    test_subscript_chained();            /* 4 */
    test_subscript_chained_three();      /* 5 */
    test_subscript_string_key();         /* 6 */
    test_subscript_expr_children();      /* 7 */
    test_call_then_subscript();          /* 8 */
    test_paren_subscript();              /* 9 */
    test_compound_assign_subscript_lhs();/* 10 — clone-distinctness */
    test_subscript_in_concat();          /* 11 */
    test_subscript_in_arith();           /* 12 */
    test_subscript_assign_lhs();         /* 13 */
    test_nested_subscripts();            /* 14 */
    test_subscript_below_exponent();     /* 15 */

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
