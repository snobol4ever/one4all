/*
 * lower.c — AST → SM_Program compiler pass
 *
 * Six frontends produce a shared AST. This file walks that AST and emits
 * a flat SM_Program (stack-machine instruction sequence) consumed by four
 * execution backends: IR-interp, SM-interp, JIT-exec, native-emit.
 *
 * Entry point: SM_Program *lower(const CODE_t *prog)
 *
 * Three phases inside lower():
 *   1. lower_proc_skeletons — JUMP/label/RETURN stubs for every procedure
 *      and Prolog predicate so forward calls resolve before bodies land.
 *   2. lower_stmt loop — walks the statement list; lower_stmt() routes each
 *      statement to pattern-match, assignment, or expression paths;
 *      lower_expr() dispatches to a per-kind handler via g_handlers[].
 *   3. SM_HALT + labtab_resolve — patches forward GOTO targets; reports any
 *      AST kinds that hit lower_unhandled() (diagnostic, normally silent).
 *
 * Naming: c = LowerCtx*, p = SM_Program*, t = AST_t*, s = STMT_t*.
 * Handler signature: static void lower_foo(LowerCtx *c, const AST_t *t)
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
#include "../../frontend/icon/icon_lex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <gc/gc.h>
#include "snobol4.h"

void lower_expr    (LowerCtx *c, const AST_t *t);
void lower_pat_expr(LowerCtx *c, const AST_t *t);
void lower_stmt    (LowerCtx *c, const STMT_t *s);

/* Helpers: read a child safely. */
#define T0(t) ((t)->nchildren > 0 ? (t)->children[0] : NULL)
#define T1(t) ((t)->nchildren > 1 ? (t)->children[1] : NULL)
#define T2(t) ((t)->nchildren > 2 ? (t)->children[2] : NULL)

/* One-liner handlers for arithmetic and unary ops. */
#define CALL1(fn) do { lower_expr(c,T0(t)); sm_emit_si(p,SM_CALL_FN,(fn),1); } while(0)
#define CALL2(fn) do { lower_expr(c,T0(t)); lower_expr(c,T1(t)); sm_emit_si(p,SM_CALL_FN,(fn),2); } while(0)

/* Inline frame-slot load for a named variable, falling back to NV store. */
static void emit_var_load(LowerCtx *c, const char *vn)
{
    SM_Program *p = c->p;
    if (c->expression_body_lowering && c->expression_scope && vn[0] && vn[0] != '&') {
        int slot = scope_get(c->expression_scope, vn);
        if (slot >= 0) { sm_emit_i(p, SM_LOAD_FRAME, slot); return; }
    }
    sm_emit_s(p, SM_PUSH_VAR, vn);
}

/* Inline frame-slot store for a named variable, falling back to NV store. */
static void emit_var_store(LowerCtx *c, const char *vn)
{
    SM_Program *p = c->p;
    if (c->expression_body_lowering && c->expression_scope && vn[0] && vn[0] != '&') {
        int slot = scope_get(c->expression_scope, vn);
        if (slot >= 0) { sm_emit_i(p, SM_STORE_FRAME, slot); return; }
    }
    sm_emit_s(p, SM_STORE_VAR, vn);
}

/* Emit a thunked SM expression (JUMP/body/RETURN/PUSH_EXPRESSION).
 * Used by DEFER, EVAL, and pattern-capture argument lowering. */
static void emit_thunk(LowerCtx *c, const AST_t *body)
{
    SM_Program *p = c->p;
    int skip = sm_emit_i(p, SM_JUMP, 0);
    int entry = sm_label(p);
    if (body) lower_expr(c, body); else sm_emit(p, SM_PUSH_NULL);
    sm_emit(p, SM_RETURN);
    sm_patch_jump(p, skip, sm_label(p));
    sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry, 0);
}

/*── Literals ────────────────────────────────────────────────────────────────*/

static void lower_qlit(LowerCtx *c, const AST_t *t) { sm_emit_s(c->p, SM_PUSH_LIT_S, t->sval ? t->sval : ""); }
static void lower_cset(LowerCtx *c, const AST_t *t) { sm_emit_s(c->p, SM_PUSH_LIT_S, t->sval ? t->sval : ""); }
static void lower_ilit(LowerCtx *c, const AST_t *t) { sm_emit_i(c->p, SM_PUSH_LIT_I, (int64_t)t->ival); }
static void lower_flit(LowerCtx *c, const AST_t *t) { sm_emit_f(c->p, SM_PUSH_LIT_F, t->dval); }
static void lower_nul (LowerCtx *c, const AST_t *t) { (void)t; sm_emit(c->p, SM_PUSH_NULL); }

/*── Variable references ─────────────────────────────────────────────────────*/

static void lower_var(LowerCtx *c, const AST_t *t)     { emit_var_load(c, t->sval ? t->sval : ""); }
static void lower_keyword(LowerCtx *c, const AST_t *t) { sm_emit_s(c->p, SM_PUSH_VAR, kw_canonicalize(t->sval)); }

static void lower_indirect(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    /* $.var[idx] fast path: bypass INDIR_GET, emit PUSH_VAR + IDX directly. */
    const AST_t *ch = T0(t);
    if (ch && ch->kind == AST_NAME && ch->nchildren == 1) {
        const AST_t *inner = ch->children[0];
        if (inner && inner->kind == AST_IDX && inner->nchildren >= 2
                && inner->children[0] && inner->children[0]->kind == AST_VAR
                && inner->children[0]->sval) {
            sm_emit_s(p, SM_PUSH_VAR, inner->children[0]->sval);
            for (int i = 1; i < inner->nchildren; i++) lower_expr(c, inner->children[i]);
            sm_emit_si(p, SM_CALL_FN, "IDX", (int64_t)inner->nchildren);
            return;
        }
    }
    lower_expr(c, ch);
    sm_emit_si(p, SM_CALL_FN, "INDIR_GET", 1);
}

static void lower_defer(LowerCtx *c, const AST_t *t)
{
    /* *expr in value context — thunk the child, push a DT_E descriptor. */
    emit_thunk(c, T0(t));
}

/*── Arithmetic ──────────────────────────────────────────────────────────────*/

static void lower_interrogate(LowerCtx *c, const AST_t *t) { lower_expr(c, T0(t)); }

static void lower_name(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    const char *vname = (T0(t) && T0(t)->sval) ? T0(t)->sval : "";
    sm_emit_s(p, SM_PUSH_LIT_S, vname);
    sm_emit_si(p, SM_CALL_FN, "NAME_PUSH", 1);
}

static void lower_mns(LowerCtx *c, const AST_t *t) { SM_Program *p = c->p; LOWER1_VAL(SM_NEG); }
static void lower_pls(LowerCtx *c, const AST_t *t) { SM_Program *p = c->p; LOWER1_VAL(SM_COERCE_NUM); }
static void lower_add(LowerCtx *c, const AST_t *t) { SM_Program *p = c->p; LOWER2(SM_ADD); }
static void lower_sub(LowerCtx *c, const AST_t *t) { SM_Program *p = c->p; LOWER2(SM_SUB); }
static void lower_mul(LowerCtx *c, const AST_t *t) { SM_Program *p = c->p; LOWER2(SM_MUL); }
static void lower_div(LowerCtx *c, const AST_t *t) { SM_Program *p = c->p; LOWER2(SM_DIV); }
static void lower_mod(LowerCtx *c, const AST_t *t) { SM_Program *p = c->p; LOWER2(SM_MOD); }
static void lower_pow(LowerCtx *c, const AST_t *t) { SM_Program *p = c->p; LOWER2(SM_EXP); }

/*── Sequences and alternation ───────────────────────────────────────────────*/

static void lower_vlist(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    if (t->nchildren == 0) { sm_emit(p, SM_PUSH_NULL); return; }
    if (t->nchildren == 1) { lower_expr(c, t->children[0]); return; }
    int n = t->nchildren - 1;
    int *jumps = (int *)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < t->nchildren; i++) {
        lower_expr(c, t->children[i]);
        if (i < t->nchildren - 1) { jumps[i] = sm_emit_i(p, SM_JUMP_S, 0); sm_emit(p, SM_VOID_POP); }
    }
    int done = sm_label(p);
    for (int i = 0; i < n; i++) sm_patch_jump(p, jumps[i], done);
    free(jumps);
}

static void lower_cat_seq(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    int has_defer = 0;
    for (int i = 0; i < t->nchildren && !has_defer; i++)
        if (t->children[i] && t->children[i]->kind == AST_DEFER) has_defer = 1;
    if (has_defer) {
        for (int i = 0; i < t->nchildren; i++) lower_pat_expr(c, t->children[i]);
        for (int i = 1; i < t->nchildren; i++) sm_emit(p, SM_PAT_CAT);
    } else {
        for (int i = 0; i < t->nchildren; i++) lower_expr(c, t->children[i]);
        for (int i = 1; i < t->nchildren; i++) sm_emit(p, SM_CONCAT);
    }
}

static void lower_alt  (LowerCtx *c, const AST_t *t) { lower_pat_expr(c, t); }

static void lower_opsyn(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    const char *raw = t->sval ? t->sval : "&";
    static char op_buf[4];
    const char *op = raw;
    const char *lp = strchr(raw, '(');
    if (lp && lp[1] && lp[2] == ')') { op_buf[0] = lp[1]; op_buf[1] = '\0'; op = op_buf; }
    else if (strcmp(raw, "BARFN")  == 0) op = "|";
    else if (strcmp(raw, "AROWFN") == 0) op = "^";
    for (int i = 0; i < t->nchildren; i++) lower_expr(c, t->children[i]);
    sm_emit_si(p, SM_CALL_FN, op, (int64_t)t->nchildren);
}

/*── Pattern-context lowering ────────────────────────────────────────────────*/

/* Build a tab-separated argument name list from *fn(var,...) for SM_PAT_CAPTURE_FN.
 * Returns NULL if any arg is not a plain AST_VAR (caller falls back to stack args). */
const char *sm_pat_capture_fn_arg_names(const AST_t *fnc)
{
    if (!fnc || fnc->nchildren <= 0) return NULL;
    size_t len = 0;
    for (int i = 0; i < fnc->nchildren; i++) {
        const AST_t *a = fnc->children[i];
        if (!a || a->kind != AST_VAR || !a->sval) return NULL;
        len += strlen(a->sval) + 1;
    }
    char *buf = GC_MALLOC(len), *q = buf;
    for (int i = 0; i < fnc->nchildren; i++) {
        const char *nm = fnc->children[i]->sval;
        size_t n = strlen(nm);
        memcpy(q, nm, n); q += n;
        *q++ = (i + 1 < fnc->nchildren) ? '\t' : '\0';
    }
    return buf;
}

/* Emit args for a *fn(arg,...) pattern-capture call.
 * Literal args go inline; non-literal args are thunked. */
static void emit_pat_fn_args(LowerCtx *c, const AST_t *fnc)
{
    for (int i = 0; i < fnc->nchildren; i++) {
        AST_t *arg = fnc->children[i];
        if (arg && arg->kind == AST_QLIT) lower_expr(c, arg);
        else                              emit_thunk(c, arg);
    }
}

/* Emit a pattern capture of kind mode (0=conditional .V, 1=immediate $V, 2=cursor @V).
 * Handles variable, *fn(), and *fn(args) targets. */
static void emit_pat_capture(LowerCtx *c, const AST_t *var_node, int mode)
{
    SM_Program *p = c->p;
    if (var_node && var_node->kind == AST_DEFER
            && var_node->nchildren > 0
            && var_node->children[0]
            && var_node->children[0]->kind == AST_FNC
            && var_node->children[0]->sval) {
        const AST_t *fnc = var_node->children[0];
        const char *names = sm_pat_capture_fn_arg_names(fnc);
        if (names || fnc->nchildren == 0) {
            int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN, fnc->sval);
            p->instrs[idx].a[1].i = mode; p->instrs[idx].a[2].s = names;
        } else {
            emit_pat_fn_args(c, fnc);
            int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN_ARGS, fnc->sval);
            p->instrs[idx].a[1].i = mode; p->instrs[idx].a[2].i = fnc->nchildren;
        }
    } else {
        int idx = sm_emit_s(p, SM_PAT_CAPTURE, var_node ? var_node->sval : "");
        p->instrs[idx].a[1].i = mode;
    }
}

void lower_pat_expr(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    if (!t) return;
    switch (t->kind) {
    case AST_QLIT:  sm_emit_s(p, SM_PAT_LIT, t->sval ? t->sval : ""); return;
    case AST_VAR:   sm_emit_s(p, SM_PUSH_VAR, t->sval); sm_emit(p, SM_PAT_DEREF); return;
    case AST_ARB:   sm_emit(p, SM_PAT_ARB);    return;
    case AST_REM:   sm_emit(p, SM_PAT_REM);    return;
    case AST_FAIL:  sm_emit(p, SM_PAT_FAIL);   return;
    case AST_SUCCEED: sm_emit(p, SM_PAT_SUCCEED); return;
    case AST_ABORT: sm_emit(p, SM_PAT_ABORT);  return;
    case AST_BAL:   sm_emit(p, SM_PAT_BAL);    return;
    case AST_FENCE:
        if (t->nchildren > 0) { lower_pat_expr(c, t->children[0]); sm_emit(p, SM_PAT_FENCE1); }
        else                    sm_emit(p, SM_PAT_FENCE);
        return;
    case AST_ANY:    lower_expr(c, T0(t)); sm_emit(p, SM_PAT_ANY);    return;
    case AST_NOTANY: lower_expr(c, T0(t)); sm_emit(p, SM_PAT_NOTANY); return;
    case AST_SPAN:   lower_expr(c, T0(t)); sm_emit(p, SM_PAT_SPAN);   return;
    case AST_BREAK:  lower_expr(c, T0(t)); sm_emit(p, SM_PAT_BREAK);  return;
    case AST_BREAKX: lower_expr(c, T0(t)); sm_emit(p, SM_PAT_BREAK);  return;
    case AST_LEN:    lower_expr(c, T0(t)); sm_emit(p, SM_PAT_LEN);    return;
    case AST_POS:    lower_expr(c, T0(t)); sm_emit(p, SM_PAT_POS);    return;
    case AST_RPOS:   lower_expr(c, T0(t)); sm_emit(p, SM_PAT_RPOS);   return;
    case AST_TAB:    lower_expr(c, T0(t)); sm_emit(p, SM_PAT_TAB);    return;
    case AST_RTAB:   lower_expr(c, T0(t)); sm_emit(p, SM_PAT_RTAB);   return;
    case AST_ARBNO:  { SM_Program *_p = p; LOWER1_PAT(SM_PAT_ARBNO); }
    case AST_SEQ:
    case AST_CAT:
        for (int i = 0; i < t->nchildren; i++) lower_pat_expr(c, t->children[i]);
        for (int i = 1; i < t->nchildren; i++) sm_emit(p, SM_PAT_CAT);
        return;
    case AST_ALT:
        for (int i = 0; i < t->nchildren; i++) lower_pat_expr(c, t->children[i]);
        for (int i = 1; i < t->nchildren; i++) sm_emit(p, SM_PAT_ALT);
        return;
    case AST_CAPT_COND_ASGN:
        lower_pat_expr(c, T0(t));
        if (t->nchildren > 1) emit_pat_capture(c, t->children[1], 0);
        return;
    case AST_CAPT_IMMED_ASGN:
        lower_pat_expr(c, T0(t));
        if (t->nchildren > 1) emit_pat_capture(c, t->children[1], 1);
        return;
    case AST_CAPT_CURSOR:
        if (t->nchildren == 1) {
            sm_emit(p, SM_PAT_EPS);
            emit_pat_capture(c, t->children[0], 2);
        } else {
            lower_pat_expr(c, T0(t));
            if (t->nchildren > 1) emit_pat_capture(c, t->children[1], 2);
        }
        return;
    case AST_DEFER: {
        const AST_t *ch = T0(t);
        /* *fn() in pattern — run fn at each match position via SM_PAT_USERCALL. */
        if (ch && ch->kind == AST_FNC && ch->sval) {
            if (ch->nchildren == 0) {
                int idx = sm_emit_s(p, SM_PAT_USERCALL, ch->sval);
                p->instrs[idx].a[2].s = NULL;
            } else {
                emit_pat_fn_args(c, ch);
                int idx = sm_emit_s(p, SM_PAT_USERCALL_ARGS, ch->sval);
                p->instrs[idx].a[1].i = ch->nchildren;
            }
            return;
        }
        /* *var — push by name so self-recursive patterns resolve at match time. */
        if (ch && ch->kind == AST_VAR && ch->sval) { sm_emit_s(p, SM_PAT_REFNAME, ch->sval); return; }
        lower_expr(c, ch); sm_emit(p, SM_PAT_DEREF);
        return;
    }
    default:
        lower_expr(c, t); sm_emit(p, SM_PAT_DEREF);
        return;
    }
}

/*── Function calls, assignment, scanning ────────────────────────────────────*/

static void lower_fnc(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    int nargs = t->nchildren;

    /* EVAL(*expr) — inline the thunked expression, call SM_CALL_EXPRESSION. */
    if (nargs == 1 && t->sval && strcmp(t->sval, "EVAL") == 0
            && t->children[0] && t->children[0]->kind == AST_DEFER) {
        const AST_t *inner = T0(t->children[0]);
        int skip = sm_emit_i(p, SM_JUMP, 0), entry = sm_label(p);
        if (inner) lower_expr(c, inner); else sm_emit(p, SM_PUSH_NULL);
        sm_emit(p, SM_RETURN);
        sm_patch_jump(p, skip, sm_label(p));
        sm_emit_ii(p, SM_CALL_EXPRESSION, (int64_t)entry, 0);
        return;
    }
    /* Icon-style call: sval==NULL, children[0] is the callee name node. */
    if (!t->sval && nargs >= 1 && t->children[0] && t->children[0]->sval) {
        const char *fn = t->children[0]->sval;
        for (int i = 1; i < nargs; i++) lower_expr(c, t->children[i]);
        sm_emit_si(p, SM_CALL_FN, fn, (int64_t)(nargs - 1));
        return;
    }
    for (int i = 0; i < nargs; i++) lower_expr(c, t->children[i]);
    sm_emit_si(p, SM_CALL_FN, t->sval ? t->sval : "", (int64_t)nargs);
}

static void lower_idx(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    for (int i = 0; i < t->nchildren; i++) lower_expr(c, t->children[i]);
    sm_emit_si(p, SM_CALL_FN, "IDX", (int64_t)t->nchildren);
}

static void lower_assign(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    lower_expr(c, T1(t));   /* rhs first */
    const AST_t *lhs = T0(t);
    if (!lhs) return;
    if (lhs->kind == AST_VAR)     { emit_var_store(c, lhs->sval ? lhs->sval : ""); return; }
    if (lhs->kind == AST_KEYWORD) { sm_emit_s(p, SM_STORE_VAR, kw_canonicalize(lhs->sval)); return; }
    if (lhs->kind == AST_FNC && lhs->sval) {
        lower_expr(c, T0(lhs));
        char set[256]; snprintf(set, sizeof set, "%s_SET", lhs->sval);
        sm_emit_si(p, SM_CALL_FN, set, 2); return;
    }
    if (lhs->kind == AST_IDX) {
        for (int i = 0; i < lhs->nchildren; i++) lower_expr(c, lhs->children[i]);
        sm_emit_si(p, SM_CALL_FN, "IDX_SET", (int64_t)(lhs->nchildren + 1)); return;
    }
    if (lhs->kind == AST_FIELD) {
        lower_expr(c, T0(lhs));
        sm_emit_s(p, SM_PUSH_LIT_S, lhs->sval ? lhs->sval : "");
        sm_emit_si(p, SM_CALL_FN, "FIELD_SET", 3); return;
    }
    lower_expr(c, lhs);
    sm_emit_si(p, SM_CALL_FN, "ASGN", 2);
}

static void lower_scan(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    if (t->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(c, t->children[0]);
    sm_emit_si(p, SM_CALL_FN, "ICN_SCAN_PUSH", 1);
    sm_emit(p, SM_VOID_POP);
    if (t->nchildren > 1) lower_expr(c, t->children[1]); else sm_emit(p, SM_PUSH_NULL);
    sm_emit_si(p, SM_CALL_FN, "ICN_SCAN_POP", 1);
}

static void lower_swap(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    /* Inline fast path for two plain variables: save→copy→restore. */
    if (t->nchildren >= 2 && T0(t) && T1(t)
            && T0(t)->kind == AST_VAR && T1(t)->kind == AST_VAR) {
        const char *ln = T0(t)->sval ? T0(t)->sval : "";
        const char *rn = T1(t)->sval ? T1(t)->sval : "";
        emit_var_load(c, ln); sm_emit_s(p, SM_STORE_VAR, "__icn_swap_tmp__"); sm_emit(p, SM_VOID_POP);
        emit_var_load(c, rn); emit_var_store(c, ln);
        sm_emit_s(p, SM_PUSH_VAR, "__icn_swap_tmp__"); emit_var_store(c, rn);
        sm_emit(p, SM_VOID_POP);
        return;
    }
    lower_expr(c, T0(t)); lower_expr(c, T1(t));
    sm_emit_si(p, SM_CALL_FN, "SWAP", 2);
}

/*── Relational operators ────────────────────────────────────────────────────*/

static void lower_acomp(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    lower_expr(c, T0(t)); lower_expr(c, T1(t));
    sm_emit_i(p, SM_ACOMP, (int64_t)t->kind);
}
static void lower_lcomp(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    lower_expr(c, T0(t)); lower_expr(c, T1(t));
    sm_emit_i(p, SM_LCOMP, (int64_t)t->kind);
}

/*── Cset / list ops ─────────────────────────────────────────────────────────*/

static void lower_cset_op(LowerCtx *c, const AST_t *t) { emit_push_expr(c, t); }

static void lower_lconcat(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    int has_gen = 0;
    for (int i = 0; i < t->nchildren && !has_gen; i++)
        if (is_suspendable(t->children[i])) has_gen = 1;
    if (!has_gen) {
        if (t->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
        for (int i = 0; i < t->nchildren; i++) lower_expr(c, t->children[i]);
        for (int i = 1; i < t->nchildren; i++) sm_emit(p, SM_CONCAT);
        return;
    }
    sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)t));
}

/*── Unary Icon ops ──────────────────────────────────────────────────────────*/

static void lower_nonnull (LowerCtx *c, const AST_t *t) { SM_Program *p=c->p; CALL1("NONNULL"); }
static void lower_null    (LowerCtx *c, const AST_t *t) { SM_Program *p=c->p; CALL1("ICN_NULL"); }
static void lower_size    (LowerCtx *c, const AST_t *t) { SM_Program *p=c->p; CALL1("SIZE"); }
static void lower_identical(LowerCtx *c, const AST_t *t){ SM_Program *p=c->p; CALL2("IDENTICAL"); }
static void lower_random  (LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    if (t->nchildren >= 1) { CALL1("ICN_RANDOM"); } else { sm_emit(p, SM_PUSH_NULL); }
}

static void lower_not(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    lower_expr(c, T0(t));
    int js = sm_emit_i(p, SM_JUMP_S, 0);
    sm_emit(p, SM_VOID_POP); sm_emit(p, SM_PUSH_NULL);
    int jend = sm_emit_i(p, SM_JUMP, 0);
    int flbl = sm_label(p); sm_patch_jump(p, js, flbl);
    sm_emit(p, SM_VOID_POP); sm_emit_si(p, SM_CALL_FN, "FAIL", 0);
    sm_patch_jump(p, jend, sm_label(p));
}

static void lower_augop(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    const AST_t *lhs = T0(t), *rhs = T1(t);
    int op = (int)t->ival;
    /* Fast path: simple variable or keyword lhs — inline load/op/store. */
    const char *lname = NULL; int lslot = -1, is_kw = 0;
    if (lhs && lhs->kind == AST_VAR && lhs->sval) {
        lname = lhs->sval;
        if (c->expression_body_lowering && c->expression_scope && lname[0] && lname[0] != '&')
            lslot = scope_get(c->expression_scope, lname);
    } else if (lhs && lhs->kind == AST_KEYWORD && lhs->sval) {
        lname = lhs->sval; is_kw = 1;
    }
    if (lslot >= 0 || lname) {
        if      (lslot >= 0) sm_emit_i(p, SM_LOAD_FRAME, lslot);
        else if (is_kw)      sm_emit_s(p, SM_PUSH_VAR, kw_canonicalize(lname));
        else                 sm_emit_s(p, SM_PUSH_VAR, lname);
        lower_expr(c, rhs);
        int done = 1;
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
            done = 0; break;
        }
        if (done) {
            if      (lslot >= 0) sm_emit_i(p, SM_STORE_FRAME, lslot);
            else if (is_kw)      sm_emit_s(p, SM_STORE_VAR, kw_canonicalize(lname));
            else                 sm_emit_s(p, SM_STORE_VAR, lname);
        }
        return;
    }
    lower_expr(c, lhs); lower_expr(c, rhs);
    sm_emit_i(p, SM_PUSH_LIT_I, (int64_t)op);
    sm_emit_si(p, SM_CALL_FN, "AUGOP", 3);
}

/*── Control flow ────────────────────────────────────────────────────────────*/

static void lower_seq_expr(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    if (t->nchildren == 0) { sm_emit(p, SM_PUSH_NULL); return; }
    for (int i = 0; i < t->nchildren; i++) {
        lower_expr(c, t->children[i]);
        if (i < t->nchildren - 1) sm_emit(p, SM_VOID_POP);
    }
}

static void lower_if(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    if (t->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(c, t->children[0]);
    int jf = sm_emit_i(p, SM_JUMP_F, 0);
    sm_emit(p, SM_VOID_POP);
    if (t->nchildren > 1) lower_expr(c, t->children[1]); else sm_emit(p, SM_PUSH_NULL);
    int jend = sm_emit_i(p, SM_JUMP, 0);
    sm_patch_jump(p, jf, sm_label(p));
    sm_emit(p, SM_VOID_POP);
    if (t->nchildren > 2) lower_expr(c, t->children[2]); else sm_emit(p, SM_PUSH_NULL);
    sm_patch_jump(p, jend, sm_label(p));
}

/* Shared body for while/until — differs only in which jump exits. */
static void lower_while_until(LowerCtx *c, const AST_t *t, int exit_on_success)
{
    SM_Program *p = c->p;
    int top = sm_label(p);
    if (t->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(c, t->children[0]);
    int jx = exit_on_success ? sm_emit_i(p, SM_JUMP_S, 0) : sm_emit_i(p, SM_JUMP_F, 0);
    sm_emit(p, SM_VOID_POP);
    if (t->nchildren > 1) { lower_expr(c, t->children[1]); sm_emit(p, SM_VOID_POP); }
    sm_emit_i(p, SM_JUMP, top);
    sm_patch_jump(p, jx, sm_label(p));
    sm_emit(p, SM_VOID_POP); sm_emit(p, SM_PUSH_NULL);
}
static void lower_while(LowerCtx *c, const AST_t *t) { lower_while_until(c, t, 0); }
static void lower_until(LowerCtx *c, const AST_t *t) { lower_while_until(c, t, 1); }

static void lower_repeat(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    int top = sm_label(p);
    if (t->nchildren > 0) { lower_expr(c, t->children[0]); sm_emit(p, SM_VOID_POP); }
    sm_emit_i(p, SM_JUMP, top);
    sm_emit(p, SM_PUSH_NULL);
}

static void lower_loop_break(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    if (t->nchildren > 0) lower_expr(c, t->children[0]); else sm_emit(p, SM_PUSH_NULL);
    sm_emit_i(p, SM_JUMP, p->count + 1);  /* sentinel: sm_interp detects self+1 as break */
}

static void lower_loop_next(LowerCtx *c, const AST_t *t) { (void)t; sm_emit(c->p, SM_PUSH_NULL); }

static void lower_return(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    if (t->nchildren > 0) lower_expr(c, t->children[0]); else sm_emit(p, SM_PUSH_NULL);
    sm_emit(p, SM_RETURN);
}

static void lower_proc_fail(LowerCtx *c, const AST_t *t)
{
    (void)t; sm_emit(c->p, SM_PUSH_NULL); sm_emit(c->p, SM_FRETURN);
}

static void lower_case(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    if (t->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }

    /* Raku triple layout: (n-1) divisible by 3, child[1] is ILIT or NUL. */
    int is_raku = (t->nchildren >= 4 && (t->nchildren - 1) % 3 == 0
                   && t->children[1]
                   && (t->children[1]->kind == AST_ILIT || t->children[1]->kind == AST_NUL));

    if (!is_raku) {
        /* Icon pair layout: topic + (val,body)* + [default] */
        int nc = t->nchildren - 1, has_def = nc % 2, npairs = nc / 2;
        lower_expr(c, t->children[0]);
        sm_emit_s(p, SM_STORE_VAR, "__case_topic__"); sm_emit(p, SM_VOID_POP);
        int end_jumps[64], nend = 0;
        for (int i = 0; i < npairs && i < 32; i++) {
            sm_emit_s(p, SM_PUSH_VAR, "__case_topic__");
            lower_expr(c, t->children[1 + i*2]);
            sm_emit_si(p, SM_CALL_FN, "ICN_CASE_EQ", 2);
            int jf = sm_emit_i(p, SM_JUMP_F, 0);
            sm_emit(p, SM_VOID_POP);
            lower_expr(c, t->children[2 + i*2]);
            if (nend < 64) end_jumps[nend++] = sm_emit_i(p, SM_JUMP, 0);
            sm_patch_jump(p, jf, sm_label(p)); sm_emit(p, SM_VOID_POP);
        }
        if (has_def) lower_expr(c, t->children[t->nchildren - 1]); else sm_emit(p, SM_PUSH_NULL);
        int end = sm_label(p);
        for (int i = 0; i < nend; i++) sm_patch_jump(p, end_jumps[i], end);
        return;
    }

    /* Raku triple layout: topic + (cmp_kind, val, body)* + [default triple] */
    #define CHUNK(expr) do { \
        int _s=sm_emit_i(p,SM_JUMP,0), _e=sm_label(p); \
        lower_expr(c,(expr)); sm_emit(p,SM_RETURN); \
        sm_patch_jump(p,_s,sm_label(p)); \
        sm_emit_ii(p,SM_PUSH_EXPRESSION,(int64_t)_e,0); } while(0)

    int ntriples = (t->nchildren - 1) / 3, has_def = 0, def_idx = -1;
    if (ntriples > 0) {
        AST_t *last_cmp = t->children[1 + (ntriples-1)*3];
        if (last_cmp && last_cmp->kind == AST_NUL) { has_def = 1; def_idx = ntriples - 1; }
    }
    CHUNK(t->children[0]);
    for (int i = 0; i < ntriples; i++) {
        if (i == def_idx) continue;
        int base = 1 + i*3;
        AST_t *cmp = t->children[base];
        sm_emit_i(p, SM_PUSH_LIT_I, (int64_t)((cmp && cmp->kind == AST_ILIT) ? cmp->ival : AST_EQ));
        CHUNK(t->children[base+1]); CHUNK(t->children[base+2]);
    }
    if (has_def) { CHUNK(t->children[1 + def_idx*3 + 2]); }
    sm_emit_ii(p, SM_BB_PUMP_CASE, (int64_t)(ntriples - has_def), (int64_t)has_def);
    #undef CHUNK
}

/*── Data constructors ───────────────────────────────────────────────────────*/

static void lower_makelist(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    for (int i = 0; i < t->nchildren; i++) lower_expr(c, t->children[i]);
    sm_emit_si(p, SM_CALL_FN, "MAKELIST", (int64_t)t->nchildren);
}

static void lower_record(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    sm_emit_s(p, SM_PUSH_LIT_S, t->sval ? t->sval : "");
    for (int i = 0; i < t->nchildren; i++) lower_expr(c, t->children[i]);
    sm_emit_si(p, SM_CALL_FN, "RECORD_MAKE", (int64_t)t->nchildren + 1);
}

static void lower_field(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    lower_expr(c, T0(t));
    sm_emit_s(p, SM_PUSH_LIT_S, t->sval ? t->sval : "");
    sm_emit_si(p, SM_CALL_FN, "FIELD_GET", 2);
}

static void lower_global(LowerCtx *c, const AST_t *t) { (void)t; sm_emit(c->p, SM_PUSH_NULL); }

static void lower_initial(LowerCtx *c, const AST_t *t)
{
    /* Once-on-first-call guard: NV sentinel per AST node pointer. */
    SM_Program *p = c->p;
    char sentinel[64];
    snprintf(sentinel, sizeof sentinel, "__initial_%lx__", (unsigned long)(uintptr_t)t);
    sm_emit_s(p, SM_PUSH_VAR, sentinel);
    sm_emit_si(p, SM_CALL_FN, "NONNULL", 1);
    int skip = sm_emit_i(p, SM_JUMP_S, 0);
    sm_emit(p, SM_VOID_POP);
    for (int i = 0; i < t->nchildren; i++) {
        if (!t->children[i]) continue;
        lower_expr(c, t->children[i]); sm_emit(p, SM_VOID_POP);
    }
    sm_emit_i(p, SM_PUSH_LIT_I, 1);
    sm_emit_s(p, SM_STORE_VAR, sentinel); sm_emit(p, SM_VOID_POP);
    int done = sm_emit_i(p, SM_JUMP, 0);
    sm_patch_jump(p, skip, sm_label(p)); sm_emit(p, SM_VOID_POP);
    sm_patch_jump(p, done, sm_label(p)); sm_emit(p, SM_PUSH_NULL);
}

/*── String sections ─────────────────────────────────────────────────────────*/

static void lower_section_3(LowerCtx *c, const AST_t *t, const char *fn)
{
    SM_Program *p = c->p;
    if (t->nchildren >= 3) {
        lower_expr(c, t->children[0]); lower_expr(c, t->children[1]); lower_expr(c, t->children[2]);
        sm_emit_si(p, SM_CALL_FN, fn, 3);
    } else sm_emit(p, SM_PUSH_NULL);
}
static void lower_section      (LowerCtx *c, const AST_t *t) { lower_section_3(c, t, "ICN_SECTION_RANGE"); }
static void lower_section_plus (LowerCtx *c, const AST_t *t) { lower_section_3(c, t, "ICN_SECTION_PLUS");  }
static void lower_section_minus(LowerCtx *c, const AST_t *t) { lower_section_3(c, t, "ICN_SECTION_MINUS"); }
static void lower_bang_binary  (LowerCtx *c, const AST_t *t)
{
    sm_emit_i(c->p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)t));
}

/*── Generator coroutines ────────────────────────────────────────────────────*/

/* Emit an SM coroutine body for integer range lo..hi [by step].
 * glocal slots: 0=lo, 1=hi, 2=cur, (3=step for to_by). */
static void emit_range_coroutine(LowerCtx *c, const AST_t *lo_expr,
                                  const AST_t *hi_expr, const AST_t *step_expr)
{
    SM_Program *p = c->p;
    int skip = sm_emit_i(p, SM_JUMP, 0), entry = sm_label(p);
    sm_emit(p, SM_RESUME);
    if (lo_expr) lower_expr(c, lo_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 0); sm_emit(p, SM_VOID_POP);
    if (hi_expr) lower_expr(c, hi_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 1); sm_emit(p, SM_VOID_POP);
    if (step_expr) {
        lower_expr(c, step_expr);
        sm_emit_i(p, SM_STORE_GLOCAL, 3); sm_emit(p, SM_VOID_POP);
    }
    sm_emit_i(p, SM_LOAD_GLOCAL, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 2); sm_emit(p, SM_VOID_POP);
    int loop = sm_label(p);
    if (!step_expr) {
        /* Simple: exit when cur > hi */
        sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 1);
        sm_emit(p, SM_ICMP_GT);
        int exit = sm_emit_i(p, SM_JUMP_S, 0);
        sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit(p, SM_SUSPEND);
        sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_INCR, 1);
        sm_emit_i(p, SM_STORE_GLOCAL, 2); sm_emit(p, SM_VOID_POP);
        sm_emit_i(p, SM_JUMP, loop);
        sm_patch_jump(p, exit, sm_label(p));
    } else {
        /* Stepped: branch on sign of step */
        sm_emit_i(p, SM_LOAD_GLOCAL, 3); sm_emit_i(p, SM_PUSH_LIT_I, 0);
        sm_emit(p, SM_ICMP_LT);
        int neg = sm_emit_i(p, SM_JUMP_S, 0);
        sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 1);
        sm_emit(p, SM_ICMP_GT);
        int exit_pos = sm_emit_i(p, SM_JUMP_S, 0);
        int body = sm_emit_i(p, SM_JUMP, 0);
        sm_patch_jump(p, neg, sm_label(p));
        sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 1);
        sm_emit(p, SM_ICMP_LT);
        int exit_neg = sm_emit_i(p, SM_JUMP_S, 0);
        sm_patch_jump(p, body, sm_label(p));
        sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit(p, SM_SUSPEND);
        sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 3);
        sm_emit(p, SM_ADD); sm_emit_i(p, SM_STORE_GLOCAL, 2); sm_emit(p, SM_VOID_POP);
        sm_emit_i(p, SM_JUMP, loop);
        int exit_pc = sm_label(p);
        sm_patch_jump(p, exit_pos, exit_pc); sm_patch_jump(p, exit_neg, exit_pc);
    }
    sm_emit(p, SM_PUSH_NULL); sm_emit(p, SM_RETURN);
    sm_patch_jump(p, skip, sm_label(p));
    sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry, 0);
    sm_emit(p, SM_BB_PUMP_SM);
}

static void lower_to   (LowerCtx *c, const AST_t *t) { emit_range_coroutine(c, T0(t), T1(t), NULL); }
static void lower_to_by(LowerCtx *c, const AST_t *t) { emit_range_coroutine(c, T0(t), T1(t), T2(t)); }

static void lower_every(LowerCtx *c, const AST_t *t)
{
    sm_emit_i(c->p, SM_BB_PUMP_EVERY, (int64_t)every_table_register((AST_t *)t));
}

static void lower_suspend(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    if (t->nchildren > 0 && t->children[0]) lower_expr(c, t->children[0]);
    else sm_emit(p, SM_PUSH_NULL);
    int jf = sm_emit_i(p, SM_JUMP_F, 0);
    sm_emit(p, SM_SUSPEND_VALUE);
    if (t->nchildren > 1 && t->children[1]) { lower_expr(c, t->children[1]); sm_emit(p, SM_VOID_POP); }
    sm_emit(p, SM_PUSH_NULL);
    int jdone = sm_emit_i(p, SM_JUMP, 0);
    sm_patch_jump(p, jf, sm_label(p));
    sm_patch_jump(p, jdone, sm_label(p));
}

static void lower_bb_pump_ast(LowerCtx *c, const AST_t *t)
{
    sm_emit_i(c->p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)t));
}
static void lower_limit(LowerCtx *c, const AST_t *t) { emit_push_expr(c, t); sm_emit(c->p, SM_BB_PUMP); }

/*── Prolog ──────────────────────────────────────────────────────────────────*/

static void lower_choice(LowerCtx *c, const AST_t *t)
{
    SM_Program *p = c->p;
    if (t->sval) {
        const char *sl = strrchr(t->sval, '/');
        int arity = sl ? atoi(sl + 1) : 0;
        sm_emit_si(p, SM_BB_ONCE_PROC, t->sval, (int64_t)arity);
    } else {
        emit_push_expr(c, t); sm_emit(p, SM_BB_ONCE);
    }
}

static void lower_prolog_child(LowerCtx *c, const AST_t *t)
{
    emit_push_expr(c, t); sm_emit(c->p, SM_BB_ONCE);
}

/*── Statement lowering ──────────────────────────────────────────────────────*/

void lower_stmt(LowerCtx *c, const STMT_t *s)
{
    SM_Program *p = c->p;
    LabelTable *labtab = &c->labtab;

    /* Skip blank lines entirely. */
    if (!s->is_end && (!s->label || !s->label[0])
            && !s->subject && !s->pattern && !s->replacement
            && !s->goto_u && !s->goto_u_expr
            && !s->goto_s && !s->goto_s_expr
            && !s->goto_f && !s->goto_f_expr)
        return;

    if (s->label && s->label[0]) {
        int lbl_idx = sm_label_named(p, s->label);
        labtab_define(labtab, s->label, lbl_idx);
        if (FUNC_IS_ENTRY_LABEL(s->label)) {
            p->instrs[p->count - 1].a[2].i = 1;
            sm_emit(p, SM_DEFINE_ENTRY);
        }
    }

    sm_emit_ii(p, SM_STNO, (int64_t)s->stno, (int64_t)s->lineno);
    if (s->is_end) { sm_emit(p, SM_HALT); return; }
    if (s->lang == LANG_ICN) return;  /* Icon defs registered by polyglot_init */

    if (s->lang == LANG_PL) {
        if (s->subject && s->subject->kind == AST_CHOICE && s->subject->sval) {
            const char *sl = strrchr(s->subject->sval, '/');
            sm_emit_si(p, SM_BB_ONCE_PROC, s->subject->sval, (int64_t)(sl ? atoi(sl+1) : 0));
        } else {
            if (s->subject) lower_expr(c, s->subject); else sm_emit(p, SM_PUSH_NULL);
            sm_emit(p, SM_BB_ONCE);
        }
        goto emit_gotos;
    }

    if (s->pattern) {
        /* Pattern-match statement: pat / subject [= replacement] */
        lower_pat_expr(c, s->pattern);
        if (s->subject) lower_expr(c, s->subject); else sm_emit(p, SM_PUSH_NULL);
        if (s->has_eq && s->replacement) lower_expr(c, s->replacement);
        else if (s->has_eq)              sm_emit_si(p, SM_PUSH_LIT_S, "", 0);
        else                             sm_emit_i(p, SM_PUSH_LIT_I, 0);
        const char *sname = (s->subject && (s->subject->kind == AST_VAR
                              || s->subject->kind == AST_KEYWORD)) ? s->subject->sval : NULL;
        sm_emit_si(p, SM_EXEC_STMT, sname, (int64_t)s->has_eq);
        goto emit_gotos;
    }

    if (s->subject) {
        if (s->has_eq) {
            if (s->replacement) lower_expr(c, s->replacement); else sm_emit(p, SM_PUSH_NULL);
            const AST_t *lhs = s->subject;
            if (lhs->kind == AST_VAR || lhs->kind == AST_KEYWORD) {
                sm_emit_s(p, SM_STORE_VAR, lhs->sval ? lhs->sval : "");
            } else if (lhs->kind == AST_INDIRECT) {
                lower_expr(c, T0(lhs)); sm_emit_si(p, SM_CALL_FN, "ASGN_INDIR", 2);
            } else if (lhs->kind == AST_IDX) {
                for (int i = 0; i < lhs->nchildren; i++) lower_expr(c, lhs->children[i]);
                sm_emit_si(p, SM_CALL_FN, "IDX_SET", (int64_t)(lhs->nchildren + 1));
            } else if (lhs->kind == AST_FNC && lhs->sval) {
                if (lhs->nchildren == 0) {
                    sm_emit_si(p, SM_CALL_FN, "NRETURN_ASGN", 1);
                    p->instrs[p->count - 1].a[1].s = GC_strdup(lhs->sval);
                } else if (strcasecmp(lhs->sval, "ITEM") == 0) {
                    for (int i = 0; i < lhs->nchildren; i++) lower_expr(c, lhs->children[i]);
                    sm_emit_si(p, SM_CALL_FN, "ITEM_SET", (int64_t)(lhs->nchildren + 1));
                } else {
                    lower_expr(c, T0(lhs));
                    char set[256]; snprintf(set, sizeof set, "%s_SET", lhs->sval);
                    sm_emit_si(p, SM_CALL_FN, set, 2);
                }
            } else {
                lower_expr(c, lhs); sm_emit_si(p, SM_CALL_FN, "ASGN", 2);
            }
        } else {
            /* Bare expression — RETURN/FRETURN/NRETURN handled as jump. */
            if (s->subject->kind == AST_VAR && s->subject->sval) {
                if (strcasecmp(s->subject->sval, "RETURN")  == 0) { sm_emit(p, SM_RETURN);  goto emit_gotos; }
                if (strcasecmp(s->subject->sval, "FRETURN") == 0) { sm_emit(p, SM_FRETURN); goto emit_gotos; }
                if (strcasecmp(s->subject->sval, "NRETURN") == 0) { sm_emit(p, SM_NRETURN); goto emit_gotos; }
            }
            lower_expr(c, s->subject); sm_emit(p, SM_VOID_POP);
        }
    }

emit_gotos:
    if (!s->goto_u && !s->goto_u_expr && !s->goto_s && !s->goto_s_expr
            && !s->goto_f && !s->goto_f_expr) return;
    if (s->goto_u && s->goto_u[0]) { emit_goto(c, SM_JUMP, s->goto_u); return; }
    if (s->goto_u_expr) { sm_emit_s(p, SM_PUSH_LIT_S, "(computed-goto)"); sm_emit(p, SM_JUMP_INDIR); return; }
    if (s->goto_s && s->goto_s[0]) emit_goto(c, SM_JUMP_S, s->goto_s);
    if (s->goto_f && s->goto_f[0]) emit_goto(c, SM_JUMP_F, s->goto_f);
}

/*── Expression dispatcher ────────────────────────────────────────────────────
 * One switch — the compiler sees all cases, warns on missing ones (-Wswitch).
 * Pattern primitives all delegate to lower_pat_expr (they carry no extra state).
 * AST_REVASSIGN / AST_REVSWAP fall to default until implemented.
 *────────────────────────────────────────────────────────────────────────────*/
void lower_expr(LowerCtx *c, const AST_t *t)
{
    if (!t) { sm_emit(c->p, SM_PUSH_NULL); return; }
    switch (t->kind) {
    /* literals */
    case AST_QLIT: case AST_CSET:              lower_qlit(c, t);          return;
    case AST_ILIT:                             lower_ilit(c, t);          return;
    case AST_FLIT:                             lower_flit(c, t);          return;
    case AST_NUL:                              lower_nul(c, t);           return;
    /* references */
    case AST_VAR:                              lower_var(c, t);           return;
    case AST_KEYWORD:                          lower_keyword(c, t);       return;
    case AST_INDIRECT:                         lower_indirect(c, t);      return;
    case AST_DEFER:                            lower_defer(c, t);         return;
    /* arithmetic */
    case AST_INTERROGATE:                      lower_interrogate(c, t);   return;
    case AST_NAME:                             lower_name(c, t);          return;
    case AST_MNS:   lower_mns(c, t);   return;    case AST_PLS: lower_pls(c, t); return;
    case AST_ADD:   lower_add(c, t);   return;    case AST_SUB: lower_sub(c, t); return;
    case AST_MUL:   lower_mul(c, t);   return;    case AST_DIV: lower_div(c, t); return;
    case AST_MOD:   lower_mod(c, t);   return;    case AST_POW: lower_pow(c, t); return;
    /* sequences */
    case AST_VLIST:                            lower_vlist(c, t);         return;
    case AST_CAT: case AST_SEQ:                lower_cat_seq(c, t);       return;
    case AST_ALT:                              lower_alt(c, t);           return;
    case AST_OPSYN:                            lower_opsyn(c, t);         return;
    /* pattern primitives — delegate to lower_pat_expr */
    case AST_ARB:    case AST_ARBNO:  case AST_POS:    case AST_RPOS:
    case AST_ANY:    case AST_NOTANY: case AST_SPAN:   case AST_BREAK:  case AST_BREAKX:
    case AST_LEN:    case AST_TAB:    case AST_RTAB:   case AST_REM:
    case AST_FAIL:   case AST_SUCCEED:case AST_FENCE:  case AST_ABORT:  case AST_BAL:
    case AST_CAPT_COND_ASGN: case AST_CAPT_IMMED_ASGN: case AST_CAPT_CURSOR:
                                               lower_pat_expr(c, t);      return;
    /* calls */
    case AST_FNC:                              lower_fnc(c, t);           return;
    case AST_IDX:                              lower_idx(c, t);           return;
    case AST_ASSIGN:                           lower_assign(c, t);        return;
    case AST_SCAN:                             lower_scan(c, t);          return;
    case AST_SWAP:                             lower_swap(c, t);          return;
    /* relops */
    case AST_LT: case AST_LE: case AST_GT: case AST_GE: case AST_EQ: case AST_NE:
                                               lower_acomp(c, t);         return;
    case AST_LLT: case AST_LLE: case AST_LGT: case AST_LGE: case AST_LEQ: case AST_LNE:
                                               lower_lcomp(c, t);         return;
    /* cset / list */
    case AST_CSET_COMPL: case AST_CSET_UNION: case AST_CSET_DIFF: case AST_CSET_INTER:
                                               lower_cset_op(c, t);       return;
    case AST_LCONCAT:                          lower_lconcat(c, t);       return;
    /* unary Icon */
    case AST_NONNULL:                          lower_nonnull(c, t);       return;
    case AST_NULL:                             lower_null(c, t);          return;
    case AST_NOT:                              lower_not(c, t);           return;
    case AST_SIZE:                             lower_size(c, t);          return;
    case AST_RANDOM:                           lower_random(c, t);        return;
    case AST_IDENTICAL:                        lower_identical(c, t);     return;
    case AST_AUGOP:                            lower_augop(c, t);         return;
    /* control */
    case AST_SEQ_EXPR:                         lower_seq_expr(c, t);      return;
    case AST_IF:                               lower_if(c, t);            return;
    case AST_WHILE:                            lower_while(c, t);         return;
    case AST_UNTIL:                            lower_until(c, t);         return;
    case AST_REPEAT:                           lower_repeat(c, t);        return;
    case AST_LOOP_BREAK:                       lower_loop_break(c, t);    return;
    case AST_LOOP_NEXT:                        lower_loop_next(c, t);     return;
    case AST_RETURN:                           lower_return(c, t);        return;
    case AST_PROC_FAIL:                        lower_proc_fail(c, t);     return;
    case AST_CASE:                             lower_case(c, t);          return;
    /* data */
    case AST_MAKELIST:                         lower_makelist(c, t);      return;
    case AST_RECORD:                           lower_record(c, t);        return;
    case AST_FIELD:                            lower_field(c, t);         return;
    case AST_GLOBAL:                           lower_global(c, t);        return;
    case AST_INITIAL:                          lower_initial(c, t);       return;
    /* sections */
    case AST_SECTION:                          lower_section(c, t);       return;
    case AST_SECTION_PLUS:                     lower_section_plus(c, t);  return;
    case AST_SECTION_MINUS:                    lower_section_minus(c, t); return;
    case AST_BANG_BINARY:                      lower_bang_binary(c, t);   return;
    /* generators */
    case AST_SUSPEND:                          lower_suspend(c, t);       return;
    case AST_TO:                               lower_to(c, t);            return;
    case AST_TO_BY:                            lower_to_by(c, t);         return;
    case AST_LIMIT:                            lower_limit(c, t);         return;
    case AST_ALTERNATE: case AST_ITERATE:      lower_bb_pump_ast(c, t);   return;
    case AST_EVERY:                            lower_every(c, t);         return;
    /* Prolog */
    case AST_CHOICE:                           lower_choice(c, t);        return;
    case AST_CLAUSE: case AST_CUT: case AST_UNIFY:
    case AST_TRAIL_MARK: case AST_TRAIL_UNWIND: lower_prolog_child(c, t); return;
    /* not yet implemented (AST_REVASSIGN, AST_REVSWAP) */
    default:                                   lower_unhandled(c, t);     return;
    }
}

/*── Procedure skeletons ─────────────────────────────────────────────────────*/

static void build_proc_scope(IcnScope *sc, const AST_t *proc, int body_start)
{
    int nparams = (int)proc->ival;
    sc->n = 0;
    for (int i = 0; i < nparams && i < FRAME_SLOT_MAX; i++) {
        AST_t *pn = proc->children[1+i];
        if (pn && pn->sval) scope_add(sc, pn->sval);
    }
    for (int i = body_start; i < proc->nchildren; i++)
        expression_scope_walk(sc, proc->children[i]);
    /* Remove names assigned in initial{} — they must use NV, not frame slots. */
    for (int i = body_start; i < proc->nchildren; i++) {
        AST_t *ch = proc->children[i];
        if (!ch || ch->kind != AST_INITIAL) continue;
        for (int ai = 0; ai < ch->nchildren; ai++) {
            AST_t *as = ch->children[ai];
            if (!as || as->kind != AST_ASSIGN || as->nchildren < 1) continue;
            AST_t *lhs = as->children[0];
            if (!lhs || lhs->kind != AST_VAR || !lhs->sval) continue;
            int w = 0;
            for (int r = 0; r < sc->n; r++) {
                if (sc->e[r].name && strcmp(sc->e[r].name, lhs->sval) == 0) continue;
                if (w != r) sc->e[w] = sc->e[r]; w++;
            }
            sc->n = w;
            for (int k = 0; k < sc->n; k++) sc->e[k].slot = k;
        }
    }
}

static void lower_proc_skeletons(LowerCtx *c)
{
    SM_Program *p = c->p;

    for (int pi = 0; pi < proc_count; pi++) {
        const char *nm = proc_table[pi].name;
        if (!nm || !*nm) continue;
        AST_t *proc = proc_table[pi].proc;
        int skip = sm_emit_i(p, SM_JUMP, 0);
        sm_label_named(p, nm);
        if (proc) {
            int body_start = 1 + (int)proc->ival;
            IcnScope sc; build_proc_scope(&sc, proc, body_start);
            c->expression_scope = &sc; c->expression_body_lowering = 1;
            for (int i = body_start; i < proc->nchildren; i++) {
                if (!proc->children[i]) continue;
                lower_expr(c, proc->children[i]); sm_emit(p, SM_VOID_POP);
            }
            /* CH-17g-proc-locals: store finalized scope so sm_call_proc can
             * icn_scope_patch() body AST nodes for every-body AST walker. */
            proc_table[pi].lower_sc = sc;
            c->expression_body_lowering = 0; c->expression_scope = NULL;
        }
        sm_emit(p, SM_RETURN);
        sm_patch_jump(p, skip, sm_label(p));
    }

    for (int b = 0; b < PL_PRED_TABLE_SIZE_FWD; b++) {
        for (Pl_PredEntry *e = g_pl_pred_table.buckets[b]; e; e = e->next) {
            if (!e->key || !*e->key) continue;
            int skip = sm_emit_i(p, SM_JUMP, 0);
            sm_label_named(p, e->key);
            sm_emit(p, SM_RETURN);
            sm_patch_jump(p, skip, sm_label(p));
        }
    }
}

/*── Public entry point ──────────────────────────────────────────────────────*/

SM_Program *lower(const CODE_t *prog)
{
    if (!prog) return NULL;

    LowerCtx ctx = {
        .p = sm_prog_new(),
        .expression_body_lowering = 0,
        .expression_scope = NULL,
    };
    labtab_init(&ctx.labtab);
    for (int i = 0; i < LOWER_UNHANDLED_WORDS; i++) ctx.unhandled_kinds[i] = 0;

    LowerCtx *c = &ctx; SM_Program *p = ctx.p; LabelTable *labtab = &ctx.labtab;

    lower_proc_skeletons(c);

    int stno = 0, has_icn = 0;
    for (const STMT_t *s = prog->head; s; s = s->next) {
        if (s->lang == LANG_ICN) { has_icn = 1; sm_stno_label_record(p, ++stno, NULL); continue; }
        lower_stmt(c, s);
        sm_stno_label_record(p, ++stno, (s->label && s->label[0]) ? s->label : NULL);
    }

    if (has_icn) sm_emit_si(p, SM_BB_PUMP_PROC, "main", 0);
    if (p->count == 0 || p->instrs[p->count - 1].op != SM_HALT) sm_emit(p, SM_HALT);

    labtab_resolve(labtab, p);
    labtab_free(labtab);

    /* Report any unhandled AST kinds — normally silent. */
    int any = 0;
    for (int w = 0; w < LOWER_UNHANDLED_WORDS; w++) if (ctx.unhandled_kinds[w]) { any = 1; break; }
    if (any) {
        fprintf(stderr, "sm_lower: unhandled AST kinds:");
        for (int k = 0; k < AST_KIND_COUNT; k++) {
            int w = k/64, b = k%64;
            if (w < LOWER_UNHANDLED_WORDS && (ctx.unhandled_kinds[w] >> b) & 1)
                fprintf(stderr, " %s", ast_e_name[k]);
        }
        fprintf(stderr, "\n");
    }

    return p;
}
