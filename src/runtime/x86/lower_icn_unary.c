/*
 * lower_icn_unary.c — Icon unary / miscellaneous expression handlers (SR-9)
 *
 * AST kinds handled:
 *   AST_NONNULL    \E    succeed iff E is non-null
 *   AST_NULL       /E    succeed iff E is null (Icon)
 *   AST_NOT        ~E    logical not (succeed iff E fails)
 *   AST_SIZE       *E    string/list/set size
 *   AST_RANDOM     ?E    random element / random integer
 *   AST_IDENTICAL  E === E  identity comparison
 *   AST_AUGOP      E op:= E  augmented assignment
 *
 * AST_AUGOP: e->ival carries the raw IcnTkKind (TK_AUGPLUS etc.) as stored
 * by icon_parse.c.  The legacy lower.c case contained an inline
 *   #include "../../frontend/icon/icon_lex.h"
 * mid-function inside the switch body — a layering violation.  SR-9 moves
 * that include to the top of this file (correct placement), eliminating the
 * in-function #include while keeping the TK_AUG* token encoding intact so
 * the IR interpreter (interp_eval.c) continues to work unchanged.
 *
 * AugOp_e in ast.h documents the semantic mapping but is not used here —
 * the TK_* values are the authoritative encoding in e->ival.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"
/* icon_lex.h is included here (top of file) rather than mid-function
 * as in the original lower.c AST_AUGOP case. */
#include "../../frontend/icon/icon_lex.h"

/* ── AST_NONNULL ─────────────────────────────────────────────── */

static void lower_nonnull(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
    sm_emit_si(p, SM_CALL_FN, "NONNULL", 1);
}

/* ── AST_NULL ────────────────────────────────────────────────── */

static void lower_null(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
    sm_emit_si(p, SM_CALL_FN, "ICN_NULL", 1);
}

/* ── AST_NOT ─────────────────────────────────────────────────── */

static void lower_not(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
    int js   = sm_emit_i(p, SM_JUMP_S, 0);   /* succeeded → flip to fail */
    sm_emit(p, SM_VOID_POP);
    sm_emit(p, SM_PUSH_NULL);
    int jend = sm_emit_i(p, SM_JUMP, 0);
    int fail_lbl = sm_label(p);
    sm_patch_jump(p, js, fail_lbl);
    sm_emit(p, SM_VOID_POP);
    sm_emit_si(p, SM_CALL_FN, "FAIL", 0);
    int end_lbl = sm_label(p);
    sm_patch_jump(p, jend, end_lbl);
}

/* ── AST_SIZE ────────────────────────────────────────────────── */

static void lower_size(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
    sm_emit_si(p, SM_CALL_FN, "SIZE", 1);
}

/* ── AST_RANDOM ──────────────────────────────────────────────── */

static void lower_random(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren >= 1) {
        lower_expr(c, e->children[0]);
        sm_emit_si(p, SM_CALL_FN, "ICN_RANDOM", 1);
    } else {
        sm_emit(p, SM_PUSH_NULL);
    }
}

/* ── AST_IDENTICAL ───────────────────────────────────────────── */

static void lower_identical(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
    lower_expr(c, e->nchildren > 1 ? e->children[1] : NULL);
    sm_emit_si(p, SM_CALL_FN, "IDENTICAL", 2);
}

/* ── AST_AUGOP ───────────────────────────────────────────────── */
/*
 * Inline lowering for simple lhs (AST_VAR, AST_KEYWORD).
 * Falls through to AUGOP call for complex lhs (subscripts, fields).
 *
 * e->ival carries AugOp_e (written by icon_parse.c since SR-9).
 * No frontend token header needed.
 */
static void lower_augop(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    const AST_t *lhs = e->nchildren > 0 ? e->children[0] : NULL;
    const AST_t *rhs = e->nchildren > 1 ? e->children[1] : NULL;
    int op = (int)e->ival;   /* raw IcnTkKind (TK_AUGPLUS etc.) */

    int lhs_slot  = -1;
    const char *lhs_name = NULL;
    int lhs_is_kw = 0;
    if (lhs && lhs->kind == AST_VAR && lhs->sval) {
        const char *vn = lhs->sval;
        if (c->expression_body_lowering && c->expression_scope && vn[0] && vn[0] != '&')
            lhs_slot = scope_get(c->expression_scope, vn);
        if (lhs_slot < 0) lhs_name = vn;
    } else if (lhs && lhs->kind == AST_KEYWORD && lhs->sval) {
        lhs_name = lhs->sval;
        lhs_is_kw = 1;
    }

    if (lhs_slot >= 0 || lhs_name) {
        if (lhs_slot >= 0)   sm_emit_i(p, SM_LOAD_FRAME, lhs_slot);
        else if (lhs_is_kw)  sm_emit_s(p, SM_PUSH_VAR, kw_canonicalize(lhs_name));
        else                 sm_emit_s(p, SM_PUSH_VAR, lhs_name);
        lower_expr(c, rhs);
        switch (op) {
        case TK_AUGPLUS:   sm_emit(p, SM_ADD);    break;
        case TK_AUGMINUS:  sm_emit(p, SM_SUB);    break;
        case TK_AUGSTAR:   sm_emit(p, SM_MUL);    break;
        case TK_AUGSLASH:  sm_emit(p, SM_DIV);    break;
        case TK_AUGMOD:    sm_emit(p, SM_MOD);    break;
        case TK_AUGCONCAT: sm_emit(p, SM_CONCAT); break;
        default:
            sm_emit_i(p, SM_PUSH_LIT_I, (int64_t)op);
            sm_emit_si(p, SM_CALL_FN, "AUGOP", 3);
            return;
        }
        if (lhs_slot >= 0)  sm_emit_i(p, SM_STORE_FRAME, lhs_slot);
        else if (lhs_is_kw) sm_emit_s(p, SM_STORE_VAR, kw_canonicalize(lhs_name));
        else                sm_emit_s(p, SM_STORE_VAR, lhs_name);
        return;
    }
    lower_expr(c, lhs);
    lower_expr(c, rhs);
    sm_emit_i(p, SM_PUSH_LIT_I, (int64_t)op);
    sm_emit_si(p, SM_CALL_FN, "AUGOP", 3);
}

/* ── Registration ─────────────────────────────────────────────── */

void lower_icn_unary_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_NONNULL]   = lower_nonnull;
    tbl[AST_NULL]      = lower_null;
    tbl[AST_NOT]       = lower_not;
    tbl[AST_SIZE]      = lower_size;
    tbl[AST_RANDOM]    = lower_random;
    tbl[AST_IDENTICAL] = lower_identical;
    tbl[AST_AUGOP]     = lower_augop;
}
