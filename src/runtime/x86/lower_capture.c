/*
 * lower_capture.c — Pattern capture handlers (SR-8)
 *
 * AST kinds handled (value context — pat-context lives in lower_pat.c):
 *   AST_CAPT_COND_ASGN   .var  conditional capture (on success)
 *   AST_CAPT_IMMED_ASGN  $var  immediate capture
 *   AST_CAPT_CURSOR      @var  cursor position capture
 *
 * In value context these nodes always appear inside a pattern sub-expression
 * (the statement's pattern field).  The correct lowering is identical to the
 * pattern-context path: delegate straight to lower_pat_expr so the SM_PAT_*
 * opcodes are emitted regardless of which context called lower_expr.
 *
 * sm_pat_capture_fn_arg_names() lives in lower_pat.c and is shared.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"

static void lower_capt_cond_asgn(LowerCtx *c, const AST_t *e)
{
    lower_pat_expr(c, e);
}

static void lower_capt_immed_asgn(LowerCtx *c, const AST_t *e)
{
    lower_pat_expr(c, e);
}

static void lower_capt_cursor(LowerCtx *c, const AST_t *e)
{
    lower_pat_expr(c, e);
}

void lower_capture_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_CAPT_COND_ASGN]  = lower_capt_cond_asgn;
    tbl[AST_CAPT_IMMED_ASGN] = lower_capt_immed_asgn;
    tbl[AST_CAPT_CURSOR]     = lower_capt_cursor;
}
