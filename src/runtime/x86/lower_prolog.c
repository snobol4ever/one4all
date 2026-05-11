/*
 * lower_prolog.c — Prolog backtracking node handlers (SR-11)
 *
 * AST kinds handled:
 *   AST_CHOICE       Prolog clause choice point / procedure head
 *   AST_CLAUSE       Prolog clause body (walked by broker)
 *   AST_CUT          Prolog cut !
 *   AST_UNIFY        Prolog unification =
 *   AST_TRAIL_MARK   trail mark for backtracking
 *   AST_TRAIL_UNWIND trail unwind on backtrack
 *
 * AST_CHOICE has two paths:
 *   - Named predicate (e->sval set): emit SM_BB_ONCE_PROC with arity.
 *   - Anonymous / inline: emit_push_expr + SM_BB_ONCE.
 *
 * AST_CLAUSE/CUT/UNIFY/TRAIL_MARK/TRAIL_UNWIND are children of AST_CHOICE
 * walked by the broker; rarely lowered standalone.  When they are, fall
 * back to emit_push_expr + SM_BB_ONCE.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"
#include <string.h>
#include <stdlib.h>

/* ── AST_CHOICE ──────────────────────────────────────────────── */

static void lower_choice(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->sval) {
        const char *key = e->sval;
        int arity = 0;
        const char *sl = strrchr(key, '/');
        if (sl) arity = atoi(sl + 1);
        sm_emit_si(p, SM_BB_ONCE_PROC, key, (int64_t)arity);
    } else {
        emit_push_expr(c, e);
        sm_emit(p, SM_BB_ONCE);
    }
}

/* ── AST_CLAUSE / AST_CUT / AST_UNIFY / TRAIL_* ─────────────── */

static void lower_prolog_broker_child(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    /* Children of AST_CHOICE walked by the broker; rarely lowered standalone. */
    emit_push_expr(c, e);
    sm_emit(p, SM_BB_ONCE);
}

/* ── Registration ─────────────────────────────────────────────── */

void lower_prolog_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_CHOICE]       = lower_choice;
    tbl[AST_CLAUSE]       = lower_prolog_broker_child;
    tbl[AST_CUT]          = lower_prolog_broker_child;
    tbl[AST_UNIFY]        = lower_prolog_broker_child;
    tbl[AST_TRAIL_MARK]   = lower_prolog_broker_child;
    tbl[AST_TRAIL_UNWIND] = lower_prolog_broker_child;
}
