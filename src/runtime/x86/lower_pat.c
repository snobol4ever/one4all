/*
 * lower_pat.c — Pattern-context expression lowering (SR-7)
 *
 * Provides lower_pat_expr(ctx, e): lowers an AST node in pattern context,
 * emitting SM_PAT_* opcodes instead of value-context SM opcodes.
 *
 * Also provides sm_pat_capture_fn_arg_names(): shared helper for
 * cohort_capture.c (SR-8) and the CAPT_* handlers here.
 *
 * Called from:
 *   sm_lower.c  — lower_stmt (SNOBOL4 statement pattern field)
 *   cohort_seq.c — AST_CAT/SEQ value-context defer detection
 *   cohort_pat_prim.c — value-context pat-prim handlers
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"

/* Extract argument names from a *fn(var,var,...) AST_FNC subtree for
 * SM_PAT_CAPTURE_FN.  Returns a GC-lifetime '\t'-separated name list, or
 * NULL if any arg is not a plain AST_VAR (callers fall back to args-on-stack). */
const char *sm_pat_capture_fn_arg_names(const AST_t *fnc)
{
    if (!fnc || fnc->nchildren <= 0) return NULL;
    size_t total_len = 0;
    for (int i = 0; i < fnc->nchildren; i++) {
        const AST_t *c = fnc->children[i];
        if (!c || c->kind != AST_VAR || !c->sval) return NULL;
        total_len += strlen(c->sval) + 1;
    }
    char *buf = (char *)GC_MALLOC(total_len);
    if (!buf) return NULL;
    char *p = buf;
    for (int i = 0; i < fnc->nchildren; i++) {
        const char *name = fnc->children[i]->sval;
        size_t n = strlen(name);
        memcpy(p, name, n);
        p += n;
        *p++ = (i + 1 < fnc->nchildren) ? '\t' : '\0';
    }
    return buf;
}

void lower_pat_expr(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    LabelTable *labtab = &c->labtab;
    (void)labtab;
    if (!e) return;

    switch (e->kind) {

    case AST_QLIT:
        sm_emit_s(p, SM_PAT_LIT, e->sval ? e->sval : "");
        return;

    case AST_VAR:
        sm_emit_s(p, SM_PUSH_VAR, e->sval);
        sm_emit(p, SM_PAT_DEREF);
        return;

    case AST_ARB:      sm_emit(p, SM_PAT_ARB);     return;
    case AST_REM:      sm_emit(p, SM_PAT_REM);      return;
    case AST_FAIL:     sm_emit(p, SM_PAT_FAIL);     return;
    case AST_SUCCEED:  sm_emit(p, SM_PAT_SUCCEED);  return;
    case AST_FENCE:
        if (e->nchildren > 0) {
            lower_pat_expr(c, e->children[0]);
            sm_emit(p, SM_PAT_FENCE1);
        } else {
            sm_emit(p, SM_PAT_FENCE);
        }
        return;
    case AST_ABORT:    sm_emit(p, SM_PAT_ABORT);    return;
    case AST_BAL:      sm_emit(p, SM_PAT_BAL);      return;

    case AST_ANY:    lower_expr(c, CH0(e)); sm_emit(p, SM_PAT_ANY);    return;
    case AST_NOTANY: lower_expr(c, CH0(e)); sm_emit(p, SM_PAT_NOTANY); return;
    case AST_SPAN:   lower_expr(c, CH0(e)); sm_emit(p, SM_PAT_SPAN);   return;
    case AST_BREAK:  lower_expr(c, CH0(e)); sm_emit(p, SM_PAT_BREAK);  return;
    case AST_BREAKX: lower_expr(c, CH0(e)); sm_emit(p, SM_PAT_BREAK);  return;
    case AST_LEN:    lower_expr(c, CH0(e)); sm_emit(p, SM_PAT_LEN);    return;
    case AST_POS:    lower_expr(c, CH0(e)); sm_emit(p, SM_PAT_POS);    return;
    case AST_RPOS:   lower_expr(c, CH0(e)); sm_emit(p, SM_PAT_RPOS);   return;
    case AST_TAB:    lower_expr(c, CH0(e)); sm_emit(p, SM_PAT_TAB);    return;
    case AST_RTAB:   lower_expr(c, CH0(e)); sm_emit(p, SM_PAT_RTAB);   return;
    case AST_ARBNO:  { SM_Program *_p = p; LOWER1_PAT(SM_PAT_ARBNO); }

    case AST_SEQ:
    case AST_CAT:
        for (int i = 0; i < e->nchildren; i++) lower_pat_expr(c, e->children[i]);
        for (int i = 1; i < e->nchildren; i++) sm_emit(p, SM_PAT_CAT);
        return;

    case AST_ALT:
        for (int i = 0; i < e->nchildren; i++) lower_pat_expr(c, e->children[i]);
        for (int i = 1; i < e->nchildren; i++) sm_emit(p, SM_PAT_ALT);
        return;

    case AST_CAPT_COND_ASGN:
        /* child[0] = sub-pattern, child[1] = variable; a[1].i=0 → conditional (.V) */
        lower_pat_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
        if (e->nchildren > 1 && e->children[1]) {
            AST_t *var_expr = e->children[1];
            if (var_expr->kind == AST_DEFER
                    && var_expr->nchildren > 0
                    && var_expr->children[0]
                    && var_expr->children[0]->kind == AST_FNC
                    && var_expr->children[0]->sval) {
                const AST_t *fnc = var_expr->children[0];
                const char *namelist = sm_pat_capture_fn_arg_names(fnc);
                if (namelist || fnc->nchildren == 0) {
                    int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN, fnc->sval);
                    p->instrs[idx].a[1].i = 0;
                    p->instrs[idx].a[2].s = namelist;
                } else {
                    for (int i = 0; i < fnc->nchildren; i++) {
                        AST_t *arg = fnc->children[i];
                        if (arg && arg->kind == AST_QLIT)
                            lower_expr(c, arg);
                        else if (arg) {
                            int skip_jump = sm_emit_i(p, SM_JUMP, 0);
                            int entry_pc  = sm_label(p);
                            lower_expr(c, arg);
                            sm_emit(p, SM_RETURN);
                            int skip_lbl  = sm_label(p);
                            sm_patch_jump(p, skip_jump, skip_lbl);
                            sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
                        } else
                            lower_expr(c, arg);
                    }
                    int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN_ARGS, fnc->sval);
                    p->instrs[idx].a[1].i = 0;
                    p->instrs[idx].a[2].i = fnc->nchildren;
                }
            } else {
                int idx = sm_emit_s(p, SM_PAT_CAPTURE, var_expr->sval);
                p->instrs[idx].a[1].i = 0;
            }
        }
        return;

    case AST_CAPT_IMMED_ASGN:
        /* a[1].i=1 → immediate ($V) */
        lower_pat_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
        if (e->nchildren > 1 && e->children[1]) {
            AST_t *var_expr = e->children[1];
            if (var_expr->kind == AST_DEFER
                    && var_expr->nchildren > 0
                    && var_expr->children[0]
                    && var_expr->children[0]->kind == AST_FNC
                    && var_expr->children[0]->sval) {
                const AST_t *fnc = var_expr->children[0];
                const char *namelist = sm_pat_capture_fn_arg_names(fnc);
                if (namelist || fnc->nchildren == 0) {
                    int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN, fnc->sval);
                    p->instrs[idx].a[1].i = 1;
                    p->instrs[idx].a[2].s = namelist;
                } else {
                    for (int i = 0; i < fnc->nchildren; i++) {
                        AST_t *arg = fnc->children[i];
                        if (arg && arg->kind == AST_QLIT)
                            lower_expr(c, arg);
                        else if (arg) {
                            int skip_jump = sm_emit_i(p, SM_JUMP, 0);
                            int entry_pc  = sm_label(p);
                            lower_expr(c, arg);
                            sm_emit(p, SM_RETURN);
                            int skip_lbl  = sm_label(p);
                            sm_patch_jump(p, skip_jump, skip_lbl);
                            sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
                        } else
                            lower_expr(c, arg);
                    }
                    int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN_ARGS, fnc->sval);
                    p->instrs[idx].a[1].i = 1;
                    p->instrs[idx].a[2].i = fnc->nchildren;
                }
            } else {
                int idx = sm_emit_s(p, SM_PAT_CAPTURE, var_expr->sval);
                p->instrs[idx].a[1].i = 1;
            }
        }
        return;

    case AST_CAPT_CURSOR:
        /* Unary @var: child[0] IS the variable name node (no sub-pattern).
         * Binary X@V: child[0] = sub-pattern, child[1] = variable. */
        if (e->nchildren == 1) {
            const char *vname = (e->children[0] && e->children[0]->sval)
                                 ? e->children[0]->sval : "";
            sm_emit(p, SM_PAT_EPS);
            int idx = sm_emit_s(p, SM_PAT_CAPTURE, vname);
            p->instrs[idx].a[1].i = 2;
        } else {
            lower_pat_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
            if (e->nchildren > 1 && e->children[1]) {
                int idx = sm_emit_s(p, SM_PAT_CAPTURE, e->children[1]->sval);
                p->instrs[idx].a[1].i = 2;
            }
        }
        return;

    case AST_DEFER: {
        AST_t *ch = e->nchildren > 0 ? e->children[0] : NULL;
        /* *fn() in pattern — invoke fn at match time via SM_PAT_USERCALL so
         * FAIL propagates as pattern FAIL and fn runs per match position. */
        if (ch && ch->kind == AST_FNC && ch->sval) {
            if (ch->nchildren == 0) {
                int idx = sm_emit_s(p, SM_PAT_USERCALL, ch->sval);
                p->instrs[idx].a[2].s = NULL;
            } else {
                for (int i = 0; i < ch->nchildren; i++) {
                    AST_t *arg = ch->children[i];
                    if (arg && arg->kind == AST_QLIT)
                        lower_expr(c, arg);
                    else if (arg) {
                        int skip_jump = sm_emit_i(p, SM_JUMP, 0);
                        int entry_pc  = sm_label(p);
                        lower_expr(c, arg);
                        sm_emit(p, SM_RETURN);
                        int skip_lbl  = sm_label(p);
                        sm_patch_jump(p, skip_jump, skip_lbl);
                        sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
                    } else
                        lower_expr(c, arg);
                }
                int idx = sm_emit_s(p, SM_PAT_USERCALL_ARGS, ch->sval);
                p->instrs[idx].a[1].i = ch->nchildren;
            }
            return;
        }
        /* *var — emit SM_PAT_REFNAME so the name (not the current value)
         * reaches the engine at match time, enabling self-recursive patterns. */
        if (ch && ch->kind == AST_VAR && ch->sval) {
            sm_emit_s(p, SM_PAT_REFNAME, ch->sval);
            return;
        }
        lower_expr(c, ch);
        sm_emit(p, SM_PAT_DEREF);
        return;
    }

    case AST_FNC:
        lower_expr(c, e);
        sm_emit(p, SM_PAT_DEREF);
        return;

    default:
        lower_expr(c, e);
        sm_emit(p, SM_PAT_DEREF);
        return;
    }
}
