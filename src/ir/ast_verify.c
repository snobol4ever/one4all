/*
 * ast_verify.c — IR structural invariant checker
 *
 * Validates every node in an AST_t tree:
 *   1. kind is in range [0, AST_KIND_COUNT)
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
#include "scrip_cc.h"   /* → ir/ast.h (AST_e, AST_t, compat aliases, ast_e_name) */

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

static const KindSpec kind_spec[AST_KIND_COUNT] = {
    /* Literals */
    [AST_QLIT]       = EXACT_S(0),   /* sval = string text, no children */
    [AST_ILIT]       = EXACT(0),     /* ival = integer, no children */
    [AST_FLIT]       = EXACT(0),     /* dval = float, no children */
    [AST_CSET]       = EXACT_S(0),   /* sval = cset chars, no children */
    [AST_NUL]        = EXACT(0),     /* null/empty value, no children */

    /* References */
    [AST_VAR]        = ANY_S,        /* sval = name; children = (none or subscripts) */
    [AST_KEYWORD]         = EXACT_S(0),   /* sval = keyword name */
    [AST_INDIRECT]       = EXACT(1),     /* $expr — one child (the indirect expr) */
    [AST_DEFER]      = EXACT(1),     /* *expr — one child */

    /* Arithmetic — all binary except unaries */
    [AST_MNS]        = EXACT(1),
    [AST_PLS]        = EXACT(1),
    [AST_ADD]        = EXACT(2),
    [AST_SUB]        = EXACT(2),
    [AST_MUL]        = EXACT(2),
    [AST_DIV]        = EXACT(2),
    [AST_MOD]        = EXACT(2),
    [AST_POW]        = EXACT(2),

    /* Sequence / alternation — n-ary, at least 2 */
    [AST_SEQ]        = MIN(2),
    [AST_ALT]        = MIN(2),
    [AST_OPSYN]      = EXACT(2),

    /* Pattern primitives — leaves (no children) or 1 arg */
    [AST_ARB]        = EXACT(0),
    [AST_ARBNO]      = EXACT(1),     /* the sub-pattern */
    [AST_POS]        = EXACT(1),     /* POS(n) — one arg */
    [AST_RPOS]       = EXACT(1),
    [AST_ANY]        = EXACT(1),     /* ANY(S) */
    [AST_NOTANY]     = EXACT(1),
    [AST_SPAN]       = EXACT(1),
    [AST_BREAK]      = EXACT(1),
    [AST_BREAKX]     = EXACT(1),
    [AST_LEN]        = EXACT(1),
    [AST_TAB]        = EXACT(1),
    [AST_RTAB]       = EXACT(1),
    [AST_REM]        = EXACT(0),
    [AST_FAIL]       = EXACT(0),
    [AST_SUCCEED]    = EXACT(0),
    [AST_FENCE]      = ANY_KIND,     /* 0 = bare FENCE; 1 = FENCE(pat) */
    [AST_ABORT]      = EXACT(0),
    [AST_BAL]        = EXACT(0),

    /* Captures */
    [AST_CAPT_COND_ASGN]  = MIN_S(1),    /* sval = var name; child = sub-pattern */
    [AST_CAPT_IMMED_ASGN]   = MIN_S(1),
    [AST_CAPT_CURSOR]   = EXACT_S(0),  /* @var — sval = var name, no children */

    /* Call / access / assignment / scan / swap */
    [AST_FNC]        = ANY_S,        /* sval = function name; children = args */
    [AST_IDX]        = MIN(1),       /* children[0] = array/expr or name via sval */
    [AST_ASSIGN]     = EXACT(2),     /* lhs, rhs */
    [AST_SCAN]      = EXACT(2),     /* subject ? pattern */
    [AST_SWAP]       = EXACT(2),     /* lhs :=: rhs */

    /* Icon generators / constructors */
    [AST_SUSPEND]    = EXACT(1),
    [AST_TO]         = EXACT(2),     /* i to j */
    [AST_TO_BY]      = EXACT(3),     /* i to j by k */
    [AST_LIMIT]      = EXACT(2),     /* E \ N */
    [AST_ALTERNATE]     = MIN(2),
    [AST_ITERATE]       = EXACT(1),     /* !E */
    [AST_MAKELIST]   = ANY_KIND,     /* [] is valid */

    /* Prolog */
    [AST_UNIFY]      = EXACT(2),
    [AST_CLAUSE]     = MIN(1),       /* head + body goals */
    [AST_CHOICE]     = MIN(1),       /* at least one clause */
    [AST_CUT]        = EXACT(0),
    [AST_TRAIL_MARK]   = EXACT(0),
    [AST_TRAIL_UNWIND] = EXACT(0),
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

static void verify_node(const AST_t *e, const char *path,
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
    if ((int)e->kind < 0 || e->kind >= AST_KIND_COUNT) {
        char msg[64];
        snprintf(msg, sizeof msg, "invalid kind %d", (int)e->kind);
        violation(vs, path, msg);
        return;   /* can't check further without a valid kind */
    }

    const char *kname = ast_e_name[e->kind] ? ast_e_name[e->kind] : "?";
    const KindSpec *spec = &kind_spec[e->kind];

    /* 2. sval required? */
    if (spec->need_sval && !e->sval) {
        char msg[128];
        snprintf(msg, sizeof msg, "%s requires non-NULL sval", kname);
        violation(vs, path, msg);
    }

    /* 3. nchildren spec */
    if (spec->sk == SPEC_EXACT && e->nchildren != spec->n) {
        char msg[128];
        snprintf(msg, sizeof msg,
                 "%s: expected %d children, got %d", kname, spec->n, e->nchildren);
        violation(vs, path, msg);
    } else if (spec->sk == SPEC_MIN && e->nchildren < spec->n) {
        char msg[128];
        snprintf(msg, sizeof msg,
                 "%s: expected >= %d children, got %d", kname, spec->n, e->nchildren);
        violation(vs, path, msg);
    }

    /* 4. No NULL children */
    for (int i = 0; i < e->nchildren; i++) {
        snprintf(child_path, sizeof child_path, "%s[%d]", path, i);
        if (!e->children[i]) {
            violation(vs, child_path, "NULL child pointer");
        } else {
            verify_node(e->children[i], child_path, vs, depth + 1);
        }
    }
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

int ir_verify_node(const AST_t *e, const char *path, FILE *err) {
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
 * Unit test — compiled when IR_VERIFY_TEST is defined.
 * Build: gcc -I src -I src/frontend/snobol4 -DIR_VERIFY_TEST \
 *             src/ir/ast_verify.c -o /tmp/ir_verify_test
 * ---------------------------------------------------------------------- */
#ifdef IR_VERIFY_TEST

#include <stdlib.h>
#include <assert.h>

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
    int failures = 0;

    /* --- Test 1: valid AST_ASSIGN(AST_VAR, AST_ILIT) --- */
    {
        AST_t *a = mk(AST_ASSIGN);
        AST_t *v = mk(AST_VAR);  v->sval = "x";
        AST_t *n = mk(AST_ILIT); n->ival = 1;
        add_child(a, v);
        add_child(a, n);
        int errs = ir_verify_node(a, "test1", stderr);
        if (errs != 0) { fprintf(stderr, "FAIL test1: expected 0 violations, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test1: valid AST_ASSIGN\n");
    }

    /* --- Test 2: AST_VAR missing sval --- */
    {
        AST_t *v = mk(AST_VAR);  /* sval intentionally NULL */
        int errs = ir_verify_node(v, "test2", NULL);  /* suppress output */
        if (errs != 1) { fprintf(stderr, "FAIL test2: expected 1 violation, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test2: AST_VAR with NULL sval caught\n");
    }

    /* --- Test 3: AST_ADD with wrong child count --- */
    {
        AST_t *a = mk(AST_ADD);
        AST_t *n = mk(AST_ILIT);
        add_child(a, n);  /* only 1 child, need 2 */
        int errs = ir_verify_node(a, "test3", NULL);
        if (errs != 1) { fprintf(stderr, "FAIL test3: expected 1 violation, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test3: AST_ADD with 1 child caught\n");
    }

    /* --- Test 4: AST_SEQ with 2 valid children --- */
    {
        AST_t *s  = mk(AST_SEQ);
        AST_t *q1 = mk(AST_QLIT); q1->sval = "hello";
        AST_t *q2 = mk(AST_QLIT); q2->sval = "world";
        add_child(s, q1);
        add_child(s, q2);
        int errs = ir_verify_node(s, "test4", stderr);
        if (errs != 0) { fprintf(stderr, "FAIL test4: expected 0 violations, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test4: valid AST_SEQ\n");
    }

    /* --- Test 5: AST_QLIT missing sval --- */
    {
        AST_t *q = mk(AST_QLIT);  /* sval intentionally NULL */
        int errs = ir_verify_node(q, "test5", NULL);
        if (errs != 1) { fprintf(stderr, "FAIL test5: expected 1 violation, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test5: AST_QLIT with NULL sval caught\n");
    }

    /* --- Test 6: nested valid tree --- */
    {
        AST_t *assign = mk(AST_ASSIGN);
        AST_t *lhs    = mk(AST_VAR);  lhs->sval = "result";
        AST_t *add    = mk(AST_ADD);
        AST_t *one    = mk(AST_ILIT); one->ival = 1;
        AST_t *two    = mk(AST_ILIT); two->ival = 2;
        add_child(add, one);
        add_child(add, two);
        add_child(assign, lhs);
        add_child(assign, add);
        int errs = ir_verify_node(assign, "test6", stderr);
        if (errs != 0) { fprintf(stderr, "FAIL test6: expected 0 violations, got %d\n", errs); failures++; }
        else fprintf(stderr, "PASS test6: valid nested AST_ASSIGN\n");
    }

    fprintf(stderr, "\n%s — %d failure(s)\n",
            failures == 0 ? "ALL PASS" : "FAILURES PRESENT", failures);
    return failures ? 1 : 0;
}
#endif /* IR_VERIFY_TEST */
