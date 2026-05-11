/*
 * cohort_call.c — Call, index, assignment, scan, and swap handlers (SR-8)
 *
 * AST kinds handled:
 *   AST_FNC     function / builtin call (n-ary); includes EVAL(*expr) and
 *               Icon-style call (sval==NULL, children[0] is callee node)
 *   AST_IDX     array / table / record subscript
 *   AST_ASSIGN  assignment (simple var, keyword, field mutator, IDX, generic)
 *   AST_SCAN    E ? E  scanning expression (Icon)
 *   AST_SWAP    :=:  swap bindings (SNOBOL4 / Icon)
 *
 * Cross-cutting: AST_ASSIGN inspects c->expression_body_lowering and
 *   c->expression_scope for frame-slot optimisation (identical to the
 *   former inline case).  AST_SWAP does the same for both operands.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"

/* ── AST_FNC ──────────────────────────────────────────────────────────── */

static void lower_fnc(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    int nargs = e->nchildren;

    /* EVAL(*expr): emit expression inline + SM_CALL_EXPRESSION. */
    if (nargs == 1 && e->sval && strcmp(e->sval, "EVAL") == 0
            && e->children[0] && e->children[0]->kind == AST_DEFER) {
        const AST_t *defer = e->children[0];
        const AST_t *child = defer->nchildren > 0 ? defer->children[0] : NULL;
        int skip_jump = sm_emit_i(p, SM_JUMP, 0);
        int entry_pc  = sm_label(p);
        if (child) lower_expr(c, child);
        else       sm_emit(p, SM_PUSH_NULL);
        sm_emit(p, SM_RETURN);
        int skip_lbl = sm_label(p);
        sm_patch_jump(p, skip_jump, skip_lbl);
        sm_emit_ii(p, SM_CALL_EXPRESSION, (int64_t)entry_pc, 0);
        return;
    }

    /* Icon-style call: sval is NULL; children[0] is the callee name node. */
    if (!e->sval && nargs >= 1 && e->children[0] && e->children[0]->sval) {
        const char *fn = e->children[0]->sval;
        int real_nargs = nargs - 1;
        for (int i = 1; i <= real_nargs; i++) lower_expr(c, e->children[i]);
        sm_emit_si(p, SM_CALL_FN, fn, (int64_t)real_nargs);
        return;
    }

    for (int i = 0; i < nargs; i++) lower_expr(c, e->children[i]);
    sm_emit_si(p, SM_CALL_FN, e->sval ? e->sval : "", (int64_t)nargs);
}

/* ── AST_IDX ──────────────────────────────────────────────────────────── */

static void lower_idx(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
    sm_emit_si(p, SM_CALL_FN, "IDX", (int64_t)e->nchildren);
}

/* ── AST_ASSIGN ───────────────────────────────────────────────────────── */

static void lower_assign(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;

    /* child[1] = rhs; child[0] = lhs */
    lower_expr(c, e->nchildren > 1 ? e->children[1] : NULL);

    if (e->nchildren > 0 && e->children[0]) {
        const AST_t *lhs = e->children[0];

        if (lhs->kind == AST_VAR) {
            const char *vn = lhs->sval ? lhs->sval : "";
            if (c->expression_body_lowering && c->expression_scope
                    && vn[0] && vn[0] != '&') {
                int slot = scope_get(c->expression_scope, vn);
                if (slot >= 0) { sm_emit_i(p, SM_STORE_FRAME, slot); return; }
            }
            sm_emit_s(p, SM_STORE_VAR, vn);
        }
        else if (lhs->kind == AST_KEYWORD) {
            sm_emit_s(p, SM_STORE_VAR, kw_canonicalize(lhs->sval));
        }
        else if (lhs->kind == AST_FNC && lhs->sval) {
            /* Field mutator: fname(obj) = val → push obj, call fname_SET 2 */
            lower_expr(c, lhs->nchildren > 0 ? lhs->children[0] : NULL);
            char setname[256];
            snprintf(setname, sizeof(setname), "%s_SET", lhs->sval);
            sm_emit_si(p, SM_CALL_FN, setname, 2);
        }
        else if (lhs->kind == AST_IDX) {
            /* t[k] := v — rhs already on stack; push base + indices, call IDX_SET. */
            int nc = lhs->nchildren;
            for (int i = 0; i < nc; i++) lower_expr(c, lhs->children[i]);
            sm_emit_si(p, SM_CALL_FN, "IDX_SET", (int64_t)(nc + 1));
        }
        else if (lhs->kind == AST_FIELD) {
            /* r.f := v → push obj, push field name, call FIELD_SET 3 */
            lower_expr(c, lhs->nchildren > 0 ? lhs->children[0] : NULL);
            sm_emit_s(p, SM_PUSH_LIT_S, lhs->sval ? lhs->sval : "");
            sm_emit_si(p, SM_CALL_FN, "FIELD_SET", 3);
        }
        else {
            lower_expr(c, lhs);
            sm_emit_si(p, SM_CALL_FN, "ASGN", 2);
        }
    }
}

/* ── AST_SCAN ─────────────────────────────────────────────────────────── */

static void lower_scan(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(c, e->children[0]);
    sm_emit_si(p, SM_CALL_FN, "ICN_SCAN_PUSH", 1);
    sm_emit(p, SM_VOID_POP);
    if (e->nchildren > 1) lower_expr(c, e->children[1]);
    else                  sm_emit(p, SM_PUSH_NULL);
    sm_emit_si(p, SM_CALL_FN, "ICN_SCAN_POP", 1);
}

/* ── AST_SWAP ─────────────────────────────────────────────────────────── */

static void lower_swap(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;

    /* Inline SM sequence for simple variable lvalues.
     * Saves lhs to NV temp, writes rhs→lhs, writes saved→rhs.
     * Leaves new lhs value on stack as expression result. */
    if (e->nchildren >= 2 && e->children[0] && e->children[1] &&
        e->children[0]->kind == AST_VAR && e->children[1]->kind == AST_VAR) {
        const AST_t *lhs = e->children[0], *rhs = e->children[1];
        const char *lname = lhs->sval ? lhs->sval : "";
        const char *rname = rhs->sval ? rhs->sval : "";
        int lslot = -1, rslot = -1;
        if (c->expression_body_lowering && c->expression_scope) {
            if (lname[0] && lname[0] != '&') lslot = scope_get(c->expression_scope, lname);
            if (rname[0] && rname[0] != '&') rslot = scope_get(c->expression_scope, rname);
        }
        if (lslot >= 0) sm_emit_i(p, SM_LOAD_FRAME, lslot);
        else             sm_emit_s(p, SM_PUSH_VAR, lname);
        sm_emit_s(p, SM_STORE_VAR, "__icn_swap_tmp__");
        sm_emit(p, SM_VOID_POP);
        if (rslot >= 0) sm_emit_i(p, SM_LOAD_FRAME, rslot);
        else             sm_emit_s(p, SM_PUSH_VAR, rname);
        if (lslot >= 0) sm_emit_i(p, SM_STORE_FRAME, lslot);
        else             sm_emit_s(p, SM_STORE_VAR, lname);
        sm_emit_s(p, SM_PUSH_VAR, "__icn_swap_tmp__");
        if (rslot >= 0) sm_emit_i(p, SM_STORE_FRAME, rslot);
        else             sm_emit_s(p, SM_STORE_VAR, rname);
        sm_emit(p, SM_VOID_POP);
        return;
    }

    lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
    lower_expr(c, e->nchildren > 1 ? e->children[1] : NULL);
    sm_emit_si(p, SM_CALL_FN, "SWAP", 2);
}

/* ── Registration ─────────────────────────────────────────────────────── */

void cohort_call_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_FNC]    = lower_fnc;
    tbl[AST_IDX]    = lower_idx;
    tbl[AST_ASSIGN] = lower_assign;
    tbl[AST_SCAN]   = lower_scan;
    tbl[AST_SWAP]   = lower_swap;
}
