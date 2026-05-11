/*
 * cohort_arith.c — Arithmetic and unary operators (SR-6)
 *
 * AST kinds handled:
 *   AST_INTERROGATE  ?X        → lower child (succeed/fail probe, value discarded)
 *   AST_NAME         .X        → SM_PUSH_LIT_S + SM_CALL_FN NAME_PUSH
 *   AST_MNS          -X        → SM_NEG
 *   AST_PLS          +X        → SM_COERCE_NUM
 *   AST_ADD          X + Y     → SM_ADD
 *   AST_SUB          X - Y     → SM_SUB
 *   AST_MUL          X * Y     → SM_MUL
 *   AST_DIV          X / Y     → SM_DIV
 *   AST_MOD          X % Y     → SM_MOD
 *   AST_POW          X ^ Y     → SM_EXP
 *
 * Cross-cutting: none.  All handlers are pure functions of (ctx, e).
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"

static void lower_interrogate(LowerCtx *c, const AST_t *e)
{
    lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
}

static void lower_name(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    const char *vname = (e->nchildren > 0 && e->children[0] && e->children[0]->sval)
                        ? e->children[0]->sval : "";
    sm_emit_s(p, SM_PUSH_LIT_S, vname);
    sm_emit_si(p, SM_CALL_FN, "NAME_PUSH", 1);
}

static void lower_mns(LowerCtx *c, const AST_t *e) { SM_Program *p = c->p; LOWER1_VAL(SM_NEG); }
static void lower_pls(LowerCtx *c, const AST_t *e) { SM_Program *p = c->p; LOWER1_VAL(SM_COERCE_NUM); }
static void lower_add(LowerCtx *c, const AST_t *e) { SM_Program *p = c->p; LOWER2(SM_ADD); }
static void lower_sub(LowerCtx *c, const AST_t *e) { SM_Program *p = c->p; LOWER2(SM_SUB); }
static void lower_mul(LowerCtx *c, const AST_t *e) { SM_Program *p = c->p; LOWER2(SM_MUL); }
static void lower_div(LowerCtx *c, const AST_t *e) { SM_Program *p = c->p; LOWER2(SM_DIV); }
static void lower_mod(LowerCtx *c, const AST_t *e) { SM_Program *p = c->p; LOWER2(SM_MOD); }
static void lower_pow(LowerCtx *c, const AST_t *e) { SM_Program *p = c->p; LOWER2(SM_EXP); }

void cohort_arith_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_INTERROGATE] = lower_interrogate;
    tbl[AST_NAME]        = lower_name;
    tbl[AST_MNS]         = lower_mns;
    tbl[AST_PLS]         = lower_pls;
    tbl[AST_ADD]         = lower_add;
    tbl[AST_SUB]         = lower_sub;
    tbl[AST_MUL]         = lower_mul;
    tbl[AST_DIV]         = lower_div;
    tbl[AST_MOD]         = lower_mod;
    tbl[AST_POW]         = lower_pow;
}
