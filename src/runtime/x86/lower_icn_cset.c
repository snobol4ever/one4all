/*
 * lower_icn_cset.c — Cset operation and list-concat handlers (SR-9)
 *
 * AST kinds handled:
 *   AST_CSET_COMPL   ~E        cset complement
 *   AST_CSET_UNION   E1 ++ E2  cset union
 *   AST_CSET_DIFF    E1 -- E2  cset difference
 *   AST_CSET_INTER   E1 ** E2  cset intersection
 *   AST_LCONCAT      E1 ||| E2 Icon list concatenation
 *
 * The four cset operations have no SM-level opcodes; they are handled
 * by the IR interpreter (coro_value.c) via emit_push_expr → SM_PUSH_EXPR.
 * Registering them here makes the absence of a dedicated SM path explicit
 * rather than relying on the silent default: fallthrough.
 *
 * AST_LCONCAT has two paths:
 *   - Scalar (no generative children): lower each child + SM_CONCAT pairs.
 *   - Generative: route through SM_BB_PUMP_AST (handled by sm_interp).
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"
#include "sm_interp.h"
#include "../../runtime/interp/coro_runtime.h"

/* ── Cset ops — no SM opcode; fall through to IR interpreter ── */

static void lower_cset_op(LowerCtx *c, const AST_t *e)
{
    emit_push_expr(c, e);
}

/* ── AST_LCONCAT ─────────────────────────────────────────────── */

static void lower_lconcat(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    /* Scalar (non-generative): lower each child, emit SM_CONCAT between pairs. */
    int has_gen = 0;
    for (int j = 0; j < e->nchildren; j++) {
        if (is_suspendable(e->children[j])) { has_gen = 1; break; }
    }
    if (!has_gen) {
        if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
        for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
        for (int i = 1; i < e->nchildren; i++) sm_emit(p, SM_CONCAT);
        return;
    }
    sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)e));
}

/* ── Registration ─────────────────────────────────────────────── */

void lower_icn_cset_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_CSET_COMPL] = lower_cset_op;
    tbl[AST_CSET_UNION]  = lower_cset_op;
    tbl[AST_CSET_DIFF]   = lower_cset_op;
    tbl[AST_CSET_INTER]  = lower_cset_op;
    tbl[AST_LCONCAT]     = lower_lconcat;
}
