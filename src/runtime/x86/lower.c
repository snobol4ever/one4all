/*
 * lower.c — IR → SM_Program compiler pass (all cohorts merged, SR-14 complete)
 *
 * Pipeline position:
 *   Six frontends → shared AST → lower() → SM_Program → four execution modes
 *   (IR-interp, SM-interp, JIT-exec, native-emit).  This file is the
 *   single translation unit that converts every AST kind into SM instructions.
 *
 * Three phases:
 *   1. lower_proc_skeletons — emit JUMP/label/RETURN stubs for all procedures
 *      and Prolog predicates so forward calls resolve before bodies are lowered.
 *   2. lower_stmt loop — walk CODE_t statement list; lower_stmt() dispatches
 *      to pattern, assignment, or expression paths; lower_expr() dispatches
 *      to a per-kind handler via g_handlers[].
 *   3. SM_HALT + labtab_resolve — patch all forward GOTO targets; report any
 *      AST kinds that reached lower_unhandled() (SR-14 diagnostic).
 *
 * LowerCtx lifecycle:
 *   Allocated on the stack in lower().  Passed by pointer to every sub-function.
 *   Contains: SM_Program *p, LabelTable labtab, expression_body_lowering flag,
 *   IcnScope *expression_scope (per-proc during proc-body emit), and the
 *   unhandled_kinds[] bitset (SR-14).
 *
 * Handler table (g_handlers[]):
 *   Every AST_e value has an explicit entry.  lower_unhandled is the default;
 *   cohort sections below overwrite their slices via init_handlers().
 *   SR-14: adding a new AST_e requires an explicit handler-table entry — either
 *   write a handler or leave lower_unhandled and document the rung that will fix it.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#define IR_DEFINE_NAMES   /* enable ast_e_name[] in ast.h */

#include "lower.h"
#include "lower_ctx.h"
#include "sm_prog.h"
#include "sm_interp.h"

#include "../../frontend/snobol4/scrip_cc.h"
#include "../ast/ast.h"
#include "../../runtime/common/ast_clone.h"
#include "../../runtime/interp/coro_runtime.h"
#include "../../runtime/interp/pl_runtime.h"
#include "../../frontend/icon/icon_lex.h"   /* TK_AUG* enum for lower_augop */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <gc/gc.h>
#include "snobol4.h"

/* Forward declarations needed before cohort sections. */
void lower_expr    (LowerCtx *c, const AST_t *e);
void lower_pat_expr(LowerCtx *c, const AST_t *e);
void lower_stmt    (LowerCtx *c, const STMT_t *s);

/*============================================================================*/
/* lower_literal — QLIT ILIT FLIT CSET NUL (SR-4) */
/*============================================================================*/

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


/*============================================================================*/
/* lower_ref — VAR KEYWORD INDIRECT DEFER (SR-5) */
/*============================================================================*/

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


/*============================================================================*/
/* lower_arith — INTERROGATE NAME MNS PLS ADD..POW (SR-6) */
/*============================================================================*/

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


/*============================================================================*/
/* lower_seq — VLIST CAT SEQ ALT OPSYN (SR-7) */
/*============================================================================*/

static void lower_vlist(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren == 0) { sm_emit(p, SM_PUSH_NULL); return; }
    if (e->nchildren == 1) { lower_expr(c, e->children[0]); return; }
    int njs = e->nchildren - 1;
    int *jumps = (int *)malloc((size_t)njs * sizeof(int));
    for (int i = 0; i < e->nchildren; i++) {
        lower_expr(c, e->children[i]);
        if (i < e->nchildren - 1) {
            jumps[i] = sm_emit_i(p, SM_JUMP_S, 0);
            sm_emit(p, SM_VOID_POP);
        }
    }
    int done = sm_label(p);
    for (int i = 0; i < njs; i++) sm_patch_jump(p, jumps[i], done);
    free(jumps);
}

static void lower_cat_seq(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    /* If any child is AST_DEFER, lower as pattern-context concatenation. */
    int has_defer = 0;
    for (int j = 0; j < e->nchildren; j++) {
        const AST_t *cj = e->children[j];
        if (cj && cj->kind == AST_DEFER) { has_defer = 1; break; }
    }
    if (has_defer) {
        for (int i = 0; i < e->nchildren; i++) lower_pat_expr(c, e->children[i]);
        for (int i = 1; i < e->nchildren; i++) sm_emit(p, SM_PAT_CAT);
    } else {
        for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
        for (int i = 1; i < e->nchildren; i++) sm_emit(p, SM_CONCAT);
    }
}

static void lower_alt(LowerCtx *c, const AST_t *e)
{
    lower_pat_expr(c, e);
}

static void lower_opsyn(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    /* sval is mangled "BIATFN(@)" (op char between parens)
     * or bare "BARFN" / "AROWFN" for unary ops. */
    const char *raw = e->sval ? e->sval : "&";
    const char *op = raw;
    static char op_buf[4];
    const char *lp = strchr(raw, '(');
    if (lp && lp[1] && lp[2] == ')') { op_buf[0] = lp[1]; op_buf[1] = '\0'; op = op_buf; }
    else if (strcmp(raw, "BARFN")  == 0) { op = "|"; }
    else if (strcmp(raw, "AROWFN") == 0) { op = "^"; }
    for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
    sm_emit_si(p, SM_CALL_FN, op, (int64_t)e->nchildren);
}


/*============================================================================*/
/* lower_pat — pattern-context lowering (SR-7) */
/*============================================================================*/

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

/*============================================================================*/
/* lower_pat_prim — 14 pattern primitive kinds (SR-7) */
/*============================================================================*/

/* All pat-prim value-context handlers delegate to lower_pat_expr. */
static void lower_pat_prim_val(LowerCtx *c, const AST_t *e)
{
    lower_pat_expr(c, e);
}


/*============================================================================*/
/* lower_capture — CAPT_COND_ASGN CAPT_IMMED CAPT_CURSOR (SR-8) */
/*============================================================================*/

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


/*============================================================================*/
/* lower_call — FNC IDX ASSIGN SCAN SWAP (SR-8) */
/*============================================================================*/

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


/*============================================================================*/
/* lower_icn_relop — numeric/string relops x12 (SR-9) */
/*============================================================================*/

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


/*============================================================================*/
/* lower_icn_cset — CSET_COMPL UNION DIFF INTER LCONCAT (SR-9) */
/*============================================================================*/

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


/*============================================================================*/
/* lower_icn_unary — NONNULL NULL NOT SIZE RANDOM IDENTICAL AUGOP (SR-9) */
/*============================================================================*/

/* icon_lex.h is included here (top of file) rather than mid-function
 * as in the original lower.c AST_AUGOP case. */

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


/*============================================================================*/
/* lower_icn_ctrl — SEQ_EXPR WHILE..REPEAT IF CASE RETURN LOOP_* (SR-10) */
/*============================================================================*/

/* ── AST_SEQ_EXPR ────────────────────────────────────────────── */

static void lower_seq_expr(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren == 0) { sm_emit(p, SM_PUSH_NULL); return; }
    for (int i = 0; i < e->nchildren; i++) {
        lower_expr(c, e->children[i]);
        if (i < e->nchildren - 1) sm_emit(p, SM_VOID_POP);
    }
}

/* ── AST_IF ──────────────────────────────────────────────────── */

static void lower_if(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(c, e->children[0]);              /* condition */
    int jf = sm_emit_i(p, SM_JUMP_F, 0);       /* jump-if-fail to else */
    /* Condition result left on stack; drain before entering then-body. */
    sm_emit(p, SM_VOID_POP);
    if (e->nchildren > 1) lower_expr(c, e->children[1]);
    else                  sm_emit(p, SM_PUSH_NULL);
    int jend = sm_emit_i(p, SM_JUMP, 0);
    int else_lbl = sm_label(p);
    sm_patch_jump(p, jf, else_lbl);
    /* Drain condition FAILDESCR on the else path too. */
    sm_emit(p, SM_VOID_POP);
    if (e->nchildren > 2) lower_expr(c, e->children[2]);
    else                  sm_emit(p, SM_PUSH_NULL);
    int end_lbl = sm_label(p);
    sm_patch_jump(p, jend, end_lbl);
}

/* ── AST_WHILE ───────────────────────────────────────────────── */

static void lower_while(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    int top_lbl = sm_label(p);
    if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(c, e->children[0]);
    int jf = sm_emit_i(p, SM_JUMP_F, 0);
    sm_emit(p, SM_VOID_POP);
    if (e->nchildren > 1) { lower_expr(c, e->children[1]); sm_emit(p, SM_VOID_POP); }
    sm_emit_i(p, SM_JUMP, top_lbl);
    int end_lbl = sm_label(p);
    sm_patch_jump(p, jf, end_lbl);
    sm_emit(p, SM_VOID_POP);   /* FAILDESCR left on stack by JUMP_F */
    sm_emit(p, SM_PUSH_NULL);
}

/* ── AST_UNTIL ───────────────────────────────────────────────── */

static void lower_until(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    int top_lbl = sm_label(p);
    if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(c, e->children[0]);
    int js = sm_emit_i(p, SM_JUMP_S, 0);
    sm_emit(p, SM_VOID_POP);
    if (e->nchildren > 1) { lower_expr(c, e->children[1]); sm_emit(p, SM_VOID_POP); }
    sm_emit_i(p, SM_JUMP, top_lbl);
    int end_lbl = sm_label(p);
    sm_patch_jump(p, js, end_lbl);
    sm_emit(p, SM_VOID_POP);
    sm_emit(p, SM_PUSH_NULL);
}

/* ── AST_REPEAT ──────────────────────────────────────────────── */

static void lower_repeat(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    int top_lbl = sm_label(p);
    if (e->nchildren > 0) { lower_expr(c, e->children[0]); sm_emit(p, SM_VOID_POP); }
    sm_emit_i(p, SM_JUMP, top_lbl);
    sm_emit(p, SM_PUSH_NULL);
}

/* ── AST_LOOP_BREAK ──────────────────────────────────────────── */

static void lower_loop_break(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren > 0) lower_expr(c, e->children[0]);
    else sm_emit(p, SM_PUSH_NULL);
    /* SM_JUMP to self+1 signals break to the sm_interp loop handler. */
    sm_emit_i(p, SM_JUMP, p->count + 1);
}

/* ── AST_LOOP_NEXT ───────────────────────────────────────────── */

static void lower_loop_next(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    (void)e;
    sm_emit(p, SM_PUSH_NULL);
}

/* ── AST_RETURN ──────────────────────────────────────────────── */

static void lower_return(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren > 0) lower_expr(c, e->children[0]);
    else sm_emit(p, SM_PUSH_NULL);
    sm_emit(p, SM_RETURN);
}

/* ── AST_PROC_FAIL ───────────────────────────────────────────── */

static void lower_proc_fail(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    (void)e;
    sm_emit(p, SM_PUSH_NULL);
    sm_emit(p, SM_FRETURN);
}

/* ── AST_CASE ────────────────────────────────────────────────── */
/*
 * case E of { ... } — Icon pair layout and Raku triple layout.
 *
 * Icon pair layout: [topic, val0, body0, val1, body1, ..., [default]]
 *   Topic stored in NV temp __case_topic__; each arm compares via
 *   ICN_CASE_EQ, on match evaluates body and jumps to end.
 *
 * Raku triple layout: (nchildren-1) % 3 == 0 and child[1] is AST_ILIT
 *   or AST_NUL. Emits topic + per-arm (cmp_kind, val, body) expression
 *   chunks then SM_BB_PUMP_CASE.
 */
static void lower_case(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }

    int is_raku_layout = (e->nchildren >= 4 && (e->nchildren - 1) % 3 == 0 &&
        e->children[1] && (e->children[1]->kind == AST_ILIT || e->children[1]->kind == AST_NUL));

    if (!is_raku_layout) {
        int nc = e->nchildren - 1;
        int has_default = (nc % 2 != 0);
        int npairs = nc / 2;
        lower_expr(c, e->children[0]);
        sm_emit_s(p, SM_STORE_VAR, "__case_topic__");
        sm_emit(p, SM_VOID_POP);
        int end_jumps[64]; int nend = 0;
        for (int pair = 0; pair < npairs && pair < 32; pair++) {
            AST_t *val  = e->children[1 + pair*2];
            AST_t *body = e->children[2 + pair*2];
            sm_emit_s(p, SM_PUSH_VAR, "__case_topic__");
            lower_expr(c, val);
            sm_emit_si(p, SM_CALL_FN, "ICN_CASE_EQ", 2);
            int jf = sm_emit_i(p, SM_JUMP_F, 0);
            sm_emit(p, SM_VOID_POP);
            lower_expr(c, body);
            if (nend < 64) end_jumps[nend++] = sm_emit_i(p, SM_JUMP, 0);
            int next_lbl = sm_label(p);
            sm_patch_jump(p, jf, next_lbl);
            sm_emit(p, SM_VOID_POP);
        }
        if (has_default) lower_expr(c, e->children[e->nchildren - 1]);
        else             sm_emit(p, SM_PUSH_NULL);
        int end_lbl = sm_label(p);
        for (int j = 0; j < nend; j++) sm_patch_jump(p, end_jumps[j], end_lbl);
        return;
    }

    /* Raku triple layout */
    #define EMIT_CHUNK_OF(child_expr) do {                              \
        int _skip = sm_emit_i(p, SM_JUMP, 0);                           \
        int _entry = sm_label(p);                                       \
        lower_expr(c, (child_expr));                                    \
        sm_emit(p, SM_RETURN);                                          \
        int _after = sm_label(p);                                       \
        sm_patch_jump(p, _skip, _after);                                \
        sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)_entry, 0);         \
    } while (0)

    int total_triples = (e->nchildren - 1) / 3;
    int has_default   = 0;
    int default_idx   = -1;
    if (total_triples > 0) {
        int last_i = 1 + (total_triples - 1) * 3;
        AST_t *last_cmp = e->children[last_i];
        if (last_cmp && last_cmp->kind == AST_NUL) { has_default = 1; default_idx = total_triples - 1; }
    }
    int ncases = total_triples - (has_default ? 1 : 0);

    EMIT_CHUNK_OF(e->children[0]);
    for (int t = 0; t < total_triples; t++) {
        if (t == default_idx) continue;
        int base = 1 + t * 3;
        AST_t *cmpnode = e->children[base];
        int cmp_kind = (cmpnode && cmpnode->kind == AST_ILIT) ? (int)cmpnode->ival : (int)AST_EQ;
        sm_emit_i(p, SM_PUSH_LIT_I, (int64_t)cmp_kind);
        EMIT_CHUNK_OF(e->children[base + 1]);
        EMIT_CHUNK_OF(e->children[base + 2]);
    }
    if (has_default) { int base = 1 + default_idx * 3; EMIT_CHUNK_OF(e->children[base + 2]); }
    sm_emit_ii(p, SM_BB_PUMP_CASE, (int64_t)ncases, (int64_t)has_default);
    #undef EMIT_CHUNK_OF
}

/* ── Registration ─────────────────────────────────────────────── */


/*============================================================================*/
/* lower_icn_data — MAKELIST RECORD FIELD GLOBAL INITIAL (SR-10) */
/*============================================================================*/

/* ── AST_MAKELIST ────────────────────────────────────────────── */

static void lower_makelist(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
    sm_emit_si(p, SM_CALL_FN, "MAKELIST", (int64_t)e->nchildren);
}

/* ── AST_RECORD ──────────────────────────────────────────────── */

static void lower_record(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    sm_emit_s(p, SM_PUSH_LIT_S, e->sval ? e->sval : "");
    for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
    sm_emit_si(p, SM_CALL_FN, "RECORD_MAKE", (int64_t)e->nchildren + 1);
}

/* ── AST_FIELD ───────────────────────────────────────────────── */

static void lower_field(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
    sm_emit_s(p, SM_PUSH_LIT_S, e->sval ? e->sval : "");
    sm_emit_si(p, SM_CALL_FN, "FIELD_GET", 2);
}

/* ── AST_GLOBAL ──────────────────────────────────────────────── */

static void lower_global(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    (void)e;
    sm_emit(p, SM_PUSH_NULL);
}

/* ── AST_INITIAL ─────────────────────────────────────────────── */
/*
 * Icon `initial { ... }` runs its body the FIRST time the enclosing
 * procedure is called, then skips on every subsequent call.  In SM
 * mode the once-flag persists across calls as a per-AST NV sentinel.
 *
 * Shape:
 *   PUSH_VAR __initial_<ptr>__   ; null on first call
 *   CALL_FN  NONNULL 1           ; FAIL if null, succeed if set
 *   JUMP_S   L_skip              ; sentinel set → skip body
 *   VOID_POP                     ; drop FAILDESCR from NONNULL
 *   [body assignments]
 *   PUSH_LIT_I 1
 *   STORE_VAR __initial_<ptr>__  ; sentinel := 1
 *   VOID_POP
 *   JUMP     L_done
 * L_skip:
 *   VOID_POP                     ; drop sentinel value
 * L_done:
 *   PUSH_NULL
 */
static void lower_initial(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    char sentinel[64];
    snprintf(sentinel, sizeof(sentinel), "__initial_%lx__",
             (unsigned long)(uintptr_t)e);

    sm_emit_s(p, SM_PUSH_VAR, sentinel);
    sm_emit_si(p, SM_CALL_FN, "NONNULL", 1);
    int skip_jump = sm_emit_i(p, SM_JUMP_S, 0);

    sm_emit(p, SM_VOID_POP);

    for (int i = 0; i < e->nchildren; i++) {
        if (!e->children[i]) continue;
        lower_expr(c, e->children[i]);
        sm_emit(p, SM_VOID_POP);
    }

    sm_emit_i(p, SM_PUSH_LIT_I, 1);
    sm_emit_s(p, SM_STORE_VAR, sentinel);
    sm_emit(p, SM_VOID_POP);

    int done_jump = sm_emit_i(p, SM_JUMP, 0);

    int skip_pc = sm_label(p);
    sm_patch_jump(p, skip_jump, skip_pc);
    sm_emit(p, SM_VOID_POP);

    int done_pc = sm_label(p);
    sm_patch_jump(p, done_jump, done_pc);

    sm_emit(p, SM_PUSH_NULL);
}

/* ── Registration ─────────────────────────────────────────────── */


/*============================================================================*/
/* lower_icn_sect — SECTION SECTION_PLUS SECTION_MINUS BANG_BINARY (SR-10) */
/*============================================================================*/

/* ── AST_SECTION ─────────────────────────────────────────────── */

static void lower_section(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren >= 3) {
        lower_expr(c, e->children[0]);
        lower_expr(c, e->children[1]);
        lower_expr(c, e->children[2]);
        sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_RANGE", 3);
    } else {
        sm_emit(p, SM_PUSH_NULL);
    }
}

/* ── AST_SECTION_PLUS ────────────────────────────────────────── */

static void lower_section_plus(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren >= 3) {
        lower_expr(c, e->children[0]);
        lower_expr(c, e->children[1]);
        lower_expr(c, e->children[2]);
        sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_PLUS", 3);
    } else {
        sm_emit(p, SM_PUSH_NULL);
    }
}

/* ── AST_SECTION_MINUS ───────────────────────────────────────── */

static void lower_section_minus(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren >= 3) {
        lower_expr(c, e->children[0]);
        lower_expr(c, e->children[1]);
        lower_expr(c, e->children[2]);
        sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_MINUS", 3);
    } else {
        sm_emit(p, SM_PUSH_NULL);
    }
}

/* ── AST_BANG_BINARY ─────────────────────────────────────────── */

static void lower_bang_binary(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)e));
}

/* ── Registration ─────────────────────────────────────────────── */


/*============================================================================*/
/* lower_icn_gen — SUSPEND TO TO_BY LIMIT ALTERNATE ITERATE EVERY (SR-11) */
/*============================================================================*/

/* ── AST_TO ──────────────────────────────────────────────────── */
/*
 * Emits an SM coroutine that yields integers lo..hi (inclusive).
 * glocal[0]=lo, glocal[1]=hi, glocal[2]=cur.
 * Shape: JUMP skip / entry: RESUME / init glocals / loop: cur>hi?exit /
 *        LOAD cur / SUSPEND / cur++ / JUMP loop / exit: NULL RETURN /
 *        skip: PUSH_EXPRESSION entry / BB_PUMP_SM
 */
static void lower_to(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    const AST_t *lo_expr = (e->nchildren > 0) ? e->children[0] : NULL;
    const AST_t *hi_expr = (e->nchildren > 1) ? e->children[1] : NULL;
    int skip_jump = sm_emit_i(p, SM_JUMP, 0);
    int entry_pc  = sm_label(p);
    sm_emit(p, SM_RESUME);
    if (lo_expr) lower_expr(c, lo_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 0); sm_emit(p, SM_VOID_POP);
    if (hi_expr) lower_expr(c, hi_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 1); sm_emit(p, SM_VOID_POP);
    sm_emit_i(p, SM_LOAD_GLOCAL, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 2); sm_emit(p, SM_VOID_POP);
    int loop_pc = sm_label(p);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 1);
    sm_emit(p, SM_ICMP_GT);
    int exit_jump = sm_emit_i(p, SM_JUMP_S, 0);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2);
    sm_emit(p, SM_SUSPEND);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_INCR, 1);
    sm_emit_i(p, SM_STORE_GLOCAL, 2); sm_emit(p, SM_VOID_POP);
    sm_emit_i(p, SM_JUMP, loop_pc);
    int exit_pc = sm_label(p);
    sm_patch_jump(p, exit_jump, exit_pc);
    sm_emit(p, SM_PUSH_NULL); sm_emit(p, SM_RETURN);
    int skip_pc = sm_label(p);
    sm_patch_jump(p, skip_jump, skip_pc);
    sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
    sm_emit(p, SM_BB_PUMP_SM);
}

/* ── AST_TO_BY ───────────────────────────────────────────────── */
/*
 * Like AST_TO but with an explicit step.
 * glocal[0]=lo, glocal[1]=hi, glocal[2]=cur, glocal[3]=step.
 * step>0: exit when cur>hi; step<0: exit when cur<hi.
 */
static void lower_to_by(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    const AST_t *lo_expr   = (e->nchildren > 0) ? e->children[0] : NULL;
    const AST_t *hi_expr   = (e->nchildren > 1) ? e->children[1] : NULL;
    const AST_t *step_expr = (e->nchildren > 2) ? e->children[2] : NULL;
    int skip_jump = sm_emit_i(p, SM_JUMP, 0);
    int entry_pc  = sm_label(p);
    sm_emit(p, SM_RESUME);
    if (lo_expr) lower_expr(c, lo_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 0); sm_emit(p, SM_VOID_POP);
    if (hi_expr) lower_expr(c, hi_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 1); sm_emit(p, SM_VOID_POP);
    if (step_expr) lower_expr(c, step_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 1);
    sm_emit_i(p, SM_STORE_GLOCAL, 3); sm_emit(p, SM_VOID_POP);
    sm_emit_i(p, SM_LOAD_GLOCAL, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 2); sm_emit(p, SM_VOID_POP);
    int loop_pc = sm_label(p);
    sm_emit_i(p, SM_LOAD_GLOCAL, 3); sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit(p, SM_ICMP_LT);
    int neg_branch = sm_emit_i(p, SM_JUMP_S, 0);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 1);
    sm_emit(p, SM_ICMP_GT);
    int exit_jump_pos = sm_emit_i(p, SM_JUMP_S, 0);
    int body_jump = sm_emit_i(p, SM_JUMP, 0);
    int neg_pc = sm_label(p);
    sm_patch_jump(p, neg_branch, neg_pc);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 1);
    sm_emit(p, SM_ICMP_LT);
    int exit_jump_neg = sm_emit_i(p, SM_JUMP_S, 0);
    int body_pc = sm_label(p);
    sm_patch_jump(p, body_jump, body_pc);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2);
    sm_emit(p, SM_SUSPEND);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 3);
    sm_emit(p, SM_ADD);
    sm_emit_i(p, SM_STORE_GLOCAL, 2); sm_emit(p, SM_VOID_POP);
    sm_emit_i(p, SM_JUMP, loop_pc);
    int exit_pc = sm_label(p);
    sm_patch_jump(p, exit_jump_pos, exit_pc);
    sm_patch_jump(p, exit_jump_neg, exit_pc);
    sm_emit(p, SM_PUSH_NULL); sm_emit(p, SM_RETURN);
    int skip_pc = sm_label(p);
    sm_patch_jump(p, skip_jump, skip_pc);
    sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
    sm_emit(p, SM_BB_PUMP_SM);
}

/* ── AST_EVERY ───────────────────────────────────────────────── */

static void lower_every(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    int every_id = every_table_register((AST_t *)e);
    sm_emit_i(p, SM_BB_PUMP_EVERY, (int64_t)every_id);
}

/* ── AST_SUSPEND ─────────────────────────────────────────────── */
/*
 * Yield value expression; run optional do-clause on resume.
 * If value fails, skip yield+do-clause and leave failed descriptor on stack.
 * On success, push NULVCL so the outer proc-body SM_VOID_POP balances.
 */
static void lower_suspend(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren > 0 && e->children[0]) lower_expr(c, e->children[0]);
    else sm_emit(p, SM_PUSH_NULL);
    int j_end = sm_emit_i(p, SM_JUMP_F, 0);
    sm_emit(p, SM_SUSPEND_VALUE);
    if (e->nchildren > 1 && e->children[1]) {
        lower_expr(c, e->children[1]);
        sm_emit(p, SM_VOID_POP);
    }
    sm_emit(p, SM_PUSH_NULL);
    int j_done = sm_emit_i(p, SM_JUMP, 0);
    int lbl_end = sm_label(p);
    sm_patch_jump(p, j_end, lbl_end);
    int lbl_finally = sm_label(p);
    sm_patch_jump(p, j_done, lbl_finally);
}

/* ── AST_ITERATE / AST_ALTERNATE / AST_LIMIT ─────────────────── */

static void lower_bb_pump_ast(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)e));
}

static void lower_limit(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    emit_push_expr(c, e);
    sm_emit(p, SM_BB_PUMP);
}

/* ── Registration ─────────────────────────────────────────────── */


/*============================================================================*/
/* lower_prolog — UNIFY CLAUSE CHOICE CUT TRAIL_MARK TRAIL_UNWIND (SR-11) */
/*============================================================================*/

/* ── AST_CHOICE ──────────────────────────────────────────────── */

static void lower_choice(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->sval) {
        const char *key = e->sval;
        int arity = 0;
        const char *sl = strrchr(key, '/');
        if (sl) arity = atoi(sl + 1);
        sm_emit_si(p, SM_BB_ONCE_PROC, key, (int64_t)arity);
    } else {
        emit_push_expr(c, e);
        sm_emit(p, SM_BB_ONCE);
    }
}

/* ── AST_CLAUSE / AST_CUT / AST_UNIFY / TRAIL_* ─────────────── */

static void lower_prolog_broker_child(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    /* Children of AST_CHOICE walked by the broker; rarely lowered standalone. */
    emit_push_expr(c, e);
    sm_emit(p, SM_BB_ONCE);
}

/* ── Registration ─────────────────────────────────────────────── */


/*============================================================================*/
/* lower_stmt — statement-level orchestration (SR-12) */
/*============================================================================*/

void lower_stmt(LowerCtx *c, const STMT_t *s)
{
    SM_Program *p = c->p;
    LabelTable *labtab = &c->labtab;

    /* Blank source line — emit nothing; the next non-blank stmt's SM_STNO fires. */
    if (!s->is_end
        && (!s->label || !s->label[0])
        && !s->subject && !s->pattern && !s->replacement
        && !s->goto_u && !s->goto_u_expr && !s->goto_s && !s->goto_s_expr && !s->goto_f && !s->goto_f_expr) {
        return;
    }

    /* Label emitted before SM_STNO so backward branches land on the STNO. */
    if (s->label && s->label[0]) {
        int lbl_idx = sm_label_named(p, s->label);
        labtab_define(labtab, s->label, lbl_idx);
        /* Tag DEFINE'd function entry labels; mode-3 emits a call prologue for them. */
        if (FUNC_IS_ENTRY_LABEL(s->label)) {
            p->instrs[p->count - 1].a[2].i = 1;
            sm_emit(p, SM_DEFINE_ENTRY);
        }
    }

    sm_emit_ii(p, SM_STNO, (int64_t)s->stno, (int64_t)s->lineno);

    if (s->is_end) { sm_emit(p, SM_HALT); return; }

    /* Icon proc/global/record defs are registered by polyglot_init; nothing to emit per-def. */
    if (s->lang == LANG_ICN) return;

    if (s->lang == LANG_PL) {
        if (s->subject && s->subject->kind == AST_CHOICE && s->subject->sval) {
            const char *key = s->subject->sval;
            int arity = 0;
            const char *sl = strrchr(key, '/');
            if (sl) arity = atoi(sl + 1);
            sm_emit_si(p, SM_BB_ONCE_PROC, key, (int64_t)arity);
        } else {
            if (s->subject) lower_expr(c, s->subject);
            else            sm_emit(p, SM_PUSH_NULL);
            sm_emit(p, SM_BB_ONCE);
        }
        goto emit_gotos;
    }

    /*
     * Pattern match statement:  subject  pattern  [= replacement]  :(goto)
     *
     * Pattern tree is emitted first so its parameterised-op args (e.g. SM_PAT_LEN)
     * are consumed from the value stack before the subject is pushed.
     */
    if (s->pattern) {
        lower_pat_expr(c, s->pattern);
        if (s->subject) lower_expr(c, s->subject);
        else            sm_emit(p, SM_PUSH_NULL);
        if (s->has_eq && s->replacement)
            lower_expr(c, s->replacement);
        else if (s->has_eq)
            sm_emit_si(p, SM_PUSH_LIT_S, "", 0);
        else
            sm_emit_i(p, SM_PUSH_LIT_I, 0);
        /* a[0].s = subject variable name for write-back; a[1].i = has_eq.
         * GC_strdup the sval — the IR may be freed before the SM_Program is used. */
        {
            const char *sname = NULL;
            if (s->subject && (s->subject->kind == AST_VAR || s->subject->kind == AST_KEYWORD))
                sname = s->subject->sval;
            sm_emit_si(p, SM_EXEC_STMT, sname, (int64_t)s->has_eq);
        }
        goto emit_gotos;
    }

    /*
     * Pure assignment or expression statement:
     *   label:  expr = value   :(goto)
     *   label:  expr           :(goto)
     */
    if (s->subject) {
        if (s->has_eq) {
            if (s->replacement) lower_expr(c, s->replacement);
            else                sm_emit(p, SM_PUSH_NULL);

            if (s->subject->kind == AST_VAR || s->subject->kind == AST_KEYWORD) {
                sm_emit_s(p, SM_STORE_VAR, s->subject->sval ? s->subject->sval : "");
            } else if (s->subject->kind == AST_INDIRECT) {
                lower_expr(c, s->subject->nchildren > 0 ? s->subject->children[0] : NULL);
                sm_emit_si(p, SM_CALL_FN, "ASGN_INDIR", 2);
            } else if (s->subject->kind == AST_IDX) {
                int nc = s->subject->nchildren;
                for (int ci = 0; ci < nc; ci++) lower_expr(c, s->subject->children[ci]);
                sm_emit_si(p, SM_CALL_FN, "IDX_SET", (int64_t)(nc + 1));
            } else if (s->subject->kind == AST_FNC && s->subject->sval) {
                if (s->subject->nchildren == 0) {
                    /* Zero-arg LHS: NRETURN path — fn returns DT_N, we write through. */
                    sm_emit_si(p, SM_CALL_FN, "NRETURN_ASGN", 1);
                    p->instrs[p->count - 1].a[1].s = GC_strdup(s->subject->sval);
                } else {
                    if (strcasecmp(s->subject->sval, "ITEM") == 0) {
                        int nc = s->subject->nchildren;
                        for (int ci = 0; ci < nc; ci++) lower_expr(c, s->subject->children[ci]);
                        sm_emit_si(p, SM_CALL_FN, "ITEM_SET", (int64_t)(nc + 1));
                    } else {
                        lower_expr(c, s->subject->nchildren > 0 ? s->subject->children[0] : NULL);
                        char _setname[256];
                        snprintf(_setname, sizeof(_setname), "%s_SET", s->subject->sval);
                        sm_emit_si(p, SM_CALL_FN, _setname, 2);
                    }
                }
            } else {
                lower_expr(c, s->subject);
                sm_emit_si(p, SM_CALL_FN, "ASGN", 2);
            }
        } else {
            /* Bare expression statement.
             * SNOBOL4 special case: bare RETURN / FRETURN / NRETURN with no
             * assignment is equivalent to :(RETURN) — emit the return opcode. */
            if (s->subject->kind == AST_VAR && s->subject->sval) {
                if (strcasecmp(s->subject->sval, "RETURN") == 0)  { sm_emit(p, SM_RETURN);  goto emit_gotos; }
                if (strcasecmp(s->subject->sval, "FRETURN") == 0) { sm_emit(p, SM_FRETURN); goto emit_gotos; }
                if (strcasecmp(s->subject->sval, "NRETURN") == 0) { sm_emit(p, SM_NRETURN); goto emit_gotos; }
            }
            lower_expr(c, s->subject);
            sm_emit(p, SM_VOID_POP);
        }
    }

emit_gotos: {
    if (!s->goto_u && !s->goto_u_expr && !s->goto_s && !s->goto_s_expr && !s->goto_f && !s->goto_f_expr) return;
    if (s->goto_u && s->goto_u[0]) { emit_goto(c, SM_JUMP, s->goto_u); return; }
    if (s->goto_u_expr) {
        sm_emit_s(p, SM_PUSH_LIT_S, "(computed-goto)");
        sm_emit(p, SM_JUMP_INDIR);
        return;
    }
    if (s->goto_s && s->goto_s[0]) emit_goto(c, SM_JUMP_S, s->goto_s);
    if (s->goto_f && s->goto_f[0]) emit_goto(c, SM_JUMP_F, s->goto_f);
    }
}


/*============================================================================*/
/* handler table + dispatcher + entry point                                   */
/*============================================================================*/

typedef void (*LowerHandler)(LowerCtx *c, const AST_t *e);

static LowerHandler g_handlers[AST_KIND_COUNT];
static int          g_handlers_initialized = 0;

static void init_handlers(void)
{
    /* SR-14: pre-fill every slot with lower_unhandled so no slot is NULL. */
    for (int i = 0; i < AST_KIND_COUNT; i++)
        g_handlers[i] = lower_unhandled;

    /* cohort_literal */
    g_handlers[AST_QLIT] = lower_qlit;
    g_handlers[AST_CSET] = lower_cset;
    g_handlers[AST_ILIT] = lower_ilit;
    g_handlers[AST_FLIT] = lower_flit;
    g_handlers[AST_NUL]  = lower_nul;

    /* cohort_ref */
    g_handlers[AST_VAR]      = lower_var;
    g_handlers[AST_KEYWORD]  = lower_keyword;
    g_handlers[AST_INDIRECT] = lower_indirect;
    g_handlers[AST_DEFER]    = lower_defer;

    /* cohort_arith */
    g_handlers[AST_INTERROGATE] = lower_interrogate;
    g_handlers[AST_NAME]        = lower_name;
    g_handlers[AST_MNS]         = lower_mns;
    g_handlers[AST_PLS]         = lower_pls;
    g_handlers[AST_ADD]         = lower_add;
    g_handlers[AST_SUB]         = lower_sub;
    g_handlers[AST_MUL]         = lower_mul;
    g_handlers[AST_DIV]         = lower_div;
    g_handlers[AST_MOD]         = lower_mod;
    g_handlers[AST_POW]         = lower_pow;

    /* cohort_seq */
    g_handlers[AST_VLIST] = lower_vlist;
    g_handlers[AST_CAT]   = lower_cat_seq;
    g_handlers[AST_SEQ]   = lower_cat_seq;
    g_handlers[AST_ALT]   = lower_alt;
    g_handlers[AST_OPSYN] = lower_opsyn;

    /* cohort_pat_prim — all delegate to lower_pat_expr */
    g_handlers[AST_ARB]     = lower_pat_prim_val;
    g_handlers[AST_ARBNO]   = lower_pat_prim_val;
    g_handlers[AST_POS]     = lower_pat_prim_val;
    g_handlers[AST_RPOS]    = lower_pat_prim_val;
    g_handlers[AST_ANY]     = lower_pat_prim_val;
    g_handlers[AST_NOTANY]  = lower_pat_prim_val;
    g_handlers[AST_SPAN]    = lower_pat_prim_val;
    g_handlers[AST_BREAK]   = lower_pat_prim_val;
    g_handlers[AST_BREAKX]  = lower_pat_prim_val;
    g_handlers[AST_LEN]     = lower_pat_prim_val;
    g_handlers[AST_TAB]     = lower_pat_prim_val;
    g_handlers[AST_RTAB]    = lower_pat_prim_val;
    g_handlers[AST_REM]     = lower_pat_prim_val;
    g_handlers[AST_FAIL]    = lower_pat_prim_val;
    g_handlers[AST_SUCCEED] = lower_pat_prim_val;
    g_handlers[AST_FENCE]   = lower_pat_prim_val;
    g_handlers[AST_ABORT]   = lower_pat_prim_val;
    g_handlers[AST_BAL]     = lower_pat_prim_val;

    /* cohort_capture */
    g_handlers[AST_CAPT_COND_ASGN]  = lower_capt_cond_asgn;
    g_handlers[AST_CAPT_IMMED_ASGN] = lower_capt_immed_asgn;
    g_handlers[AST_CAPT_CURSOR]     = lower_capt_cursor;

    /* cohort_call */
    g_handlers[AST_FNC]    = lower_fnc;
    g_handlers[AST_IDX]    = lower_idx;
    g_handlers[AST_ASSIGN] = lower_assign;
    g_handlers[AST_SCAN]   = lower_scan;
    g_handlers[AST_SWAP]   = lower_swap;

    /* cohort_icn_relop */
    g_handlers[AST_LT]  = lower_acomp; g_handlers[AST_LE]  = lower_acomp;
    g_handlers[AST_GT]  = lower_acomp; g_handlers[AST_GE]  = lower_acomp;
    g_handlers[AST_EQ]  = lower_acomp; g_handlers[AST_NE]  = lower_acomp;
    g_handlers[AST_LLT] = lower_lcomp; g_handlers[AST_LLE] = lower_lcomp;
    g_handlers[AST_LGT] = lower_lcomp; g_handlers[AST_LGE] = lower_lcomp;
    g_handlers[AST_LEQ] = lower_lcomp; g_handlers[AST_LNE] = lower_lcomp;

    /* cohort_icn_cset */
    g_handlers[AST_CSET_COMPL] = lower_cset_op;
    g_handlers[AST_CSET_UNION] = lower_cset_op;
    g_handlers[AST_CSET_DIFF]  = lower_cset_op;
    g_handlers[AST_CSET_INTER] = lower_cset_op;
    g_handlers[AST_LCONCAT]    = lower_lconcat;

    /* cohort_icn_unary */
    g_handlers[AST_NONNULL]   = lower_nonnull;
    g_handlers[AST_NULL]      = lower_null;
    g_handlers[AST_NOT]       = lower_not;
    g_handlers[AST_SIZE]      = lower_size;
    g_handlers[AST_RANDOM]    = lower_random;
    g_handlers[AST_IDENTICAL] = lower_identical;
    g_handlers[AST_AUGOP]     = lower_augop;

    /* cohort_icn_ctrl */
    g_handlers[AST_SEQ_EXPR]   = lower_seq_expr;
    g_handlers[AST_WHILE]      = lower_while;
    g_handlers[AST_UNTIL]      = lower_until;
    g_handlers[AST_REPEAT]     = lower_repeat;
    g_handlers[AST_IF]         = lower_if;
    g_handlers[AST_CASE]       = lower_case;
    g_handlers[AST_RETURN]     = lower_return;
    g_handlers[AST_PROC_FAIL]  = lower_proc_fail;
    g_handlers[AST_LOOP_BREAK] = lower_loop_break;
    g_handlers[AST_LOOP_NEXT]  = lower_loop_next;

    /* cohort_icn_data */
    g_handlers[AST_MAKELIST] = lower_makelist;
    g_handlers[AST_RECORD]   = lower_record;
    g_handlers[AST_FIELD]    = lower_field;
    g_handlers[AST_GLOBAL]   = lower_global;
    g_handlers[AST_INITIAL]  = lower_initial;

    /* cohort_icn_sect */
    g_handlers[AST_SECTION]       = lower_section;
    g_handlers[AST_SECTION_PLUS]  = lower_section_plus;
    g_handlers[AST_SECTION_MINUS] = lower_section_minus;
    g_handlers[AST_BANG_BINARY]   = lower_bang_binary;

    /* cohort_icn_gen */
    g_handlers[AST_SUSPEND]   = lower_suspend;
    g_handlers[AST_TO]        = lower_to;
    g_handlers[AST_TO_BY]     = lower_to_by;
    g_handlers[AST_LIMIT]     = lower_limit;
    g_handlers[AST_ALTERNATE] = lower_bb_pump_ast;
    g_handlers[AST_ITERATE]   = lower_bb_pump_ast;
    g_handlers[AST_EVERY]     = lower_every;

    /* cohort_prolog */
    g_handlers[AST_CHOICE]       = lower_choice;
    g_handlers[AST_CLAUSE]       = lower_prolog_broker_child;
    g_handlers[AST_CUT]          = lower_prolog_broker_child;
    g_handlers[AST_UNIFY]        = lower_prolog_broker_child;
    g_handlers[AST_TRAIL_MARK]   = lower_prolog_broker_child;
    g_handlers[AST_TRAIL_UNWIND] = lower_prolog_broker_child;

    /* AST_REVASSIGN, AST_REVSWAP: not yet implemented — SR-15+ rung.
     * Remain at lower_unhandled (set by the loop above). */

    g_handlers_initialized = 1;
}

void lower_expr(LowerCtx *c, const AST_t *e)
{
    if (!e) { sm_emit(c->p, SM_PUSH_NULL); return; }
    if (!g_handlers_initialized) init_handlers();
    g_handlers[e->kind](c, e);
}

static void lower_proc_skeletons(LowerCtx *c)
{
    SM_Program *p = c->p;

    for (int pi = 0; pi < proc_count; pi++) {
        const char *nm = proc_table[pi].name;
        if (!nm || !*nm) continue;
        AST_t *proc = proc_table[pi].proc;

        int skip_jump = sm_emit_i(p, SM_JUMP, 0);
        sm_label_named(p, nm);

        if (proc) {
            int nparams    = (int)proc->ival;
            int body_start = 1 + nparams;

            IcnScope expression_sc; expression_sc.n = 0;
            for (int i = 0; i < nparams && i < FRAME_SLOT_MAX; i++) {
                AST_t *pn = proc->children[1+i];
                if (pn && pn->sval) scope_add(&expression_sc, pn->sval);
            }
            for (int bi = body_start; bi < proc->nchildren; bi++)
                expression_scope_walk(&expression_sc, proc->children[bi]);

            for (int bi = body_start; bi < proc->nchildren; bi++) {
                AST_t *child = proc->children[bi];
                if (!child || child->kind != AST_INITIAL) continue;
                for (int ai = 0; ai < child->nchildren; ai++) {
                    AST_t *as = child->children[ai];
                    if (!as || as->kind != AST_ASSIGN || as->nchildren < 1) continue;
                    AST_t *lhs = as->children[0];
                    if (!lhs || lhs->kind != AST_VAR || !lhs->sval) continue;
                    const char *iname = lhs->sval;
                    int w = 0;
                    for (int r = 0; r < expression_sc.n; r++) {
                        if (expression_sc.e[r].name &&
                            strcmp(expression_sc.e[r].name, iname) == 0) continue;
                        if (w != r) expression_sc.e[w] = expression_sc.e[r];
                        w++;
                    }
                    expression_sc.n = w;
                    for (int s = 0; s < expression_sc.n; s++)
                        expression_sc.e[s].slot = s;
                }
            }

            c->expression_scope         = &expression_sc;
            c->expression_body_lowering = 1;
            for (int bi = body_start; bi < proc->nchildren; bi++) {
                AST_t *body_expr = proc->children[bi];
                if (!body_expr) continue;
                lower_expr(c, body_expr);
                sm_emit(p, SM_VOID_POP);
            }
            c->expression_body_lowering = 0;
            c->expression_scope         = NULL;
        }

        sm_emit(p, SM_RETURN);
        int skip_lbl = sm_label(p);
        sm_patch_jump(p, skip_jump, skip_lbl);
    }

    for (int b = 0; b < PL_PRED_TABLE_SIZE_FWD; b++) {
        for (Pl_PredEntry *e = g_pl_pred_table.buckets[b]; e; e = e->next) {
            if (!e->key || !*e->key) continue;
            int skip_jump_pl = sm_emit_i(p, SM_JUMP, 0);
            sm_label_named(p, e->key);
            sm_emit(p, SM_RETURN);
            int skip_lbl_pl  = sm_label(p);
            sm_patch_jump(p, skip_jump_pl, skip_lbl_pl);
        }
    }
}

SM_Program *lower(const CODE_t *prog)
{
    if (!prog) return NULL;

    LowerCtx ctx;
    ctx.p                        = sm_prog_new();
    ctx.expression_body_lowering = 0;
    ctx.expression_scope         = NULL;
    labtab_init(&ctx.labtab);
    for (int i = 0; i < LOWER_UNHANDLED_WORDS; i++) ctx.unhandled_kinds[i] = 0;

    LowerCtx   *c      = &ctx;
    SM_Program *p      = ctx.p;
    LabelTable *labtab = &ctx.labtab;

    lower_proc_skeletons(c);

    int stno = 0;
    int has_icn = 0;
    for (const STMT_t *s = prog->head; s; s = s->next) {
        if (s->lang == LANG_ICN) {
            has_icn = 1;
            sm_stno_label_record(p, ++stno, NULL);
            continue;
        }
        lower_stmt(c, s);
        sm_stno_label_record(p, ++stno, (s->label && s->label[0]) ? s->label : NULL);
    }

    if (has_icn) sm_emit_si(p, SM_BB_PUMP_PROC, "main", 0);
    if (p->count == 0 || p->instrs[p->count - 1].op != SM_HALT)
        sm_emit(p, SM_HALT);

    labtab_resolve(labtab, p);
    labtab_free(labtab);

    /* SR-14: report unhandled kinds; silent if all handled. */
    int any_unhandled = 0;
    for (int w = 0; w < LOWER_UNHANDLED_WORDS; w++)
        if (ctx.unhandled_kinds[w]) { any_unhandled = 1; break; }
    if (any_unhandled) {
        fprintf(stderr, "sm_lower: unhandled AST kinds:");
        for (int k = 0; k < AST_KIND_COUNT; k++) {
            int w = k / 64, b = k % 64;
            if (w < LOWER_UNHANDLED_WORDS && (ctx.unhandled_kinds[w] >> b) & 1)
                fprintf(stderr, " %s", ast_e_name[k]);
        }
        fprintf(stderr, "\n");
    }

    return p;
}
