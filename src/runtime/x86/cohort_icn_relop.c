/*
 * cohort_icn_relop.c — Numeric and string relational comparison handlers (SR-9)
 *
 * AST kinds handled:
 *   Numeric (6): AST_EQ  AST_NE  AST_LT  AST_LE  AST_GT  AST_GE
 *   String  (6): AST_LLT AST_LLE AST_LGT AST_LGE AST_LEQ AST_LNE
 *
 * Both groups lower identically: push both operands, emit the comparison
 * opcode with e->kind as the discriminator (SM_ACOMP for numeric,
 * SM_LCOMP for string).  The sm_interp reads e->kind back to select the
 * actual comparison at runtime.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"

static void lower_acomp(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
    lower_expr(c, e->nchildren > 1 ? e->children[1] : NULL);
    sm_emit_i(p, SM_ACOMP, (int64_t)e->kind);
}

static void lower_lcomp(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
    lower_expr(c, e->nchildren > 1 ? e->children[1] : NULL);
    sm_emit_i(p, SM_LCOMP, (int64_t)e->kind);
}

void cohort_icn_relop_register(LowerHandler tbl[AST_KIND_COUNT])
{
    /* Numeric */
    tbl[AST_EQ] = lower_acomp;
    tbl[AST_NE] = lower_acomp;
    tbl[AST_LT] = lower_acomp;
    tbl[AST_LE] = lower_acomp;
    tbl[AST_GT] = lower_acomp;
    tbl[AST_GE] = lower_acomp;
    /* String */
    tbl[AST_LLT] = lower_lcomp;
    tbl[AST_LLE] = lower_lcomp;
    tbl[AST_LGT] = lower_lcomp;
    tbl[AST_LGE] = lower_lcomp;
    tbl[AST_LEQ] = lower_lcomp;
    tbl[AST_LNE] = lower_lcomp;
}
