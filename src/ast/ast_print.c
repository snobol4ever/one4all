/*
 * ast_print.c — Unified IR pretty-printer
 *
 * Prints any AST_t node (and its subtree) in a readable S-expression form.
 * Used for debugging all frontends uniformly — one printer, all 59 node kinds.
 *
 * Public API:
 *   ir_print_node(e, f)          — print node + subtree, no trailing newline
 *   ir_print_node_nl(e, f)       — same + trailing newline
 *
 * Output format (S-expression, compact):
 *   Leaf:    (AST_QLIT "hello")   (AST_ILIT 42)   (AST_VAR x)   (AST_NUL)
 *   Unary:   (AST_MNS (AST_ILIT 1))
 *   N-ary:   (AST_SEQ (AST_QLIT "a") (AST_VAR x) (AST_QLIT "b"))
 *   Wide:    multi-line with 2-space indent per depth level when nchildren > 1
 *
 * Produced by: Claude Sonnet 4.6 (G-7 session, 2026-03-28)
 * Milestone: M-G1-IR-PRINT
 */

/*
 * Include scrip-cc.h — it defines EXPR_T_DEFINED then includes ir/ast.h,
 * giving us AST_e and AST_t.  IR_DEFINE_NAMES pulls in ast_e_name[].
 */
#define IR_DEFINE_NAMES
#include "scrip_cc.h"   /* → ir/ast.h (AST_e, AST_t, ast_e_name) */

/* -------------------------------------------------------------------------
 * ast_print.h forward declarations (inlined here — no separate .h needed
 * for a debug utility).
 * ---------------------------------------------------------------------- */

/* Maximum recursion depth before we truncate with "..." */
#define AST_PRINT_MAX_DEPTH 64

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/* Escape a string for printing: replace control chars, quote double-quotes */
static void print_escaped(const char *s, FILE *f) {
    if (!s) { fputs("(null)", f); return; }
    fputc('"', f);
    for (const char *p = s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n",  f); break;
        case '\r': fputs("\\r",  f); break;
        case '\t': fputs("\\t",  f); break;
        default:
            if ((unsigned char)*p < 0x20)
                fprintf(f, "\\x%02x", (unsigned char)*p);
            else
                fputc(*p, f);
        }
    }
    fputc('"', f);
}

static void print_indent(int depth, FILE *f) {
    for (int i = 0; i < depth * 2; i++) fputc(' ', f);
}

/* Core recursive printer */
static void print_node(const AST_t *e, FILE *f, int depth) {
    if (!e) { fputs("(null)", f); return; }
    if (depth > AST_PRINT_MAX_DEPTH) { fputs("(...)", f); return; }

    /* Node kind name */
    const char *kname = (e->kind >= 0 && e->kind < AST_KIND_COUNT)
                        ? ast_e_name[e->kind]
                        : "E_???";

    /* Leaf nodes — no children, just payload */
    switch (e->kind) {
    case AST_QLIT:
        fputc('(', f); fputs(kname, f); fputc(' ', f);
        print_escaped(e->sval, f); fputc(')', f);
        return;
    case AST_ILIT:
        fprintf(f, "(%s %lld)", kname, (long long)e->ival);
        return;
    case AST_FLIT:
        fprintf(f, "(%s %g)", kname, e->dval);
        return;
    case AST_CSET:
        fputc('(', f); fputs(kname, f); fputc(' ', f);
        print_escaped(e->sval, f); fputc(')', f);
        return;
    case AST_NUL:
        fprintf(f, "(%s)", kname);
        return;
    case AST_VAR:
    case AST_KEYWORD:
    case AST_FNC:
    case AST_IDX:
    case AST_CAPT_COND_ASGN:
    case AST_CAPT_IMMED_ASGN:
    case AST_CAPT_CURSOR:
        /* sval carries the name; children (if any) are args */
        if (e->nchildren == 0) {
            fputc('(', f); fputs(kname, f);
            if (e->sval) { fputc(' ', f); fputs(e->sval, f); }
            fputc(')', f);
            return;
        }
        break;
    case AST_ARB: case AST_REM: case AST_FAIL: case AST_SUCCEED:
    case AST_FENCE: case AST_ABORT: case AST_BAL:
        if (e->nchildren == 0) {
            fprintf(f, "(%s)", kname);
            return;
        }
        break;
    default:
        break;
    }

    /* General case: (KIND child1 child2 ...) */
    fputc('(', f);
    fputs(kname, f);

    /* Attach sval label when present and meaningful */
    if (e->sval && e->kind != AST_QLIT && e->kind != AST_CSET) {
        fputc(' ', f); fputs(e->sval, f);
    }

    if (e->nchildren == 0) {
        fputc(')', f);
        return;
    }

    if (e->nchildren == 1) {
        /* Inline single child */
        fputc(' ', f);
        print_node(e->children[0], f, depth + 1);
        fputc(')', f);
    } else {
        /* Multiple children — each on its own indented line */
        for (int i = 0; i < e->nchildren; i++) {
            fputc('\n', f);
            print_indent(depth + 1, f);
            print_node(e->children[i], f, depth + 1);
        }
        fputc('\n', f);
        print_indent(depth, f);
        fputc(')', f);
    }
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

void ir_print_node(const AST_t *e, FILE *f) {
    print_node(e, f, 0);
}

void ir_print_node_nl(const AST_t *e, FILE *f) {
    print_node(e, f, 0);
    fputc('\n', f);
}

/* -------------------------------------------------------------------------
 * Unit test — compiled when AST_PRINT_TEST is defined.
 * Build: gcc -I src -I src/frontend/snobol4 -DIR_PRINT_TEST \
 *             src/ast/ast_print.c -o /tmp/ir_print_test
 * ---------------------------------------------------------------------- */
#ifdef AST_PRINT_TEST

/* Minimal AST_t for test — mirrors scrip-cc.h fields we use */
#include <stdlib.h>

static AST_t *mk(AST_e k) {
    AST_t *e = calloc(1, sizeof *e);
    e->kind = k;
    return e;
}
static void add_child(AST_t *parent, AST_t *child) {
    parent->children = realloc(parent->children,
                               (size_t)(parent->nchildren + 1) * sizeof(AST_t *));
    parent->children[parent->nchildren++] = child;
}

int main(void) {
    /* (AST_SEQ (AST_QLIT "hello") (AST_VAR x) (AST_ILIT 42)) */
    AST_t *root = mk(AST_SEQ);
    AST_t *lit  = mk(AST_QLIT); lit->sval  = "hello";
    AST_t *var  = mk(AST_VAR);  var->sval  = "x";
    AST_t *num  = mk(AST_ILIT); num->ival  = 42;
    add_child(root, lit);
    add_child(root, var);
    add_child(root, num);

    /* (AST_ASSIGN (AST_VAR result) (AST_ADD (AST_ILIT 1) (AST_ILIT 2))) */
    AST_t *assign = mk(AST_ASSIGN);
    AST_t *lhs    = mk(AST_VAR);  lhs->sval = "result";
    AST_t *add    = mk(AST_ADD);
    AST_t *one    = mk(AST_ILIT); one->ival = 1;
    AST_t *two    = mk(AST_ILIT); two->ival = 2;
    add_child(add, one);
    add_child(add, two);
    add_child(assign, lhs);
    add_child(assign, add);

    /* (AST_FNC LENGTH (AST_VAR s)) */
    AST_t *fnc = mk(AST_FNC); fnc->sval = "LENGTH";
    AST_t *arg = mk(AST_VAR); arg->sval = "s";
    add_child(fnc, arg);

    /* Pattern: (AST_ALT (AST_QLIT "foo") (AST_SPAN "abc")) */
    AST_t *alt  = mk(AST_ALT);
    AST_t *foo  = mk(AST_QLIT); foo->sval  = "foo";
    AST_t *span = mk(AST_SPAN); span->sval = "abc";
    add_child(alt, foo);
    add_child(alt, span);

    fputs("=== ast_print unit test ===\n\n", stdout);

    fputs("1. AST_SEQ:\n", stdout);
    ir_print_node_nl(root, stdout);

    fputs("\n2. AST_ASSIGN:\n", stdout);
    ir_print_node_nl(assign, stdout);

    fputs("\n3. AST_FNC:\n", stdout);
    ir_print_node_nl(fnc, stdout);

    fputs("\n4. AST_ALT (pattern):\n", stdout);
    ir_print_node_nl(alt, stdout);

    fputs("\n5. AST_NUL leaf:\n", stdout);
    ir_print_node_nl(mk(AST_NUL), stdout);

    fputs("\n6. AST_FAIL leaf:\n", stdout);
    ir_print_node_nl(mk(AST_FAIL), stdout);

    fputs("\n=== PASS ===\n", stdout);
    return 0;
}
#endif /* AST_PRINT_TEST */
