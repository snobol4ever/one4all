/*
 * lower_ref.c — Reference handlers (SR-5)
 *
 * AST kinds handled:
 *   AST_VAR      name      → SM_LOAD_FRAME (in-scope) or SM_PUSH_VAR (NV store)
 *   AST_KEYWORD  &kw       → SM_PUSH_VAR (canonicalized uppercase)
 *   AST_INDIRECT $expr     → SM_CALL_FN INDIR_GET, with $.var[idx] fast path
 *   AST_DEFER    *expr     → SM_PUSH_EXPRESSION (thawed at call time via EVAL_fn)
 *
 * Cross-cutting: AST_VAR consults ctx->expression_body_lowering and
 *   ctx->expression_scope (set/cleared by the per-proc lowering loop in
 *   lower.c). All other handlers are pure functions of (ctx, e).
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"

static void lower_var(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    const char *vn = e->sval ? e->sval : "";
    /* Inside proc-body expression lowering, consult the per-proc frame scope.
     * Globals, keywords ('&'-prefixed), and unscoped names fall through to
     * SM_PUSH_VAR (NV store). */
    if (c->expression_body_lowering && c->expression_scope && vn[0] && vn[0] != '&') {
        int slot = scope_get(c->expression_scope, vn);
        if (slot >= 0) { sm_emit_i(p, SM_LOAD_FRAME, slot); return; }
    }
    sm_emit_s(p, SM_PUSH_VAR, vn);
}

static void lower_keyword(LowerCtx *c, const AST_t *e)
{
    /* Keywords are stored uppercase; fold the source case before lookup. */
    sm_emit_s(c->p, SM_PUSH_VAR, kw_canonicalize(e->sval));
}

static void lower_indirect(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    /* $expr — eval name-string, look up variable, push value.
     * $.var<idx> special case: push var value directly + IDX (bypasses INDIR_GET). */
    AST_t *ch = e->nchildren > 0 ? e->children[0] : NULL;
    if (ch && ch->kind == AST_NAME && ch->nchildren == 1) {
        AST_t *inner = ch->children[0];
        if (inner && inner->kind == AST_IDX && inner->nchildren >= 2
                && inner->children[0] && inner->children[0]->kind == AST_VAR
                && inner->children[0]->sval) {
            const char *vn = inner->children[0]->sval;
            sm_emit_s(p, SM_PUSH_VAR, vn);
            for (int i = 1; i < inner->nchildren; i++) lower_expr(c, inner->children[i]);
            sm_emit_si(p, SM_CALL_FN, "IDX", (int64_t)inner->nchildren);
            return;
        }
    }
    lower_expr(c, ch);
    sm_emit_si(p, SM_CALL_FN, "INDIR_GET", 1);
}

static void lower_defer(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    /* *expr in value context — lower child as a compiled SM expression.
     * DT_E carries an entry_pc; EVAL_fn thaws it at call time.
     *
     *   SM_JUMP  skip
     *   entry_pc: <lower_expr(child)>
     *   SM_RETURN
     *   skip: SM_PUSH_EXPRESSION entry_pc, 0
     */
    const AST_t *child = e->nchildren > 0 ? e->children[0] : NULL;
    int skip_jump = sm_emit_i(p, SM_JUMP, 0);
    int entry_pc  = sm_label(p);
    if (child) lower_expr(c, child);
    else       sm_emit(p, SM_PUSH_NULL);
    sm_emit(p, SM_RETURN);
    int skip_lbl = sm_label(p);
    sm_patch_jump(p, skip_jump, skip_lbl);
    sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
}

void lower_ref_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_VAR]      = lower_var;
    tbl[AST_KEYWORD]  = lower_keyword;
    tbl[AST_INDIRECT] = lower_indirect;
    tbl[AST_DEFER]    = lower_defer;
}
