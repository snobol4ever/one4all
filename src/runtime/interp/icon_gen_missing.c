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

bb_node_t coro_eval_missing_base(tree_t *e) {
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

/*============================================================================================================================
 * icn_bb_proc_call -- pure BB Icon procedure executor (replaces coro_bb_suspend + swapcontext)
 *
 * ir_a_Suspend wiring (JCON):
 *   start  → expr.start   (pump the expr generator α)
 *   expr.γ → yield value; β comes back → expr.β (resume generator)
 *   expr.ω → proc failure (no more values from this suspend)
 *
 * For a proc body with N statements:
 *   - Non-suspend statements: execute eagerly via bb_exec_stmt, advance to next.
 *   - Suspend statements: build bb_node_t from expr; pump α/β until ω; advance.
 *   - Return E: evaluate E, set return_val, stop.
 *   - Fall off end: proc fails.
 *
 * State: stmt index, current expr_box (if pumping a suspend), proc tree_t*.
 *============================================================================================================================*/

#define ICN_PROC_STMT_MAX 256

typedef struct {
    tree_t   *proc;           /* TT_FNC proc node */
    int       body_start;     /* index of first body stmt in proc->c[] */
    int       nbody;          /* number of body statements */
    int       stmt_idx;       /* current statement index */
    bb_node_t expr_box;       /* current suspend expr generator */
    int       in_suspend;     /* 1 if currently pumping a suspend expr */
    tree_t   *suspend_body;   /* do-clause of current suspend (may be NULL) */
} icn_proc_state_t;

/* Forward: frame push/pop -- same as coro_call */
extern int frame_depth;
extern IcnFrame frame_stack[];
/* extern declarations from coro_runtime.h */

static void icn_bb_proc_push_frame(tree_t *proc, DESCR_t *args, int nargs,
                                    IcnScope *sc_out, int *nslots_out) {
    int nparams    = proc->_id;
    int body_start = 1 + nparams;
    int nbody      = proc->n - body_start;
    IcnScope sc;  sc.n = 0;
    for (int i = 0; i < nparams && i < FRAME_SLOT_MAX; i++) {
        tree_t *pn = proc->c[1+i];
        if (pn && pn->v.sval) scope_add(&sc, pn->v.sval);
    }
    for (int i = 0; i < nbody; i++) {
        tree_t *st = proc->c[body_start+i];
        if (st && st->t == TT_GLOBAL)
            for (int j = 0; j < st->n; j++)
                if (st->c[j] && st->c[j]->v.sval)
                    scope_add(&sc, st->c[j]->v.sval);
    }
    for (int i = 0; i < nbody; i++)
        icn_scope_patch(&sc, proc->c[body_start+i]);
    int nslots = sc.n > 0 ? sc.n : (nparams > 0 ? nparams : FRAME_SLOT_MAX);
    if (nslots > FRAME_SLOT_MAX) nslots = FRAME_SLOT_MAX;
    IcnFrame *f = &frame_stack[frame_depth++];
    memset(f, 0, sizeof *f);
    f->env_n = nslots;
    f->sc    = sc;
    for (int i = 0; i < nparams && i < nargs && i < FRAME_SLOT_MAX; i++)
        f->env[i] = args[i];
    /* restore statics */
    for (int i = 0; i < nbody; i++) {
        tree_t *st = proc->c[body_start+i];
        if (!st || st->t != TT_GLOBAL || st->v.ival != 1) continue;
        for (int j = 0; j < st->n; j++) {
            tree_t *vn = st->c[j];
            if (!vn || !vn->v.sval) continue;
            int slot = scope_get(&sc, vn->v.sval);
            if (slot < 0 || slot >= nslots) continue;
            DESCR_t saved;
            if (static_get(proc, vn->v.sval, &saved)) f->env[slot] = saved;
        }
    }
    if (sc_out)      *sc_out      = sc;
    if (nslots_out)  *nslots_out  = nslots;
}

static void icn_bb_proc_pop_frame(tree_t *proc) {
    int nparams    = proc->_id;
    int body_start = 1 + nparams;
    int nbody      = proc->n - body_start;
    IcnScope *sc   = &FRAME.sc;
    int nslots     = FRAME.env_n;
    for (int i = 0; i < nbody; i++) {
        tree_t *st = proc->c[body_start+i];
        if (!st || st->t != TT_GLOBAL || st->v.ival != 1) continue;
        for (int j = 0; j < st->n; j++) {
            tree_t *vn = st->c[j];
            if (!vn || !vn->v.sval) continue;
            int slot = scope_get(sc, vn->v.sval);
            if (slot < 0 || slot >= nslots) continue;
            static_set(proc, vn->v.sval, FRAME.env[slot]);
        }
    }
    icn_init_save_frame();
    frame_depth--;
}

/* Extended state: carries args for alpha-time frame push */
typedef struct { icn_proc_state_t base; DESCR_t args[16]; int nargs; } icn_proc_call_state_t;

DESCR_t icn_bb_proc_call(void *zeta, int entry) {
    icn_proc_state_t *z = (icn_proc_state_t *)zeta;
    tree_t *proc       = z->proc;
    int body_start     = z->body_start;
    int nbody          = z->nbody;

    /* alpha: push fresh frame with args from extended state */
    if (entry == alpha) {
        if (frame_depth >= FRAME_STACK_MAX) return FAILDESCR;
        icn_proc_call_state_t *zz = (icn_proc_call_state_t *)zeta;
        icn_bb_proc_push_frame(proc, zz->args, zz->nargs, NULL, NULL);
        z->stmt_idx  = 0;
        z->in_suspend = 0;
    }

    /* Drive statements */
    for (;;) {
        /* If currently pumping a suspend expr, pump beta */
        if (z->in_suspend) {
            DESCR_t v = z->expr_box.fn(z->expr_box.ζ, beta);
            if (!IS_FAIL_fn(v)) {
                if (z->suspend_body) bb_eval_value(z->suspend_body);
                return v;  /* yield next value */
            }
            /* expr exhausted -- advance past this suspend stmt */
            z->in_suspend = 0;
            z->stmt_idx++;
        }

        /* Advance through statements */
        while (z->stmt_idx < nbody) {
            tree_t *st = proc->c[body_start + z->stmt_idx];
            if (!st || st->t == TT_GLOBAL) { z->stmt_idx++; continue; }

            /* TT_RETURN or TT_PROC_FAIL: evaluate, pop frame, done */
            if (st->t == TT_RETURN) {
                DESCR_t rv = (st->n >= 1 && st->c[0]) ? bb_eval_value(st->c[0]) : NULVCL;
                icn_bb_proc_pop_frame(proc);
                return rv;  /* one value, then omega on next beta */
            }
            if (st->t == TT_PROC_FAIL) {
                icn_bb_proc_pop_frame(proc);
                return FAILDESCR;
            }

            /* TT_SUSPEND: build expr generator, pump alpha for first value */
            if (st->t == TT_SUSPEND) {
                tree_t *expr_node = (st->n >= 1) ? st->c[0] : NULL;
                tree_t *body_node = (st->n >= 2) ? st->c[1] : NULL;
                if (!expr_node) { z->stmt_idx++; continue; }
                z->expr_box    = coro_eval(expr_node);
                z->suspend_body = body_node;
                z->in_suspend   = 1;
                DESCR_t v = z->expr_box.fn(z->expr_box.ζ, alpha);
                if (!IS_FAIL_fn(v)) {
                    if (body_node) bb_eval_value(body_node);
                    return v;  /* first yield */
                }
                /* expr immediately exhausted */
                z->in_suspend = 0;
                z->stmt_idx++;
                continue;
            }

            /* All other statements: execute for side effects, advance */
            bb_exec_stmt(st);
            if (FRAME.returning) {
                DESCR_t rv = FRAME.return_val;
                icn_bb_proc_pop_frame(proc);
                return rv;
            }
            if (FRAME.loop_break) { z->stmt_idx++; continue; }
            z->stmt_idx++;
        }

        /* Fell off end of body -- proc fails */
        icn_bb_proc_pop_frame(proc);
        return FAILDESCR;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------
 * icn_bb_make_proc_box -- build an icn_bb_proc_call box for a proc node.
 * Called from coro_eval TT_FNC user-proc path to replace coro_bb_suspend.
 *--------------------------------------------------------------------------------------------------------------------------*/
bb_node_t icn_bb_make_proc_box(tree_t *proc, DESCR_t *args, int nargs) {
    icn_proc_call_state_t *zz = calloc(1, sizeof(*zz));
    zz->base.proc       = proc;
    zz->base.body_start = 1 + proc->_id;
    zz->base.nbody      = proc->n - zz->base.body_start;
    zz->base.stmt_idx   = 0;
    zz->base.in_suspend = 0;
    zz->nargs = nargs < 16 ? nargs : 16;
    for (int i = 0; i < zz->nargs; i++) zz->args[i] = args ? args[i] : NULVCL;
    return (bb_node_t){ icn_bb_proc_call, zz, 0 };
}

/*============================================================================================================================
 * coro_bb_section_gen -- ir_a_Sectionop with generative operands
 *
 * JCON wiring: val (object), left (i), right (j) all potentially generative.
 * Drive val first; on each val tick drive left; on each left tick drive right;
 * on each right tick compute val[left:right] (or +: or -:) and yield.
 * On right exhaustion resume left; on left exhaustion resume val.
 *
 * State: val_gen, left_gen, right_gen, section op kind, cached val/left values.
 *============================================================================================================================*/
typedef enum { ICN_SEC_RANGE, ICN_SEC_PLUS, ICN_SEC_MINUS } icn_sec_kind_t;
typedef struct {
    bb_node_t    val_gen;
    bb_node_t    left_gen;
    bb_node_t    right_gen;
    tree_t      *val_expr;
    tree_t      *left_expr;
    tree_t      *right_expr;
    icn_sec_kind_t kind;
    DESCR_t      cur_val;
    DESCR_t      cur_left;
    int          val_started;
    int          left_started;
    int          right_started;
} icn_section_gen_state_t;

static DESCR_t icn_apply_section(DESCR_t val, DESCR_t left, DESCR_t right, icn_sec_kind_t kind) {
    /* Mirrors bb_section in coro_value.c */
    extern DESCR_t bb_eval_value(tree_t *e);
    const char *s = VARVAL_fn(val); if (!s) s = "";
    long slen = (long)strlen(s);
    long i = IS_INT_fn(left)  ? left.i  : 0;
    long j = IS_INT_fn(right) ? right.i : 0;
    /* Normalize positions: 0->slen+1, negative->from end */
    if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
    if (kind == ICN_SEC_RANGE) {
        if (j == 0) j = slen + 1; else if (j < 0) j = slen + 1 + j;
    } else if (kind == ICN_SEC_PLUS)  { j = i + j; }
      else                            { j = i - j; }
    if (i > j) { long t = i; i = j; j = t; }
    i--; /* 0-based start */
    if (i < 0 || j > slen + 1 || i > j) return FAILDESCR;
    long len = j - i;
    char *buf = GC_malloc(len + 1);
    memcpy(buf, s + i, len); buf[len] = '\0';
    return STRVAL(buf);
}

DESCR_t coro_bb_section_gen(void *zeta, int entry) {
    icn_section_gen_state_t *z = (icn_section_gen_state_t *)zeta;
    int vport = (z->val_started   && entry == beta) ? beta : alpha;
    int lport = (z->left_started  && entry == beta) ? beta : alpha;
    int rport = (z->right_started && entry == beta) ? beta : alpha;

    for (;;) {
        /* Drive val */
        if (!z->val_started || rport == alpha && lport == alpha) {
            z->cur_val = z->val_gen.fn(z->val_gen.ζ, z->val_started ? beta : alpha);
            z->val_started = 1;
            if (IS_FAIL_fn(z->cur_val)) return FAILDESCR;
            /* Reset left/right for new val */
            z->left_gen    = coro_eval(z->left_expr);
            z->left_started = 0;
            z->right_started = 0;
        }
        /* Drive left */
        z->cur_left = z->left_gen.fn(z->left_gen.ζ, z->left_started ? beta : alpha);
        z->left_started = 1;
        if (IS_FAIL_fn(z->cur_left)) {
            /* Left exhausted: resume val */
            z->left_started = 0; z->right_started = 0;
            z->cur_val = z->val_gen.fn(z->val_gen.ζ, beta);
            if (IS_FAIL_fn(z->cur_val)) return FAILDESCR;
            z->left_gen = coro_eval(z->left_expr);
            z->left_started = 0;
            continue;
        }
        /* Drive right */
        z->right_gen = coro_eval(z->right_expr);
        z->right_started = 0;
        DESCR_t rv = z->right_gen.fn(z->right_gen.ζ, alpha);
        z->right_started = 1;
        if (IS_FAIL_fn(rv)) continue; /* right immediately exhausted, try next left */
        return icn_apply_section(z->cur_val, z->cur_left, rv, z->kind);
    }
}

/*============================================================================================================================
 * coro_bb_key_gen -- ir_a_Key for generative keywords (&features, &allocated, etc.)
 *
 * JCON: generative keywords emit ir_ResumeValue; one-shot keywords emit ir_Key once.
 * Generative keywords in Icon: &features (generates feature strings), &allocated (3 ints),
 * &collections (4 ints). For now: fallback to icn_kw_read and wrap as one-shot.
 * Actual generators added per keyword as needed.
 *============================================================================================================================*/
typedef struct { const char *kw; int fired; DESCR_t val; } icn_kw_gen_state_t;

DESCR_t coro_bb_key_gen(void *zeta, int entry) {
    icn_kw_gen_state_t *z = (icn_kw_gen_state_t *)zeta;
    if (entry == beta || z->fired) return FAILDESCR;
    z->fired = 1;
    extern DESCR_t icn_kw_read(const char *kw);
    return icn_kw_read(z->kw);
}

/*============================================================================================================================
 * coro_bb_listcon_gen -- ir_a_ListConstructor with generative elements
 *
 * JCON: evaluate each element (bounded); collect into a list; return it once.
 * The ListConstructor itself is one-shot (always bounded in JCON).
 * Generative elements are evaluated eagerly — the list gets the first value of each.
 * State: child exprs, count.
 *============================================================================================================================*/
#define ICN_LISTCON_MAX 64
typedef struct {
    tree_t *children[ICN_LISTCON_MAX];
    int     n;
    int     fired;
} icn_listcon_state_t;

DESCR_t coro_bb_listcon_gen(void *zeta, int entry) {
    icn_listcon_state_t *z = (icn_listcon_state_t *)zeta;
    if (entry == beta || z->fired) return FAILDESCR;
    z->fired = 1;
    DESCR_t elems[ICN_LISTCON_MAX];
    for (int i = 0; i < z->n; i++) {
        elems[i] = z->children[i] ? bb_eval_value(z->children[i]) : NULVCL;
        if (IS_FAIL_fn(elems[i])) return FAILDESCR;
    }
    /* Build icnlist descriptor -- mirrors MAKELIST in icn_try_call_builtin_by_name */
    static int reg = 0;
    if (!reg) { DEFDAT_fn("icnlist(frame_elems,frame_size,icn_type)"); reg = 1; }
    DESCR_t *ep = GC_malloc((z->n > 0 ? z->n : 1) * sizeof(DESCR_t));
    for (int i = 0; i < z->n; i++) ep[i] = elems[i];
    DESCR_t eptr; eptr.v = DT_DATA; eptr.slen = 0; eptr.ptr = (void*)ep;
    return DATCON_fn("icnlist", eptr, INTVAL(z->n), STRVAL("list"));
}

/*============================================================================================================================
 * Wire new BBs into coro_eval_missing
 *============================================================================================================================*/
/* Override: replace the old coro_eval_missing with extended version */
/* Note: we append here; the linker will use the last definition if both
 * are non-static. Instead, we rename the old one and call from new. */

/*============================================================================================================================
 * coro_eval_missing -- top-level dispatch for all missing JCON BBs.
 * Called from coro_eval fallback. Handles new constructs then delegates.
 *============================================================================================================================*/
bb_node_t coro_eval_missing(tree_t *e) {
    if (!e) goto fallback;

    /* ir_a_Sectionop with generative operands */
    if ((e->t == TT_SECTION || e->t == TT_SECTION_PLUS || e->t == TT_SECTION_MINUS)
        && e->n >= 3
        && (is_suspendable(e->c[0]) || is_suspendable(e->c[1]) || is_suspendable(e->c[2]))) {
        icn_section_gen_state_t *z = calloc(1, sizeof(*z));
        z->val_expr   = e->c[0];
        z->left_expr  = e->c[1];
        z->right_expr = e->c[2];
        z->val_gen    = coro_eval(e->c[0]);
        z->left_gen   = coro_eval(e->c[1]);
        z->kind       = (e->t == TT_SECTION_PLUS) ? ICN_SEC_PLUS
                      : (e->t == TT_SECTION_MINUS) ? ICN_SEC_MINUS
                      : ICN_SEC_RANGE;
        return (bb_node_t){ coro_bb_section_gen, z, 0 };
    }

    /* ir_a_Key -- generative keyword */
    if (e->t == TT_KEYWORD && e->v.sval) {
        icn_kw_gen_state_t *z = calloc(1, sizeof(*z));
        z->kw    = e->v.sval;
        z->fired = 0;
        return (bb_node_t){ coro_bb_key_gen, z, 0 };
    }

    /* ir_a_ListConstructor -- generative list [e1, e2, ...] */
    if (e->t == TT_MAKELIST) {
        icn_listcon_state_t *z = calloc(1, sizeof(*z));
        z->n     = 0;
        z->fired = 0;
        for (int i = 0; i < e->n && z->n < ICN_LISTCON_MAX; i++)
            z->children[z->n++] = e->c[i];
        return (bb_node_t){ coro_bb_listcon_gen, z, 0 };
    }

    fallback:
    return coro_eval_missing_base(e);
}

/*============================================================================================================================
 * State allocators for emit_bb.c wiring -- one per new BB
 *============================================================================================================================*/
icn_not_state_t        *icon_not_new(void)          { return calloc(1, sizeof(icn_not_state_t)); }
icn_repalt_state_t     *icon_repalt_new(void)        { return calloc(1, sizeof(icn_repalt_state_t)); }
icn_while_state_t      *icon_while_gen_new(void)     { return calloc(1, sizeof(icn_while_state_t)); }
icn_until_state_t      *icon_until_gen_new(void)     { return calloc(1, sizeof(icn_until_state_t)); }
icn_repeat_state_t     *icon_repeat_gen_new(void)    { return calloc(1, sizeof(icn_repeat_state_t)); }
icn_case_state_t       *icon_case_gen_new(void)      { return calloc(1, sizeof(icn_case_state_t)); }
icn_compound_state_t   *icon_compound_gen_new(void)  { return calloc(1, sizeof(icn_compound_state_t)); }
icn_field_gen_state_t  *icon_field_gen_new(void)     { return calloc(1, sizeof(icn_field_gen_state_t)); }
icn_section_gen_state_t *icon_section_gen_new(void)  { return calloc(1, sizeof(icn_section_gen_state_t)); }
icn_kw_gen_state_t     *icon_kw_gen_new(void)        { return calloc(1, sizeof(icn_kw_gen_state_t)); }
icn_listcon_state_t    *icon_listcon_gen_new(void)   { return calloc(1, sizeof(icn_listcon_state_t)); }
icn_proc_call_state_t  *icon_proc_call_new(void)     { return calloc(1, sizeof(icn_proc_call_state_t)); }
