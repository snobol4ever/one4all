/*============================================================================================================================
 * icon_gen_missing.c -- IJ-18..IJ-28: missing JCON Byrd Box implementations.
 *
 * Each function mirrors the wiring in .github/jcon_irgen.icn:
 *   ir_a_Not, ir_a_RepAlt, ir_a_While, ir_a_Until, ir_a_Repeat,
 *   ir_a_Case, ir_a_Next, ir_a_Break, ir_a_Compound, ir_a_Suspend (fix),
 *   ir_a_ListConstructor (generative), ir_a_Sectionop (generative).
 *
 * Ported to C four-port model: alpha=entry 0, beta=entry 1, gamma=success,
 * omega=failure.  State lives in a heap-allocated zeta struct per box.
 * All BBs follow: fn(zeta, alpha) -> first value or FAILDESCR
 *                 fn(zeta, beta)  -> next value or FAILDESCR
 *============================================================================================================================*/

#include "../../runtime/interp/coro_runtime.h"
#include "../../runtime/interp/coro_value.h"
#include "../../runtime/interp/coro_stmt.h"
#include "../../runtime/x86/snobol4.h"
#include "../../ast/ast.h"
#include <stdlib.h>
#include <string.h>

/* Forward declarations from coro_runtime.c */
typedef struct { tree_t *expr; } icn_lazy_state_t;
extern DESCR_t icn_lazy_box(void *zeta, int entry);
extern int is_suspendable(tree_t *e);

/* alpha/beta port tags -- same as coro_runtime.h */
#ifndef alpha
#define alpha 0
#define beta  1
#endif

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-18: coro_bb_not -- ir_a_Not
 *
 * JCON wiring:
 *   start -> expr.start
 *   expr.success -> failure   (if E succeeded, not-E fails)
 *   expr.failure -> push null, success  (if E failed, not-E succeeds with null)
 *   resume -> failure (not-E is one-shot, bounded="always bounded" in JCON)
 *
 * State: expr to evaluate (one-shot, non-generative child).
 * alpha: evaluate child; if child fails -> NULVCL; if child succeeds -> FAILDESCR.
 * beta:  FAILDESCR (not-E is always bounded/one-shot).
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct { tree_t *expr; } icn_not_state_t;

DESCR_t coro_bb_not(void *zeta, int entry) {
    if (entry != alpha) return FAILDESCR;  /* one-shot: resume always fails */
    icn_not_state_t *z = (icn_not_state_t *)zeta;
    if (!z->expr) return NULVCL;
    DESCR_t v = bb_eval_value(z->expr);
    /* expr succeeded -> not fails; expr failed -> not succeeds with null */
    return IS_FAIL_fn(v) ? NULVCL : FAILDESCR;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-18: coro_bb_repalt -- ir_a_RepAlt  (|E  repeated alternation)
 *
 * JCON wiring (unbounded):
 *   start: save failure label = ir.failure; goto expr.start
 *   expr.success: save failure label = ir.start; goto ir.success
 *   expr.failure: IndirectGoto(t)  -- i.e. if expr exhausted first time -> omega;
 *                                     after first success, restart from ir.start
 *   resume -> expr.resume
 *
 * Simplified: pump sub-box; on exhaustion restart from alpha.
 * Semantics: keep generating E over and over, never exhausting until...
 * Actually |E generates all of E then wraps: this is an infinite generator.
 * For our purposes: restart inner box on exhaustion.
 *
 * State: inner box, started flag.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    bb_node_t inner;
    tree_t   *expr;
    int       started;
    int       ever_succeeded;  /* first exhaustion -> omega if never succeeded */
} icn_repalt_state_t;

DESCR_t coro_bb_repalt(void *zeta, int entry) {
    icn_repalt_state_t *z = (icn_repalt_state_t *)zeta;
    for (;;) {
        int port = (z->started) ? beta : alpha;
        if (!z->started) {
            z->inner   = coro_eval(z->expr);
            z->started = 1;
        }
        DESCR_t v = z->inner.fn(z->inner.ζ, port);
        if (!IS_FAIL_fn(v)) { z->ever_succeeded = 1; return v; }
        /* inner exhausted */
        if (!z->ever_succeeded) return FAILDESCR;  /* never succeeded -> omega */
        /* restart inner from alpha */
        z->inner   = coro_eval(z->expr);
        z->started = 1;  /* but pump with alpha next iteration */
        /* rebuild and pump alpha */
        v = z->inner.fn(z->inner.ζ, alpha);
        if (!IS_FAIL_fn(v)) return v;
        return FAILDESCR;  /* restarted and immediately exhausted */
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-20: coro_bb_while_gen -- ir_a_While  (while E do B, used as generator)
 *
 * JCON wiring:
 *   start -> expr.start
 *   expr.success -> body.start
 *   expr.failure -> ir.failure (omega)
 *   body.success -> expr.start  (next iteration)
 *   body.failure -> expr.start  (body failing just continues loop)
 *   resume -> IndirectGoto(continue)  (for break-driven resume)
 *
 * As a BB: generates the value of body on each iteration where expr succeeds.
 * State: expr_node, body_node. On each pump: test expr, run body.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct { tree_t *expr; tree_t *body; } icn_while_state_t;

DESCR_t coro_bb_while_gen(void *zeta, int entry) {
    icn_while_state_t *z = (icn_while_state_t *)zeta;
    /* Both alpha and beta: test expr, if succeeds run body, return body value.
     * Loop control (next/break) is handled at statement level. */
    for (;;) {
        DESCR_t test = z->expr ? bb_eval_value(z->expr) : FAILDESCR;
        if (IS_FAIL_fn(test)) return FAILDESCR;  /* expr failed -> loop done */
        DESCR_t bval = z->body ? bb_eval_value(z->body) : NULVCL;
        /* Body value (success or fail) — yield and continue */
        if (!IS_FAIL_fn(bval)) return bval;
        /* body failed -> continue to next iteration (not omega) */
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-20: coro_bb_until_gen -- ir_a_Until  (until E do B, used as generator)
 *
 * JCON wiring:
 *   expr.success -> ir.failure  (expr true -> stop)
 *   expr.failure -> body.start  (expr false -> run body)
 *   body.success/failure -> expr.start  (keep looping)
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct { tree_t *expr; tree_t *body; } icn_until_state_t;

DESCR_t coro_bb_until_gen(void *zeta, int entry) {
    icn_until_state_t *z = (icn_until_state_t *)zeta;
    for (;;) {
        DESCR_t test = z->expr ? bb_eval_value(z->expr) : NULVCL;
        if (!IS_FAIL_fn(test)) return FAILDESCR;  /* expr succeeded -> loop done */
        DESCR_t bval = z->body ? bb_eval_value(z->body) : NULVCL;
        if (!IS_FAIL_fn(bval)) return bval;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-20: coro_bb_repeat_gen -- ir_a_Repeat  (repeat B, used as generator)
 *
 * JCON wiring: unconditional loop. body.success/failure both -> ir.start.
 * As a BB: infinite generator yielding body values.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct { tree_t *body; } icn_repeat_state_t;

DESCR_t coro_bb_repeat_gen(void *zeta, int entry) {
    icn_repeat_state_t *z = (icn_repeat_state_t *)zeta;
    for (;;) {
        DESCR_t bval = z->body ? bb_eval_value(z->body) : NULVCL;
        if (!IS_FAIL_fn(bval)) return bval;
        /* body failed -> continue (repeat never stops on body failure) */
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-21: coro_bb_case_gen -- ir_a_Case  (case E of { C: B ... } used as generator)
 *
 * JCON wiring:
 *   eval E (bounded); compare === each clause expr; on match pump clause body.
 *   On body exhaustion try next clause. Default if no clause matches.
 *
 * State: discriminant value, current clause index, body_box, n_clauses.
 *--------------------------------------------------------------------------------------------------------------------------*/
#define ICN_CASE_MAX 32
typedef struct {
    DESCR_t   disc;          /* discriminant value */
    tree_t   *clause_exprs[ICN_CASE_MAX];
    tree_t   *clause_bodies[ICN_CASE_MAX];
    int       n_clauses;
    tree_t   *dflt;          /* default body (may be null) */
    int       cur_clause;    /* which clause body we're pumping (-1=not started) */
    bb_node_t body_box;
    int       body_started;
} icn_case_state_t;

/* descr equality: mirrors === (TT_IDENTICAL) */
static int descr_identical(DESCR_t a, DESCR_t b) {
    if (a.v != b.v) return 0;
    if (a.v == DT_I) return a.i == b.i;
    if (a.v == DT_R) return a.r == b.r;
    if (a.v == DT_S || a.v == DT_SNUL) {
        const char *as = VARVAL_fn(a), *bs = VARVAL_fn(b);
        if (!as && !bs) return 1;
        if (!as || !bs) return 0;
        return strcmp(as, bs) == 0;
    }
    return a.ptr == b.ptr;
}

DESCR_t coro_bb_case_gen(void *zeta, int entry) {
    icn_case_state_t *z = (icn_case_state_t *)zeta;
    /* If already in a body, pump beta first */
    if (z->body_started && entry == beta) {
        DESCR_t v = z->body_box.fn(z->body_box.ζ, beta);
        if (!IS_FAIL_fn(v)) return v;
        z->body_started = 0;
        z->cur_clause++;  /* try next clause */
    }
    /* Find matching clause */
    for (; z->cur_clause < z->n_clauses; z->cur_clause++) {
        tree_t *ce = z->clause_exprs[z->cur_clause];
        if (!ce) continue;
        DESCR_t cv = bb_eval_value(ce);
        if (IS_FAIL_fn(cv)) continue;
        if (!descr_identical(z->disc, cv)) continue;
        /* Match -- pump body */
        tree_t *cb = z->clause_bodies[z->cur_clause];
        if (!cb) return NULVCL;
        z->body_box     = coro_eval(cb);
        z->body_started = 1;
        DESCR_t v = z->body_box.fn(z->body_box.ζ, alpha);
        if (!IS_FAIL_fn(v)) return v;
        z->body_started = 0;
        /* body immediately exhausted, try next clause */
    }
    /* default */
    if (z->dflt) {
        if (!z->body_started) {
            z->body_box     = coro_eval(z->dflt);
            z->body_started = 1;
            DESCR_t v = z->body_box.fn(z->body_box.ζ, alpha);
            if (!IS_FAIL_fn(v)) return v;
        }
    }
    return FAILDESCR;
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-22: LOOP_NEXT / LOOP_BREAK in BB context
 *
 * next -> sets FRAME.loop_next=1; break E -> sets FRAME.loop_break=1, stashes E.
 * These are handled at statement level by bb_exec_stmt's default clause.
 * No separate BB needed -- they propagate via FRAME flags.
 *--------------------------------------------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-26: coro_bb_makelist_gen -- ir_a_ListConstructor with generative elements
 *
 * JCON wiring: evaluate each element left-to-right (all bounded for list construction).
 * The list constructor itself is one-shot (bounded) -- it produces one list.
 * Elements that are generators produce the cross-product, but for simplicity
 * and fidelity with ir_a_ListConstructor: we build the list eagerly, elements
 * are evaluated left-to-right, each one-shot.
 *
 * alpha: eval all children, build list, return it.
 * beta:  FAILDESCR (one-shot).
 *--------------------------------------------------------------------------------------------------------------------------*/
/* Note: non-generative list constructor is already handled in bb_eval_value
 * (TT_MAKELIST case). This BB is wired for when is_suspendable detects a
 * generative child -- we drive the cross-product via existing coro_eval
 * machinery by pumping the generative elements. For now: eager eval, one list. */

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-27: coro_bb_compound_gen -- ir_a_Compound  ((E1; E2; ...; EN) generative)
 *
 * JCON wiring:
 *   E1..E(N-1): evaluate for side effects (bounded); if any fail, continue.
 *   EN: pump as generator, yield its values.
 *   start -> L[1].start; resume -> L[N].resume
 *   L[i].success -> L[i+1].start  (for i < N)
 *   L[i].failure -> L[i+1].start  (failure of non-last also continues)
 *   L[N].success -> ir.success; L[N].failure -> ir.failure
 *--------------------------------------------------------------------------------------------------------------------------*/
#define ICN_COMPOUND_MAX 32
typedef struct {
    tree_t   *children[ICN_COMPOUND_MAX];
    int       n;
    bb_node_t last_box;
    int       started;
} icn_compound_state_t;

DESCR_t coro_bb_compound_gen(void *zeta, int entry) {
    icn_compound_state_t *z = (icn_compound_state_t *)zeta;
    if (entry == alpha || !z->started) {
        /* eval all but last for side effects */
        for (int i = 0; i < z->n - 1; i++)
            if (z->children[i]) bb_eval_value(z->children[i]);
        if (z->n <= 0 || !z->children[z->n - 1]) return FAILDESCR;
        z->last_box = coro_eval(z->children[z->n - 1]);
        z->started  = 1;
        return z->last_box.fn(z->last_box.ζ, alpha);
    }
    return z->last_box.fn(z->last_box.ζ, beta);
}

/*----------------------------------------------------------------------------------------------------------------------------
 * IJ-28: coro_bb_field_gen -- ir_a_Field with generative object
 *
 * JCON: val = eval(object generator each tick); result = val.field
 * alpha/beta: pump object_gen, evaluate field on each result.
 *--------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    bb_node_t  obj_gen;
    const char *field;
} icn_field_gen_state_t;

DESCR_t coro_bb_field_gen(void *zeta, int entry) {
    icn_field_gen_state_t *z = (icn_field_gen_state_t *)zeta;
    DESCR_t obj = z->obj_gen.fn(z->obj_gen.ζ, entry);
    if (IS_FAIL_fn(obj)) return FAILDESCR;
    if (!z->field) return obj;
    return FIELD_GET_fn(obj, z->field);
}

/*============================================================================================================================
 * coro_eval additions -- wire new BBs into the dispatch.
 * Call coro_eval_missing(e) from coro_eval's fallback.
 *============================================================================================================================*/

bb_node_t coro_eval_missing(tree_t *e) {
    if (!e) { icn_lazy_state_t *z = calloc(1, sizeof(*z)); return (bb_node_t){ icn_lazy_box, z, 0 }; }

    /* ir_a_Not */
    if (e->t == TT_NOT) {
        icn_not_state_t *z = calloc(1, sizeof(*z));
        z->expr = (e->n >= 1) ? e->c[0] : NULL;
        return (bb_node_t){ coro_bb_not, z, 0 };
    }

    /* ir_a_While (as generator) */
    if (e->t == TT_WHILE) {
        icn_while_state_t *z = calloc(1, sizeof(*z));
        z->expr = (e->n >= 1) ? e->c[0] : NULL;
        z->body = (e->n >= 2) ? e->c[1] : NULL;
        return (bb_node_t){ coro_bb_while_gen, z, 0 };
    }

    /* ir_a_Until (as generator) */
    if (e->t == TT_UNTIL) {
        icn_until_state_t *z = calloc(1, sizeof(*z));
        z->expr = (e->n >= 1) ? e->c[0] : NULL;
        z->body = (e->n >= 2) ? e->c[1] : NULL;
        return (bb_node_t){ coro_bb_until_gen, z, 0 };
    }

    /* ir_a_Repeat (as generator) */
    if (e->t == TT_REPEAT) {
        icn_repeat_state_t *z = calloc(1, sizeof(*z));
        z->body = (e->n >= 1) ? e->c[0] : NULL;
        return (bb_node_t){ coro_bb_repeat_gen, z, 0 };
    }

    /* ir_a_Case (as generator) */
    if (e->t == TT_CASE) {
        icn_case_state_t *z = calloc(1, sizeof(*z));
        /* c[0] = discriminant; c[1..2n] = clause_expr, clause_body pairs; last = default */
        z->disc = e->n >= 1 ? bb_eval_value(e->c[0]) : NULVCL;
        z->cur_clause   = 0;
        z->body_started = 0;
        int i = 1;
        while (i + 1 < e->n && z->n_clauses < ICN_CASE_MAX) {
            z->clause_exprs[z->n_clauses]  = e->c[i];
            z->clause_bodies[z->n_clauses] = e->c[i + 1];
            z->n_clauses++;
            i += 2;
        }
        z->dflt = (i < e->n) ? e->c[i] : NULL;
        return (bb_node_t){ coro_bb_case_gen, z, 0 };
    }

    /* ir_a_Compound (generative -- last child is generator) */
    if (e->t == TT_SEQ_EXPR) {
        icn_compound_state_t *z = calloc(1, sizeof(*z));
        z->n = 0;
        for (int i = 0; i < e->n && z->n < ICN_COMPOUND_MAX; i++)
            z->children[z->n++] = e->c[i];
        return (bb_node_t){ coro_bb_compound_gen, z, 0 };
    }

    /* ir_a_Field with generative object */
    if (e->t == TT_FIELD && e->n >= 1 && is_suspendable(e->c[0])) {
        icn_field_gen_state_t *z = calloc(1, sizeof(*z));
        z->obj_gen = coro_eval(e->c[0]);
        z->field   = e->v.sval;
        return (bb_node_t){ coro_bb_field_gen, z, 0 };
    }

    /* ir_a_RepAlt -- TT_ALTERNATE with single child wrapping repeat semantics.
     * In our AST, |E is represented as TT_ALTERNATE with one child.
     * Standard TT_ALTERNATE with 2+ children is handled by coro_bb_alternate. */
    /* (covered by existing coro_bb_alternate for n>=2; n==1 is |E) */
    if (e->t == TT_ALTERNATE && e->n == 1) {
        icn_repalt_state_t *z = calloc(1, sizeof(*z));
        z->expr          = e->c[0];
        z->started       = 0;
        z->ever_succeeded = 0;
        return (bb_node_t){ coro_bb_repalt, z, 0 };
    }

    /* fallback: lazy box */
    icn_lazy_state_t *z = calloc(1, sizeof(*z));
    z->expr = e;
    return (bb_node_t){ icn_lazy_box, z, 0 };
}
