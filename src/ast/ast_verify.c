/*
 * ast_verify.c — IR structural invariant checker
 *
 * Validates every node in an tree_t tree:
 *   1. kind is in range [0, TT_KIND_COUNT)
 *   2. nchildren satisfies the per-kind spec (min/max)
 *   3. No NULL child pointers where children are expected
 *   4. sval non-NULL for kinds that require a name/payload
 *
 * Called from driver in debug builds (-DDEBUG) before emit.
 * Prints a diagnostic and returns non-zero on any violation.
 *
 * Public API:
 *   ir_verify_node(e, path, errors_out)  — verify subtree rooted at e
 *   ir_verify_program(prog, errors_out)  — verify all stmts in a CODE_t
 *
 * Both return the number of violations found (0 = clean).
 *
 * Produced by: Claude Sonnet 4.6 (G-7 session, 2026-03-28)
 * Milestone: M-G1-IR-VERIFY
 */

#define IR_DEFINE_NAMES
#include "scrip_cc.h"   /* → ir/ast.h (tree_e, tree_t, compat aliases, tt_e_name) */

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Per-kind nchildren specification
 *
 * EXACT(n)  — must have exactly n children
 * MIN(n)    — must have at least n children
 * ANY       — any number of children (including 0) is valid
 * SVAL      — must have non-NULL sval
 *
 * Derived from GRAND_MASTER_REORG.md node table and emitter evidence.
 * ---------------------------------------------------------------------- */

typedef enum { SPEC_EXACT, SPEC_MIN, SPEC_ANY } SpecKind;

typedef struct {
    SpecKind sk;
    int      n;       /* exact count or minimum */
    int      need_sval;
} KindSpec;

#define EXACT(n)        { SPEC_EXACT, (n), 0 }
#define EXACT_S(n)      { SPEC_EXACT, (n), 1 }
#define MIN(n)          { SPEC_MIN,   (n), 0 }
#define MIN_S(n)        { SPEC_MIN,   (n), 1 }
#define ANY_KIND        { SPEC_ANY,   0,   0 }
#define ANY_S           { SPEC_ANY,   0,   1 }

static const KindSpec kind_spec[TT_KIND_COUNT] = {
    /* Literals */
    [TT_QLIT]       = EXACT_S(0),   /* sval = string text, no children */
    [TT_ILIT]       = EXACT(0),     /* ival = integer, no children */
    [TT_FLIT]       = EXACT(0),     /* dval = float, no children */
    [TT_CSET]       = EXACT_S(0),   /* sval = cset chars, no children */
    [TT_NUL]        = EXACT(0),     /* null/empty value, no children */

    /* References */
    [TT_VAR]        = ANY_S,        /* sval = name; children = (none or subscripts) */
    [TT_KEYWORD]         = EXACT_S(0),   /* sval = keyword name */
    [TT_INDIRECT]       = EXACT(1),     /* $expr — one child (the indirect expr) */
    [TT_DEFER]      = EXACT(1),     /* *expr — one child */

    /* Arithmetic — all binary except unaries */
    [TT_MNS]        = EXACT(1),
    [TT_PLS]        = EXACT(1),
    [TT_ADD]        = EXACT(2),
    [TT_SUB]        = EXACT(2),
    [TT_MUL]        = EXACT(2),
    [TT_DIV]        = EXACT(2),
    [TT_MOD]        = EXACT(2),
    [TT_POW]        = EXACT(2),

    /* Sequence / alternation — n-ary, at least 2 */
    [TT_SEQ]        = MIN(2),
    [TT_ALT]        = MIN(2),
    [TT_OPSYN]      = EXACT(2),

    /* Pattern primitives — leaves (no children) or 1 arg */
    [TT_ARB]        = EXACT(0),
    [TT_ARBNO]      = EXACT(1),     /* the sub-pattern */
    [TT_POS]        = EXACT(1),     /* POS(n) — one arg */
    [TT_RPOS]       = EXACT(1),
    [TT_ANY]        = EXACT(1),     /* ANY(S) */
    [TT_NOTANY]     = EXACT(1),
    [TT_SPAN]       = EXACT(1),
    [TT_BREAK]      = EXACT(1),
    [TT_BREAKX]     = EXACT(1),
    [TT_LEN]        = EXACT(1),
    [TT_TAB]        = EXACT(1),
    [TT_RTAB]       = EXACT(1),
    [TT_REM]        = EXACT(0),
    [TT_FAIL]       = EXACT(0),
    [TT_SUCCEED]    = EXACT(0),
    [TT_FENCE]      = ANY_KIND,     /* 0 = bare FENCE; 1 = FENCE(pat) */
    [TT_ABORT]      = EXACT(0),
    [TT_BAL]        = EXACT(0),

    /* Captures */
    [TT_CAPT_COND_ASGN]  = MIN_S(1),    /* sval = var name; child = sub-pattern */
    [TT_CAPT_IMMED_ASGN]   = MIN_S(1),
    [TT_CAPT_CURSOR]   = EXACT_S(0),  /* @var — sval = var name, no children */

    /* Call / access / assignment / scan / swap */
    [TT_FNC]        = ANY_S,        /* sval = function name; children = args */
    [TT_IDX]        = MIN(1),       /* children[0] = array/expr or name via sval */
    [TT_ASSIGN]     = EXACT(2),     /* lhs, rhs */
    [TT_SCAN]      = EXACT(2),     /* subject ? pattern */
    [TT_SWAP]       = EXACT(2),     /* lhs :=: rhs */

    /* Icon generators / constructors */
    [TT_SUSPEND]    = EXACT(1),
    [TT_TO]         = EXACT(2),     /* i to j */
    [TT_TO_BY]      = EXACT(3),     /* i to j by k */
    [TT_LIMIT]      = EXACT(2),     /* E \ N */
    [TT_ALTERNATE]     = MIN(2),
    [TT_ITERATE]       = EXACT(1),     /* !E */
    [TT_MAKELIST]   = ANY_KIND,     /* [] is valid */

    /* Prolog */
    [TT_UNIFY]      = EXACT(2),
    [TT_CLAUSE]     = MIN(1),       /* head + body goals */
    [TT_CHOICE]     = MIN(1),       /* at least one clause */
    [TT_CUT]        = EXACT(0),
    [TT_TRAIL_MARK]   = EXACT(0),
    [TT_TRAIL_UNWIND] = EXACT(0),
};

/* -------------------------------------------------------------------------
 * Violation reporter
 * ---------------------------------------------------------------------- */

typedef struct {
    int   count;
    FILE *f;
} VerifyState;

static void violation(VerifyState *vs, const char *path, const char *msg) {
    if (vs->f)
        fprintf(vs->f, "ast_verify: %s: %s\n", path, msg);
    vs->count++;
}

/* -------------------------------------------------------------------------
 * Core recursive checker
 * ---------------------------------------------------------------------- */

static void verify_node(const tree_t *e, const char *path,
                        VerifyState *vs, int depth) {
    char child_path[512];

    if (!e) {
        violation(vs, path, "NULL node pointer");
        return;
    }
    if (depth > 256) {
        violation(vs, path, "tree depth > 256 (cycle or runaway tree)");
        return;
    }

    /* 1. kind in range */
    if ((int)e->t < 0 || e->t >= TT_KIND_COUNT) {
        char msg[64];
        snprintf(msg, sizeof msg, "invalid kind %d", (int)e->t);
        violation(vs, path, msg);
        return;   /* can't check further without a valid kind */
    }

    const char *kname = tt_e_name[e->t] ? tt_e_name[e->t] : "?";
    const KindSpec *spec = &kind_spec[e->t];

    /* 2. sval required? */
    if (spec->need_sval && !e->v.sval) {
        char msg[128];
        snprintf(msg, sizeof msg, "%s requires non-NULL sval", kname);
        violation(vs, path, msg);
    }

    /* 3. nchildren spec */
    if (spec->sk == SPEC_EXACT && e->n != spec->n) {
        char msg[128];
        snprintf(msg, sizeof msg,
                 "%s: expected %d children, got %d", kname, spec->n, e->n);
        violation(vs, path, msg);
    } else if (spec->sk == SPEC_MIN && e->n < spec->n) {
        char msg[128];
        snprintf(msg, sizeof msg,
                 "%s: expected >= %d children, got %d", kname, spec->n, e->n);
        violation(vs, path, msg);
    }

    /* 4. No NULL children */
    for (int i = 0; i < e->n; i++) {
        snprintf(child_path, sizeof child_path, "%s[%d]", path, i);
        if (!e->c[i]) {
            violation(vs, child_path, "NULL child pointer");
        } else {
            verify_node(e->c[i], child_path, vs, depth + 1);
        }
    }
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

int ir_verify_node(const tree_t *e, const char *path, FILE *err) {
    VerifyState vs = { 0, err ? err : stderr };
    verify_node(e, path ? path : "root", &vs, 0);
    return vs.count;
}

int ir_verify_program(const CODE_t *prog, FILE *err) {
    if (!prog) {
        if (err) fprintf(err, "ast_verify: NULL CODE_t pointer\n");
        return 1;
    }
    VerifyState vs = { 0, err ? err : stderr };
    int stmt_idx = 0;
    for (const STMT_t *s = prog->head; s; s = s->next, stmt_idx++) {
        char path[64];
        snprintf(path, sizeof path, "stmt[%d].subject", stmt_idx);
        if (s->subject)
            verify_node(s->subject, path, &vs, 0);

        snprintf(path, sizeof path, "stmt[%d].pattern", stmt_idx);
        if (s->pattern)
            verify_node(s->pattern, path, &vs, 0);

        snprintf(path, sizeof path, "stmt[%d].replacement", stmt_idx);
        if (s->replacement)
            verify_node(s->replacement, path, &vs, 0);
    }
    return vs.count;
}

/* -------------------------------------------------------------------------
 * Unit test — compiled when AST_VERIFY_TEST is defined.
 * Build: gcc -I src -I src/frontend/snobol4 -DIR_VERIFY_TEST \
 *             src/ast/ast_verify.c -o /tmp/ir_verify_test
 * ---------------------------------------------------------------------- */
#ifdef AST_VERIFY_TEST

#include <stdlib.h>
#include <assert.h>

static tree_t *mk(tree_e k) {
    tree_t *e = calloc(1, sizeof *e);
    e->t = k;
    return e;
}
static void add_child(tree_t *parent, tree_t *child) {
    parent->c = realloc(parent->c,
                               (size_t)(parent->n + 1) * sizeof(tree_t *));
    parent->c[parent->n++] = child;
}

int main(void) {
    int failures = 0;

    /* --- Test 1: valid TT_ASSIGN(TT_VAR, TT_ILIT) --- */
    {
        tree_t *a = mk(TT_ASSIGN);
        tree_t *v = mk(TT_VAR);  v->v.sval = "x";
        tree_t *n = mk(TT_ILIT); n->v.ival = 1;
        add_child(a, v);
        add_child(a, n);
        int errs = ir_verify_node(a, "test1", stderr);
        if (errs != 0) { fprintf(stderr, "FAIL test1: expected 0 violations, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test1: valid TT_ASSIGN\n");
    }

    /* --- Test 2: TT_VAR missing sval --- */
    {
        tree_t *v = mk(TT_VAR);  /* sval intentionally NULL */
        int errs = ir_verify_node(v, "test2", NULL);  /* suppress output */
        if (errs != 1) { fprintf(stderr, "FAIL test2: expected 1 violation, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test2: TT_VAR with NULL sval caught\n");
    }

    /* --- Test 3: TT_ADD with wrong child count --- */
    {
        tree_t *a = mk(TT_ADD);
        tree_t *n = mk(TT_ILIT);
        add_child(a, n);  /* only 1 child, need 2 */
        int errs = ir_verify_node(a, "test3", NULL);
        if (errs != 1) { fprintf(stderr, "FAIL test3: expected 1 violation, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test3: TT_ADD with 1 child caught\n");
    }

    /* --- Test 4: TT_SEQ with 2 valid children --- */
    {
        tree_t *s  = mk(TT_SEQ);
        tree_t *q1 = mk(TT_QLIT); q1->v.sval = "hello";
        tree_t *q2 = mk(TT_QLIT); q2->v.sval = "world";
        add_child(s, q1);
        add_child(s, q2);
        int errs = ir_verify_node(s, "test4", stderr);
        if (errs != 0) { fprintf(stderr, "FAIL test4: expected 0 violations, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test4: valid TT_SEQ\n");
    }

    /* --- Test 5: TT_QLIT missing sval --- */
    {
        tree_t *q = mk(TT_QLIT);  /* sval intentionally NULL */
        int errs = ir_verify_node(q, "test5", NULL);
        if (errs != 1) { fprintf(stderr, "FAIL test5: expected 1 violation, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test5: TT_QLIT with NULL sval caught\n");
    }

    /* --- Test 6: nested valid tree --- */
    {
        tree_t *assign = mk(TT_ASSIGN);
        tree_t *lhs    = mk(TT_VAR);  lhs->v.sval = "result";
        tree_t *add    = mk(TT_ADD);
        tree_t *one    = mk(TT_ILIT); one->v.ival = 1;
        tree_t *two    = mk(TT_ILIT); two->v.ival = 2;
        add_child(add, one);
        add_child(add, two);
        add_child(assign, lhs);
        add_child(assign, add);
        int errs = ir_verify_node(assign, "test6", stderr);
        if (errs != 0) { fprintf(stderr, "FAIL test6: expected 0 violations, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test6: valid nested TT_ASSIGN\n");
    }

    fprintf(stderr, "\n%s — %d failure(s)\n",
            failures == 0 ? "ALL PASS" : "FAILURES PRESENT", failures);
    return failures ? 1 : 0;
}
#endif /* AST_VERIFY_TEST */
