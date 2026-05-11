/*
 * lower_pat_prim.c — Pattern primitives in value context (SR-7)
 *
 * AST kinds handled (all 18 nullary/unary SNOBOL4 pattern primitives):
 *   AST_ARB      arbitrary match
 *   AST_REM      remainder of subject
 *   AST_FAIL     always fail
 *   AST_SUCCEED  always succeed
 *   AST_FENCE    seal beta / FENCE(P) one-way gate
 *   AST_ABORT    abort entire match
 *   AST_BAL      balanced parentheses
 *   AST_ANY      ANY(S)
 *   AST_NOTANY   NOTANY(S)
 *   AST_SPAN     SPAN(S)
 *   AST_BREAK    BREAK(S)
 *   AST_BREAKX   BREAKX(S)
 *   AST_LEN      LEN(N)
 *   AST_POS      POS(N)
 *   AST_RPOS     RPOS(N)
 *   AST_TAB      TAB(N)
 *   AST_RTAB     RTAB(N)
 *   AST_ARBNO    ARBNO(P)
 *
 * In value context these kinds appear as the rhs of an assignment or as
 * a function argument — they represent a pattern object.  All handlers
 * delegate to lower_pat_expr which emits SM_PAT_* opcodes.
 *
 * Cross-cutting: none.  All handlers are pure functions of (ctx, e).
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"

/* All pat-prim value-context handlers delegate to lower_pat_expr. */
static void lower_pat_prim_val(LowerCtx *c, const AST_t *e)
{
    lower_pat_expr(c, e);
}

void lower_pat_prim_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_ARB]     = lower_pat_prim_val;
    tbl[AST_REM]     = lower_pat_prim_val;
    tbl[AST_FAIL]    = lower_pat_prim_val;
    tbl[AST_SUCCEED] = lower_pat_prim_val;
    tbl[AST_FENCE]   = lower_pat_prim_val;
    tbl[AST_ABORT]   = lower_pat_prim_val;
    tbl[AST_BAL]     = lower_pat_prim_val;
    tbl[AST_ANY]     = lower_pat_prim_val;
    tbl[AST_NOTANY]  = lower_pat_prim_val;
    tbl[AST_SPAN]    = lower_pat_prim_val;
    tbl[AST_BREAK]   = lower_pat_prim_val;
    tbl[AST_BREAKX]  = lower_pat_prim_val;
    tbl[AST_LEN]     = lower_pat_prim_val;
    tbl[AST_POS]     = lower_pat_prim_val;
    tbl[AST_RPOS]    = lower_pat_prim_val;
    tbl[AST_TAB]     = lower_pat_prim_val;
    tbl[AST_RTAB]    = lower_pat_prim_val;
    tbl[AST_ARBNO]   = lower_pat_prim_val;
}
