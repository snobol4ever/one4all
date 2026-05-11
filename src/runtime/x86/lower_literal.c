/*
 * lower_literal.c — Literal value handlers (SR-4)
 *
 * AST kinds handled:
 *   AST_QLIT   "string"   → SM_PUSH_LIT_S
 *   AST_CSET   'chars'    → SM_PUSH_LIT_S  (cset stored as string)
 *   AST_ILIT   integer    → SM_PUSH_LIT_I
 *   AST_FLIT   real       → SM_PUSH_LIT_F
 *   AST_NUL    &null      → SM_PUSH_NULL
 *
 * Note: AST_NULL (/E null-test) is NOT here — it recurses into lower_expr
 * and remains in the legacy switch until lower_expr is made non-static.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"

static void lower_qlit(LowerCtx *c, const AST_t *e)
{
    sm_emit_s(c->p, SM_PUSH_LIT_S, e->sval ? e->sval : "");
}

static void lower_cset(LowerCtx *c, const AST_t *e)
{
    sm_emit_s(c->p, SM_PUSH_LIT_S, e->sval ? e->sval : "");
}

static void lower_ilit(LowerCtx *c, const AST_t *e)
{
    sm_emit_i(c->p, SM_PUSH_LIT_I, (int64_t)e->ival);
}

static void lower_flit(LowerCtx *c, const AST_t *e)
{
    sm_emit_f(c->p, SM_PUSH_LIT_F, e->dval);
}

static void lower_nul(LowerCtx *c, const AST_t *e)
{
    (void)e;
    sm_emit(c->p, SM_PUSH_NULL);
}

void lower_literal_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_QLIT] = lower_qlit;
    tbl[AST_CSET] = lower_cset;
    tbl[AST_ILIT] = lower_ilit;
    tbl[AST_FLIT] = lower_flit;
    tbl[AST_NUL]  = lower_nul;
}
