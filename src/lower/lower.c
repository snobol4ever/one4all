#define IR_DEFINE_NAMES
#include "lower.h"
#include "lower_ctx.h"
#include "sm_prog.h"
#include "sm_interp.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "../ast/ast.h"
#include "ast_clone.h"
#include "lower_pat_dcg.h"
#include "lower_icn.h"
#include "../../runtime/interp/icn_runtime.h"
#include "../../runtime/interp/pl_runtime.h"
#include "../../frontend/icon/icon_lex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <gc/gc.h>
#include "snobol4.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_pat_nary(const tree_t *t, sm_opcode_t op);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void lower_expr    (const tree_t *t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void lower_pat_expr(const tree_t *t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void lower_stmt    (const tree_t *s);
#define LOWER_UNHANDLED_WORDS 4
#define ICN_BB_EVAL(t) do { if (g_lang == LANG_ICN) { sm_emit_i(g_p, SM_BB_EVAL, (int64_t)every_table_register((tree_t *)(t))); return; } } while(0)
static SM_Program  *g_p;
static LabelTable   g_labtab;
static int          g_in_proc_body;
static IcnScope    *g_proc_scope;
static unsigned long long g_unhandled_kinds[LOWER_UNHANDLED_WORDS];
static int          g_in_value_ctx;
static const tree_t *g_hoist_alt   = NULL;
static int           g_hoist_entry = -1;
static int           g_hoist_slot  = -1;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int lower_is_suspendable_icn(const tree_t *e)
{
    if (!e) return 0;
    if (e->t == TT_SEQ) {
        for (int i = 0; i < e->n; i++)
            if (lower_is_suspendable_icn(e->c[i])) return 1;
        return 0;
    }
    return is_suspendable((tree_t *)e);
}
extern int g_lang;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_push_expr(const tree_t *t)
{
    if (!t) { sm_emit(g_p, SM_PUSH_NULL); return; }
    sm_emit_ptr(g_p, SM_PUSH_EXPR, (void *)ast_gc_clone(t));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_unhandled(const tree_t *t)
{
    if (!g_in_proc_body && t->t >= 0 && t->t < TT_KIND_COUNT) {
        int w = t->t / 64, b = t->t % 64;
        if (w < LOWER_UNHANDLED_WORDS) g_unhandled_kinds[w] |= (1ULL << b);
    }
    sm_emit(g_p, SM_PUSH_NULL);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_goto(sm_opcode_t op, const char *target)
{
    if (!target) return -1;
    static const struct {
        const char *name;
        sm_opcode_t plain, succ, fail;
    } ret_kinds[] = {
        { "RETURN",  SM_RETURN,  SM_RETURN_S,  SM_RETURN_F  },
        { "FRETURN", SM_FRETURN, SM_FRETURN_S, SM_FRETURN_F },
        { "NRETURN", SM_NRETURN, SM_NRETURN_S, SM_NRETURN_F },
    };
    for (unsigned i = 0; i < sizeof ret_kinds / sizeof ret_kinds[0]; i++) {
        if (strcasecmp(target, ret_kinds[i].name) == 0) {
            sm_opcode_t emit_op = (op == SM_JUMP_S) ? ret_kinds[i].succ
                                : (op == SM_JUMP_F) ? ret_kinds[i].fail
                                :                     ret_kinds[i].plain;
            return sm_emit(g_p, emit_op);
        }
    }
    int idx = sm_emit_i(g_p, op, 0);
    int resolved = labtab_find(&g_labtab, target);
    if (resolved >= 0) sm_patch_jump(g_p, idx, resolved);
    else               labtab_patch_later(&g_labtab, idx, target);
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_var_load(const char *vn)
{
    if (g_in_proc_body && g_proc_scope && vn[0] && vn[0] != '&') {
        if (g_lang == LANG_ICN) {
            for (int _pi = 0; _pi < proc_count; _pi++) {
                if (proc_table[_pi].name && strcmp(proc_table[_pi].name, vn) == 0) {
                    sm_emit_s(g_p, SM_PUSH_VAR, vn);
                    return;
                }
            }
            extern DESCR_t icn_proc_as_value(const char *);
            DESCR_t pv = icn_proc_as_value(vn);
            if (pv.v == DT_S) {
                sm_emit_s(g_p, SM_PUSH_VAR, vn);
                return;
            }
        }
        int slot = scope_get(g_proc_scope, vn);
        if (slot >= 0) { sm_emit_i(g_p, SM_LOAD_FRAME, slot); return; }
    }
    sm_emit_s(g_p, SM_PUSH_VAR, vn);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_var_store(const char *vn)
{
    if (g_in_proc_body && g_proc_scope && vn[0] && vn[0] != '&') {
        int slot = scope_get(g_proc_scope, vn);
        if (slot >= 0) { sm_emit_i(g_p, SM_STORE_FRAME, slot); return; }
    }
    sm_emit_s(g_p, SM_STORE_VAR, vn);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_thunk(const tree_t *body)
{
    int skip = sm_emit_i(g_p, SM_JUMP, 0);
    int entry = sm_label(g_p);
    if (body) lower_expr(body); else sm_emit(g_p, SM_PUSH_NULL);
    sm_emit(g_p, SM_RETURN);
    sm_patch_jump(g_p, skip, sm_label(g_p));
    sm_emit_ii(g_p, SM_PUSH_EXPRESSION, (int64_t)entry, 0);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_strlit(const tree_t *t) { ICN_BB_EVAL(t); sm_emit_s(g_p, SM_PUSH_LIT_S, t->v.sval ? t->v.sval : ""); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_ilit  (const tree_t *t) { ICN_BB_EVAL(t); sm_emit_i(g_p, SM_PUSH_LIT_I, (int64_t)t->v.ival); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_flit  (const tree_t *t) { ICN_BB_EVAL(t); sm_emit_f(g_p, SM_PUSH_LIT_F, t->v.dval); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_nul   (const tree_t *t) { ICN_BB_EVAL(t); (void)t; sm_emit(g_p, SM_PUSH_NULL); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_var(const tree_t *t)     { ICN_BB_EVAL(t); emit_var_load(t->v.sval ? t->v.sval : ""); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_keyword(const tree_t *t) { ICN_BB_EVAL(t); sm_emit_s(g_p, SM_PUSH_VAR, kw_canonicalize(t->v.sval)); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_indirect(const tree_t *t)
{
    ICN_BB_EVAL(t);
    const tree_t *ch = T0(t);
    if (ch && ch->t == TT_NAME && ch->n == 1) {
        const tree_t *inner = ch->c[0];
        if (inner && inner->t == TT_IDX && inner->n >= 2
                && inner->c[0] && inner->c[0]->t == TT_VAR
                && inner->c[0]->v.sval) {
            sm_emit_s(g_p, SM_PUSH_VAR, inner->c[0]->v.sval);
            for (int i = 1; i < inner->n; i++) lower_expr( inner->c[i]);
            sm_emit_si(g_p, SM_CALL_FN, "IDX", (int64_t)inner->n);
            return;
        }
    }
    lower_expr(ch);
    sm_emit_si(g_p, SM_CALL_FN, "INDIR_GET", 1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_defer(const tree_t *t)
{
    emit_thunk(T0(t));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_interrogate(const tree_t *t) { ICN_BB_EVAL(t); lower_expr(T0(t)); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_name(const tree_t *t)
{
    ICN_BB_EVAL(t);
    const char *vname = (T0(t) && T0(t)->v.sval) ? T0(t)->v.sval : "";
    sm_emit_s(g_p, SM_PUSH_LIT_S, vname);
    sm_emit_si(g_p, SM_CALL_FN, "NAME_PUSH", 1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_mns(const tree_t *t) { ICN_BB_EVAL(t); LOWER1_VAL(SM_NEG); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_pls(const tree_t *t) { ICN_BB_EVAL(t); LOWER1_VAL(SM_COERCE_NUM); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_add(const tree_t *t) { ICN_BB_EVAL(t); lower_expr(T0(t)); sm_emit(g_p, SM_COERCE_NUM); lower_expr(T1(t)); sm_emit(g_p, SM_COERCE_NUM); sm_emit(g_p, SM_ADD); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_sub(const tree_t *t) { ICN_BB_EVAL(t); lower_expr(T0(t)); sm_emit(g_p, SM_COERCE_NUM); lower_expr(T1(t)); sm_emit(g_p, SM_COERCE_NUM); sm_emit(g_p, SM_SUB); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_mul(const tree_t *t) { ICN_BB_EVAL(t); lower_expr(T0(t)); sm_emit(g_p, SM_COERCE_NUM); lower_expr(T1(t)); sm_emit(g_p, SM_COERCE_NUM); sm_emit(g_p, SM_MUL); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_div(const tree_t *t) { ICN_BB_EVAL(t); lower_expr(T0(t)); sm_emit(g_p, SM_COERCE_NUM); lower_expr(T1(t)); sm_emit(g_p, SM_COERCE_NUM); sm_emit(g_p, SM_DIV); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_mod(const tree_t *t) { ICN_BB_EVAL(t); lower_expr(T0(t)); sm_emit(g_p, SM_COERCE_NUM); lower_expr(T1(t)); sm_emit(g_p, SM_COERCE_NUM); sm_emit(g_p, SM_MOD); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_pow(const tree_t *t) { ICN_BB_EVAL(t); LOWER2(SM_EXP); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_vlist(const tree_t *t)
{
    ICN_BB_EVAL(t);
    if (t->n == 0) { sm_emit(g_p, SM_PUSH_NULL); return; }
    if (t->n == 1) { lower_expr(t->c[0]); return; }
    int n = t->n - 1;
    int *jumps = (int *)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < t->n; i++) {
        lower_expr(t->c[i]);
        if (i < t->n - 1) { jumps[i] = sm_emit_i(g_p, SM_JUMP_S, 0); sm_emit(g_p, SM_VOID_POP); }
    }
    int done = sm_label(g_p);
    for (int i = 0; i < n; i++) sm_patch_jump(g_p, jumps[i], done);
    free(jumps);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_cat_seq(const tree_t *t)
{
    ICN_BB_EVAL(t);
    if (t->t == TT_SEQ && g_lang == LANG_ICN) {
        if (t->n == 0) { sm_emit(g_p, SM_PUSH_NULL); return; }
        if (t->n == 1) { lower_expr(t->c[0]); return; }
        int njumps = t->n - 1;
        int *fail_jumps = (int *)GC_MALLOC((size_t)njumps * sizeof(int));
        for (int i = 0; i < t->n; i++) {
            lower_expr(t->c[i]);
            if (i < t->n - 1) {
                fail_jumps[i] = sm_emit_i(g_p, SM_JUMP_F, 0);
                sm_emit(g_p, SM_VOID_POP);
            }
        }
        int done_lbl = sm_label(g_p);
        for (int i = 0; i < njumps; i++) sm_patch_jump(g_p, fail_jumps[i], done_lbl);
        return;
    }
    int has_defer = 0;
    for (int i = 0; i < t->n && !has_defer; i++)
        if (t->c[i] && t->c[i]->t == TT_DEFER) has_defer = 1;
    if (has_defer) {
        emit_pat_nary(t, SM_PAT_CAT);
    } else {
        for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
        for (int i = 1; i < t->n; i++) sm_emit(g_p, SM_CONCAT);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_opsyn(const tree_t *t)
{
    ICN_BB_EVAL(t);
    const char *raw = t->v.sval ? t->v.sval : "&";
    char op_buf[4];
    const char *op = raw;
    const char *lp = strchr(raw, '(');
    if (lp && lp[1] && lp[2] == ')') { op_buf[0] = lp[1]; op_buf[1] = '\0'; op = op_buf; }
    else if (strcmp(raw, "BARFN")  == 0) op = "|";
    else if (strcmp(raw, "AROWFN") == 0) op = "^";
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(g_p, SM_CALL_FN, op, (int64_t)t->n);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *sm_pat_capture_fn_arg_names(const tree_t *fnc)
{
    if (!fnc || fnc->n <= 0) return NULL;
    size_t len = 0;
    for (int i = 0; i < fnc->n; i++) {
        const tree_t *a = fnc->c[i];
        if (!a || a->t != TT_VAR || !a->v.sval) return NULL;
        len += strlen(a->v.sval) + 1;
    }
    char *buf = GC_MALLOC(len), *q = buf;
    for (int i = 0; i < fnc->n; i++) {
        const char *nm = fnc->c[i]->v.sval;
        size_t n = strlen(nm);
        memcpy(q, nm, n); q += n;
        *q++ = (i + 1 < fnc->n) ? '\t' : '\0';
    }
    return buf;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_pat_fn_args(const tree_t *fnc)
{
    for (int i = 0; i < fnc->n; i++) {
        tree_t *arg = fnc->c[i];
        if (arg && arg->t == TT_QLIT) lower_expr(arg);
        else                              emit_thunk(arg);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_pat_capture(const tree_t *var_node, int mode)
{
    if (var_node && var_node->t == TT_DEFER
            && var_node->n > 0
            && var_node->c[0]
            && var_node->c[0]->t == TT_FNC
            && var_node->c[0]->v.sval) {
        const tree_t *fnc = var_node->c[0];
        const char *names = sm_pat_capture_fn_arg_names(fnc);
        if (names || fnc->n == 0) {
            int idx = sm_emit_s(g_p, SM_PAT_CAPTURE_FN, fnc->v.sval);
            g_p->instrs[idx].a[1].i = mode; g_p->instrs[idx].a[2].s = names;
        } else {
            emit_pat_fn_args(fnc);
            int idx = sm_emit_s(g_p, SM_PAT_CAPTURE_FN_ARGS, fnc->v.sval);
            g_p->instrs[idx].a[1].i = mode; g_p->instrs[idx].a[2].i = fnc->n;
        }
    } else {
        int idx = sm_emit_s(g_p, SM_PAT_CAPTURE, var_node ? var_node->v.sval : "");
        g_p->instrs[idx].a[1].i = mode;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_pat_nary(const tree_t *t, sm_opcode_t op)
{
    for (int i = 0; i < t->n; i++) lower_pat_expr(t->c[i]);
    for (int i = 1; i < t->n; i++) sm_emit(g_p, op);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void lower_pat_expr(const tree_t *t)
{
    if (!t) return;
    switch (t->t) {
    case TT_QLIT:  sm_emit_s(g_p, SM_PAT_LIT, t->v.sval ? t->v.sval : ""); return;
    case TT_VAR:   sm_emit_s(g_p, SM_PUSH_VAR, t->v.sval); sm_emit(g_p, SM_PAT_DEREF); return;
    case TT_ARB:   sm_emit(g_p, SM_PAT_ARB);    return;
    case TT_REM:   sm_emit(g_p, SM_PAT_REM);    return;
    case TT_FAIL:  sm_emit(g_p, SM_PAT_FAIL);   return;
    case TT_SUCCEED: sm_emit(g_p, SM_PAT_SUCCEED); return;
    case TT_ABORT: sm_emit(g_p, SM_PAT_ABORT);  return;
    case TT_BAL:   sm_emit(g_p, SM_PAT_BAL);    return;
    case TT_FENCE:
        if (t->n > 0) { lower_pat_expr(t->c[0]); sm_emit(g_p, SM_PAT_FENCE1); }
        else                    sm_emit(g_p, SM_PAT_FENCE0);
        return;
    case TT_ANY:    lower_expr(T0(t)); sm_emit(g_p, SM_PAT_ANY);    return;
    case TT_NOTANY: lower_expr(T0(t)); sm_emit(g_p, SM_PAT_NOTANY); return;
    case TT_SPAN:   lower_expr(T0(t)); sm_emit(g_p, SM_PAT_SPAN);   return;
    case TT_BREAK:  lower_expr(T0(t)); sm_emit(g_p, SM_PAT_BREAK);  return;
    case TT_BREAKX: lower_expr(T0(t)); sm_emit(g_p, SM_PAT_BREAK);  return;
    case TT_LEN:    lower_expr(T0(t)); sm_emit(g_p, SM_PAT_LEN);    return;
    case TT_POS:    lower_expr(T0(t)); sm_emit(g_p, SM_PAT_POS);    return;
    case TT_RPOS:   lower_expr(T0(t)); sm_emit(g_p, SM_PAT_RPOS);   return;
    case TT_TAB:    lower_expr(T0(t)); sm_emit(g_p, SM_PAT_TAB);    return;
    case TT_RTAB:   lower_expr(T0(t)); sm_emit(g_p, SM_PAT_RTAB);   return;
    case TT_ARBNO:  lower_pat_expr(T0(t)); sm_emit(g_p, SM_PAT_ARBNO); return;
    case TT_SEQ:
    case TT_CAT:  emit_pat_nary(t, SM_PAT_CAT); return;
    case TT_ALT:  emit_pat_nary(t, SM_PAT_ALT); return;
    case TT_CAPT_COND_ASGN:
        lower_pat_expr(T0(t));
        if (t->n > 1) emit_pat_capture(t->c[1], 0);
        return;
    case TT_CAPT_IMMED_ASGN:
        lower_pat_expr(T0(t));
        if (t->n > 1) emit_pat_capture(t->c[1], 1);
        return;
    case TT_CAPT_CURSOR:
        if (t->n == 1) {
            sm_emit(g_p, SM_PAT_EPS);
            emit_pat_capture(t->c[0], 2);
        } else {
            lower_pat_expr(T0(t));
            if (t->n > 1) emit_pat_capture(t->c[1], 2);
        }
        return;
    case TT_DEFER: {
        const tree_t *ch = T0(t);
        if (ch && ch->t == TT_FNC && ch->v.sval) {
            if (ch->n == 0) {
                int idx = sm_emit_s(g_p, SM_PAT_USERCALL, ch->v.sval);
                g_p->instrs[idx].a[2].s = NULL;
            } else {
                emit_pat_fn_args(ch);
                int idx = sm_emit_s(g_p, SM_PAT_USERCALL_ARGS, ch->v.sval);
                g_p->instrs[idx].a[1].i = ch->n;
            }
            return;
        }
        if (ch && ch->t == TT_VAR && ch->v.sval) { sm_emit_s(g_p, SM_PAT_REFNAME, ch->v.sval); return; }
        lower_expr(ch); sm_emit(g_p, SM_PAT_DEREF);
        return;
    }
    default:
        lower_expr(t); sm_emit(g_p, SM_PAT_DEREF);
        return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_fnc(const tree_t *t)
{
    int nargs = t->n;
    if (nargs == 1 && t->v.sval && strcmp(t->v.sval, "EVAL") == 0
            && t->c[0] && t->c[0]->t == TT_DEFER) {
        emit_thunk(T0(t->c[0]));
        g_p->instrs[g_p->count - 1].op = SM_CALL_EXPRESSION;
        return;
    }
    {
        int is_upto = 0;
        const char *cset = NULL, *hay = NULL;
        if (t->v.sval && strcmp(t->v.sval, "upto") == 0 && nargs >= 2) {
            if (t->c[0] && (t->c[0]->t == TT_CSET || t->c[0]->t == TT_QLIT) &&
                t->c[1] && (t->c[1]->t == TT_QLIT || t->c[1]->t == TT_CSET)) {
                cset = t->c[0]->v.sval ? t->c[0]->v.sval : "";
                hay  = t->c[1]->v.sval ? t->c[1]->v.sval : "";
                is_upto = 1;
            }
        }
        if (!t->v.sval && nargs >= 3 && t->c[0] && t->c[0]->v.sval
                && strcmp(t->c[0]->v.sval, "upto") == 0) {
            if (t->c[1] && (t->c[1]->t == TT_CSET || t->c[1]->t == TT_QLIT) &&
                t->c[2] && (t->c[2]->t == TT_QLIT || t->c[2]->t == TT_CSET)) {
                cset = t->c[1]->v.sval ? t->c[1]->v.sval : "";
                hay  = t->c[2]->v.sval ? t->c[2]->v.sval : "";
                is_upto = 1;
            }
        }
        if (g_lang == LANG_ICN && is_upto && cset && hay) {
            IR_block_t *cfg = lower_icn_upto(cset, hay);
            if (cfg) {
                int dcg_idx = sm_prog_dcg_add(g_p, cfg);
                int idx = sm_emit_i(g_p, SM_EXEC_BB, (int64_t)dcg_idx);
                (void)idx;
                return;
            }
        }
    }
    if (t->v.sval) { ICN_BB_EVAL(t); }
    if (!t->v.sval && nargs >= 1 && t->c[0] && t->c[0]->v.sval) {
        const char *fn = t->c[0]->v.sval;
        for (int i = 1; i < nargs; i++) lower_expr(t->c[i]);
        sm_emit_si(g_p, SM_CALL_FN, fn, (int64_t)(nargs - 1));
        return;
    }
    for (int i = 0; i < nargs; i++) lower_expr(t->c[i]);
    sm_emit_si(g_p, SM_CALL_FN, t->v.sval ? t->v.sval : "", (int64_t)nargs);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_idx(const tree_t *t)
{
    ICN_BB_EVAL(t);
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(g_p, SM_CALL_FN, "IDX", (int64_t)t->n);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_lhs_store(const tree_t *lhs)
{
    if (!lhs) return;
    if (lhs->t == TT_VAR)     { emit_var_store(lhs->v.sval ? lhs->v.sval : ""); return; }
    if (lhs->t == TT_KEYWORD) { sm_emit_s(g_p, SM_STORE_VAR, kw_canonicalize(lhs->v.sval)); return; }
    if (lhs->t == TT_INDIRECT) {
        lower_expr(T0(lhs)); sm_emit_si(g_p, SM_CALL_FN, "ASGN_INDIR", 2); return;
    }
    if (lhs->t == TT_IDX) {
        for (int i = 0; i < lhs->n; i++) lower_expr(lhs->c[i]);
        sm_emit_si(g_p, SM_CALL_FN, "IDX_SET", (int64_t)(lhs->n + 1)); return;
    }
    if (lhs->t == TT_FNC && lhs->v.sval) {
        if (lhs->n == 0) {
            sm_emit_si(g_p, SM_CALL_FN, "NRETURN_ASGN", 1);
            g_p->instrs[g_p->count - 1].a[1].s = GC_strdup(lhs->v.sval);
        } else if (strcasecmp(lhs->v.sval, "ITEM") == 0) {
            for (int i = 0; i < lhs->n; i++) lower_expr(lhs->c[i]);
            sm_emit_si(g_p, SM_CALL_FN, "ITEM_SET", (int64_t)(lhs->n + 1));
        } else {
            lower_expr(T0(lhs));
            char set[256]; snprintf(set, sizeof set, "%s_SET", lhs->v.sval);
            sm_emit_si(g_p, SM_CALL_FN, set, 2);
        }
        return;
    }
    if (lhs->t == TT_FIELD) {
        lower_expr(T0(lhs));
        sm_emit_s(g_p, SM_PUSH_LIT_S, lhs->v.sval ? lhs->v.sval : "");
        sm_emit_si(g_p, SM_CALL_FN, "FIELD_SET", 3); return;
    }
    if (lhs->t == TT_RANDOM && lhs->n >= 1) {
        lower_expr(T0(lhs));
        sm_emit_si(g_p, SM_CALL_FN, "ICN_RANDOM_SET", 2); return;
    }
    if (lhs->t == TT_ITERATE && lhs->n >= 1) {
        tree_t *inner = T0(lhs);
        lower_expr(inner);
        if (inner->t == TT_VAR && inner->v.sval)
            sm_emit_s(g_p, SM_PUSH_LIT_S, inner->v.sval);
        else
            sm_emit(g_p, SM_PUSH_NULL);
        sm_emit_si(g_p, SM_CALL_FN, "ICN_ITERATE_FIRST_SET", 3); return;
    }
    if ((lhs->t == TT_SECTION || lhs->t == TT_SECTION_PLUS || lhs->t == TT_SECTION_MINUS) && lhs->n >= 3) {
        const char *fn = (lhs->t == TT_SECTION) ? "ICN_SECTION_RANGE_SET"
                       : (lhs->t == TT_SECTION_PLUS) ? "ICN_SECTION_PLUS_SET" : "ICN_SECTION_MINUS_SET";
        for (int i = 0; i < lhs->n; i++) lower_expr(lhs->c[i]);
        tree_t *_sb = lhs->c[0];
        if (_sb && _sb->t == TT_VAR && _sb->v.sval) sm_emit_s(g_p, SM_PUSH_LIT_S, _sb->v.sval);
        else sm_emit(g_p, SM_PUSH_NULL);
        sm_emit_si(g_p, SM_CALL_FN, fn, (int64_t)(lhs->n + 2)); return;
    }
    lower_expr(lhs);
    sm_emit_si(g_p, SM_CALL_FN, "ASGN", 2);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_assign(const tree_t *t)
{
    lower_expr(T1(t));
    emit_lhs_store(T0(t));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_scan(const tree_t *t)
{
    if (t->n < 1) { sm_emit(g_p, SM_PUSH_NULL); return; }
    lower_expr(t->c[0]);
    sm_emit_si(g_p, SM_CALL_FN, "ICN_SCAN_PUSH", 1);
    sm_emit(g_p, SM_VOID_POP);
    if (t->n > 1) lower_expr(t->c[1]); else sm_emit(g_p, SM_PUSH_NULL);
    sm_emit_si(g_p, SM_CALL_FN, "ICN_SCAN_POP", 1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_swap(const tree_t *t)
{
    if (t->n >= 2 && T0(t) && T1(t)
            && T0(t)->t == TT_VAR && T1(t)->t == TT_VAR) {
        const char *ln = T0(t)->v.sval ? T0(t)->v.sval : "";
        const char *rn = T1(t)->v.sval ? T1(t)->v.sval : "";
        if (ln[0] == '&' || rn[0] == '&') {
            const char *kw  = (ln[0] == '&') ? ln : rn;
            const char *var = (ln[0] == '&') ? rn : ln;
            int kw_is_lhs = (ln[0] == '&') ? 1 : 0;
            int var_slot = -1;
            if (g_in_proc_body && g_proc_scope && var[0] && var[0] != '&')
                var_slot = scope_get(g_proc_scope, var);
            emit_var_load(ln);  emit_var_load(rn);
            sm_emit_s(g_p, SM_PUSH_LIT_S, kw);
            sm_emit_s(g_p, SM_PUSH_LIT_S, var);
            sm_emit_i(g_p, SM_PUSH_LIT_I, (int64_t)var_slot);
            sm_emit_i(g_p, SM_PUSH_LIT_I, (int64_t)kw_is_lhs);
            sm_emit_si(g_p, SM_CALL_FN, "ICN_KW_SWAP", 6);
            return;
        }
        emit_var_load( ln); sm_emit_s(g_p, SM_STORE_VAR, "__icn_swap_tmp__"); sm_emit(g_p, SM_VOID_POP);
        emit_var_load( rn); emit_var_store( ln);
        sm_emit_s(g_p, SM_PUSH_VAR, "__icn_swap_tmp__"); emit_var_store( rn);
        sm_emit(g_p, SM_VOID_POP);
        return;
    }
    lower_expr(T0(t)); lower_expr(T1(t));
    sm_emit_si(g_p, SM_CALL_FN, "SWAP", 2);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_comp(const tree_t *t, sm_opcode_t op)
{
    ICN_BB_EVAL(t);
    lower_expr(T0(t)); lower_expr(T1(t));
    sm_emit_i(g_p, op, (int64_t)t->t);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_acomp(const tree_t *t) { lower_comp(t, SM_ACOMP); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_lcomp(const tree_t *t) { lower_comp(t, SM_LCOMP); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_lconcat(const tree_t *t)
{
    ICN_BB_EVAL(t);
    int has_gen = 0;
    for (int i = 0; i < t->n && !has_gen; i++)
        if (is_suspendable(t->c[i])) has_gen = 1;
    if (!has_gen) {
        if (t->n < 1) { sm_emit(g_p, SM_PUSH_NULL); return; }
        for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
        for (int i = 1; i < t->n; i++) sm_emit(g_p, SM_CONCAT);
        return;
    }
    lower_unhandled(t);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_nonnull (const tree_t *t) { ICN_BB_EVAL(t); lower_expr(T0(t)); sm_emit_si(g_p, SM_CALL_FN, "NONNULL",    1); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_null    (const tree_t *t) { ICN_BB_EVAL(t); lower_expr(T0(t)); sm_emit_si(g_p, SM_CALL_FN, "ICN_NULL",  1); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_size    (const tree_t *t) { ICN_BB_EVAL(t); lower_expr(T0(t)); sm_emit_si(g_p, SM_CALL_FN, "SIZE",      1); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_identical(const tree_t *t){ ICN_BB_EVAL(t); lower_expr(T0(t)); lower_expr(T1(t)); sm_emit_si(g_p, SM_CALL_FN, "IDENTICAL", 2); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_random  (const tree_t *t)
{
    ICN_BB_EVAL(t);
    if (t->n >= 1) { lower_expr(T0(t)); sm_emit_si(g_p, SM_CALL_FN, "ICN_RANDOM", 1); }
    else           { sm_emit(g_p, SM_PUSH_NULL); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_not(const tree_t *t)
{
    ICN_BB_EVAL(t);
    lower_expr(T0(t));
    int js = sm_emit_i(g_p, SM_JUMP_S, 0);
    sm_emit(g_p, SM_VOID_POP); sm_emit(g_p, SM_PUSH_NULL);
    int jend = sm_emit_i(g_p, SM_JUMP, 0);
    int flbl = sm_label(g_p); sm_patch_jump(g_p, js, flbl);
    sm_emit(g_p, SM_VOID_POP); sm_emit_si(g_p, SM_CALL_FN, "FAIL", 0);
    sm_patch_jump(g_p, jend, sm_label(g_p));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_augop_store(int lslot, int is_kw, const char *lname)
{
    if      (lslot >= 0) sm_emit_i(g_p, SM_STORE_FRAME, lslot);
    else if (is_kw)      sm_emit_s(g_p, SM_STORE_VAR, kw_canonicalize(lname));
    else                 sm_emit_s(g_p, SM_STORE_VAR, lname);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_augop(const tree_t *t)
{
    ICN_BB_EVAL(t);
    const tree_t *lhs = T0(t), *rhs = T1(t);
    int op = (int)t->v.ival;
    const char *lname = NULL; int lslot = -1, is_kw = 0;
    if (lhs && lhs->t == TT_VAR && lhs->v.sval) {
        lname = lhs->v.sval;
        if (g_in_proc_body && g_proc_scope && lname[0] && lname[0] != '&')
            lslot = scope_get(g_proc_scope, lname);
    } else if (lhs && lhs->t == TT_KEYWORD && lhs->v.sval) {
        lname = lhs->v.sval; is_kw = 1;
    }
    if (lslot >= 0 || lname) {
        if      (lslot >= 0) sm_emit_i(g_p, SM_LOAD_FRAME, lslot);
        else if (is_kw)      sm_emit_s(g_p, SM_PUSH_VAR, kw_canonicalize(lname));
        else                 sm_emit_s(g_p, SM_PUSH_VAR, lname);
        lower_expr(rhs);
        switch (op) {
        case TK_AUGPLUS:   sm_emit(g_p, SM_ADD);    emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGMINUS:  sm_emit(g_p, SM_SUB);    emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGSTAR:   sm_emit(g_p, SM_MUL);    emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGSLASH:  sm_emit(g_p, SM_DIV);    emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGMOD:    sm_emit(g_p, SM_MOD);    emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGCONCAT: sm_emit(g_p, SM_CONCAT); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGPOW:    sm_emit(g_p, SM_EXP);    emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGEQ: sm_emit_i(g_p, SM_ACOMP, TT_EQ); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGNE: sm_emit_i(g_p, SM_ACOMP, TT_NE); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGLT: sm_emit_i(g_p, SM_ACOMP, TT_LT); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGLE: sm_emit_i(g_p, SM_ACOMP, TT_LE); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGGT: sm_emit_i(g_p, SM_ACOMP, TT_GT); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGGE: sm_emit_i(g_p, SM_ACOMP, TT_GE); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGSEQ:  sm_emit_i(g_p, SM_LCOMP, TT_LEQ); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGSNE:  sm_emit_i(g_p, SM_LCOMP, TT_LNE); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGSLT:  sm_emit_i(g_p, SM_LCOMP, TT_LLT); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGSLE:  sm_emit_i(g_p, SM_LCOMP, TT_LLE); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGSGT:  sm_emit_i(g_p, SM_LCOMP, TT_LGT); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGSGE:  sm_emit_i(g_p, SM_LCOMP, TT_LGE); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGCSET_UNION: sm_emit_si(g_p, SM_CALL_FN, "++", 2); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGCSET_DIFF:  sm_emit_si(g_p, SM_CALL_FN, "--", 2); emit_augop_store(lslot, is_kw, lname); return;
        case TK_AUGCSET_INTER: sm_emit_si(g_p, SM_CALL_FN, "**", 2); emit_augop_store(lslot, is_kw, lname); return;
        default:
            sm_emit_i(g_p, SM_PUSH_LIT_I, (int64_t)op);
            sm_emit_si(g_p, SM_CALL_FN, "AUGOP", 3);
            return;
        }
    }
    lower_expr(lhs); lower_expr(rhs);
    sm_emit_i(g_p, SM_PUSH_LIT_I, (int64_t)op);
    sm_emit_si(g_p, SM_CALL_FN, "AUGOP", 3);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_seq_expr(const tree_t *t)
{
    if (t->n == 0) { sm_emit(g_p, SM_PUSH_NULL); return; }
    for (int i = 0; i < t->n; i++) {
        lower_expr(t->c[i]);
        if (i < t->n - 1) sm_emit(g_p, SM_VOID_POP);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_if(const tree_t *t)
{
    if (t->n < 1) { sm_emit(g_p, SM_PUSH_NULL); return; }
    lower_expr(t->c[0]);
    int jf = sm_emit_i(g_p, SM_JUMP_F, 0);
    sm_emit(g_p, SM_VOID_POP);
    if (t->n > 1) lower_expr(t->c[1]); else sm_emit(g_p, SM_PUSH_NULL);
    int jend = sm_emit_i(g_p, SM_JUMP, 0);
    sm_patch_jump(g_p, jf, sm_label(g_p));
    sm_emit(g_p, SM_VOID_POP);
    if (t->n > 2) lower_expr(t->c[2]); else sm_emit(g_p, SM_PUSH_NULL);
    sm_patch_jump(g_p, jend, sm_label(g_p));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_while_until(const tree_t *t, int exit_on_success)
{
    int top = sm_label(g_p);
    if (t->n < 1) { sm_emit(g_p, SM_PUSH_NULL); return; }
    lower_expr(t->c[0]);
    int jx = exit_on_success ? sm_emit_i(g_p, SM_JUMP_S, 0) : sm_emit_i(g_p, SM_JUMP_F, 0);
    sm_emit(g_p, SM_VOID_POP);
    if (t->n > 1) { lower_expr(t->c[1]); sm_emit(g_p, SM_VOID_POP); }
    sm_emit_i(g_p, SM_JUMP, top);
    sm_patch_jump(g_p, jx, sm_label(g_p));
    sm_emit(g_p, SM_VOID_POP); sm_emit(g_p, SM_PUSH_NULL);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_while(const tree_t *t) { lower_while_until(t, 0); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_until(const tree_t *t) { lower_while_until(t, 1); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_repeat(const tree_t *t)
{
    int top = sm_label(g_p);
    if (t->n > 0) { lower_expr(t->c[0]); sm_emit(g_p, SM_VOID_POP); }
    sm_emit_i(g_p, SM_JUMP, top);
    sm_emit(g_p, SM_PUSH_NULL);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_loop_break(const tree_t *t)
{
    if (t->n > 0) lower_expr(t->c[0]); else sm_emit(g_p, SM_PUSH_NULL);
    sm_emit_i(g_p, SM_JUMP, g_p->count + 1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_loop_next(const tree_t *t) { (void)t; sm_emit(g_p, SM_PUSH_NULL); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_return(const tree_t *t)
{
    if (t->n > 0) lower_expr(t->c[0]); else sm_emit(g_p, SM_PUSH_NULL);
    sm_emit(g_p, SM_RETURN);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_proc_fail(const tree_t *t)
{
    (void)t; sm_emit(g_p, SM_PUSH_NULL); sm_emit(g_p, SM_FRETURN);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_case(const tree_t *t)
{
    if (t->n < 1) { sm_emit(g_p, SM_PUSH_NULL); return; }
    int is_raku = (t->n >= 4 && (t->n - 1) % 3 == 0
                   && t->c[1]
                   && (t->c[1]->t == TT_ILIT || t->c[1]->t == TT_NUL));
    if (!is_raku) {
        int nc = t->n - 1, has_def = nc % 2, npairs = nc / 2;
        lower_expr(t->c[0]);
        sm_emit_s(g_p, SM_STORE_VAR, "__case_topic__"); sm_emit(g_p, SM_VOID_POP);
        int *end_jumps = (int *)GC_MALLOC((size_t)(npairs > 0 ? npairs : 1) * sizeof(int));
        int nend = 0;
        for (int i = 0; i < npairs; i++) {
            sm_emit_s(g_p, SM_PUSH_VAR, "__case_topic__");
            lower_expr(t->c[1 + i*2]);
            sm_emit_si(g_p, SM_CALL_FN, "ICN_CASE_EQ", 2);
            int jf = sm_emit_i(g_p, SM_JUMP_F, 0);
            sm_emit(g_p, SM_VOID_POP);
            lower_expr(t->c[2 + i*2]);
            if (nend < 64) end_jumps[nend++] = sm_emit_i(g_p, SM_JUMP, 0);
            sm_patch_jump(g_p, jf, sm_label(g_p)); sm_emit(g_p, SM_VOID_POP);
        }
        if (has_def) lower_expr(t->c[t->n - 1]); else sm_emit(g_p, SM_PUSH_NULL);
        int end = sm_label(g_p);
        for (int i = 0; i < nend; i++) sm_patch_jump(g_p, end_jumps[i], end);
        return;
    }
    int ntriples = (t->n - 1) / 3, has_def = 0, def_idx = -1;
    if (ntriples > 0) {
        tree_t *last_cmp = t->c[1 + (ntriples-1)*3];
        if (last_cmp && last_cmp->t == TT_NUL) { has_def = 1; def_idx = ntriples - 1; }
    }
    emit_thunk(t->c[0]);
    for (int i = 0; i < ntriples; i++) {
        if (i == def_idx) continue;
        int base = 1 + i*3;
        tree_t *cmp = t->c[base];
        sm_emit_i(g_p, SM_PUSH_LIT_I, (int64_t)((cmp && cmp->t == TT_ILIT) ? cmp->v.ival : TT_EQ));
        emit_thunk(t->c[base+1]); emit_thunk(t->c[base+2]);
    }
    if (has_def) { emit_thunk(t->c[1 + def_idx*3 + 2]); }
    sm_emit_ii(g_p, SM_BB_PUMP_CASE, (int64_t)(ntriples - has_def), (int64_t)has_def);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_makelist(const tree_t *t)
{
    ICN_BB_EVAL(t);
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(g_p, SM_CALL_FN, "MAKELIST", (int64_t)t->n);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_record(const tree_t *t)
{
    ICN_BB_EVAL(t);
    sm_emit_s(g_p, SM_PUSH_LIT_S, t->v.sval ? t->v.sval : "");
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(g_p, SM_CALL_FN, "RECORD_MAKE", (int64_t)t->n + 1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_field(const tree_t *t)
{
    ICN_BB_EVAL(t);
    lower_expr(T0(t));
    sm_emit_s(g_p, SM_PUSH_LIT_S, t->v.sval ? t->v.sval : "");
    sm_emit_si(g_p, SM_CALL_FN, "FIELD_GET", 2);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_global(const tree_t *t) { ICN_BB_EVAL(t); (void)t; sm_emit(g_p, SM_PUSH_NULL); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_initial(const tree_t *t)
{
    char sentinel[64];
    snprintf(sentinel, sizeof sentinel, "__initial_%lx__", (unsigned long)(uintptr_t)t);
    sm_emit_s(g_p, SM_PUSH_VAR, sentinel);
    sm_emit_si(g_p, SM_CALL_FN, "NONNULL", 1);
    int skip = sm_emit_i(g_p, SM_JUMP_S, 0);
    sm_emit(g_p, SM_VOID_POP);
    for (int i = 0; i < t->n; i++) {
        if (!t->c[i]) continue;
        lower_expr(t->c[i]); sm_emit(g_p, SM_VOID_POP);
    }
    sm_emit_i(g_p, SM_PUSH_LIT_I, 1);
    sm_emit_s(g_p, SM_STORE_VAR, sentinel); sm_emit(g_p, SM_VOID_POP);
    int done = sm_emit_i(g_p, SM_JUMP, 0);
    sm_patch_jump(g_p, skip, sm_label(g_p)); sm_emit(g_p, SM_VOID_POP);
    sm_patch_jump(g_p, done, sm_label(g_p)); sm_emit(g_p, SM_PUSH_NULL);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_section_3(const tree_t *t, const char *fn)
{
    if (t->n >= 3) {
        lower_expr(t->c[0]); lower_expr(t->c[1]); lower_expr(t->c[2]);
        sm_emit_si(g_p, SM_CALL_FN, fn, 3);
    } else sm_emit(g_p, SM_PUSH_NULL);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_bang_binary  (const tree_t *t)
{
    if (g_lang != LANG_ICN) { lower_unhandled(t); return; }
    sm_emit_i(g_p, SM_BB_EVAL, (int64_t)every_table_register((tree_t *)t));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_to(const tree_t *t) {
    if (g_lang != LANG_ICN) { lower_unhandled(t); return; }
    sm_emit_i(g_p, SM_BB_EVAL, (int64_t)every_table_register((tree_t *)t));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_to_by(const tree_t *t) {
    if (g_lang != LANG_ICN) { lower_unhandled(t); return; }
    sm_emit_i(g_p, SM_BB_EVAL, (int64_t)every_table_register((tree_t *)t));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const tree_t *find_first_alternate(const tree_t *t)
{
    if (!t) return NULL;
    if (t->t == TT_ALTERNATE) return t;
    if (t->t == TT_ASSIGN && t->n >= 2)
        return find_first_alternate(t->c[1]);
    if (t->t == TT_SEQ) {
        for (int i = 0; i < t->n; i++) {
            const tree_t *found = find_first_alternate(t->c[i]);
            if (found) return found;
        }
    }
    for (int i = 0; i < t->n; i++) {
        if (t->c[i] && t->c[i]->t == TT_ASSIGN) {
            const tree_t *found = find_first_alternate(t->c[i]);
            if (found) return found;
        }
    }
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_limit_every(const tree_t *limit_node, const tree_t *body)
{
    (void)body;
    sm_emit_i(g_p, SM_BB_PUMP_EVERY, (int64_t)every_table_register((tree_t *)limit_node));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_every(const tree_t *t)
{
    if (t->n < 1) { sm_emit(g_p, SM_PUSH_NULL); return; }
    const tree_t *gen_expr = t->c[0];
    const tree_t *body     = (t->n > 1) ? t->c[1] : NULL;
    if (g_lang == LANG_ICN && g_in_value_ctx >= 2) {
        sm_emit_i(g_p, SM_BB_EVAL, (int64_t)every_table_register((tree_t *)t));
        return;
    }
    if (g_lang == LANG_ICN && gen_expr->t == TT_LIMIT) {
        lower_limit_every(gen_expr, body);
        return;
    }
    (void)body;
    sm_emit_i(g_p, SM_BB_PUMP_EVERY, (int64_t)every_table_register((tree_t *)t));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_suspend(const tree_t *t)
{
    if (t->n > 0 && t->c[0]) lower_expr(t->c[0]);
    else sm_emit(g_p, SM_PUSH_NULL);
    int jf = sm_emit_i(g_p, SM_JUMP_F, 0);
    sm_emit(g_p, SM_SUSPEND);
    if (t->n > 1 && t->c[1]) { lower_expr(t->c[1]); sm_emit(g_p, SM_VOID_POP); }
    sm_emit(g_p, SM_PUSH_NULL);
    int jdone = sm_emit_i(g_p, SM_JUMP, 0);
    sm_patch_jump(g_p, jf, sm_label(g_p));
    sm_patch_jump(g_p, jdone, sm_label(g_p));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_limit(const tree_t *t) { emit_push_expr(t); sm_emit(g_p, SM_BB_PUMP); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_iterate(const tree_t *t)
{
    if (!t || t->n < 1 || !t->c[0]) { sm_emit(g_p, SM_PUSH_NULL); return; }
    if (g_lang != LANG_ICN) { lower_unhandled(t); return; }
    sm_emit_i(g_p, SM_BB_EVAL, (int64_t)every_table_register((tree_t *)t));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_prolog_call(const char *sval)
{
    const char *sl = strrchr(sval, '/');
    sm_emit_si(g_p, SM_BB_ONCE_PROC, sval, (int64_t)(sl ? atoi(sl + 1) : 0));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_prolog_call_arity0(const char *sval)
{
    sm_emit_si(g_p, SM_BB_ONCE_PROC, sval, 0);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_choice(const tree_t *t)
{
    if (t->v.sval) emit_prolog_call(t->v.sval);
    else {
        fprintf(stderr, "FATAL: lower_choice unnamed TT_CHOICE reached — legacy SM_BB_ONCE deleted (PB-7)\n");
        abort();
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *attr_expr_of(const tree_t *s, const char *tag)
{
    tree_t *a = stmt_attr_find(s, tag);
    return a ? stmt_attr_expr(a) : NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *attr_str_of(const tree_t *s, const char *tag)
{
    tree_t *a = stmt_attr_find(s, tag);
    return stmt_attr_str(a);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int attr_int_of(const tree_t *s, const char *tag)
{
    const char *sv = attr_str_of(s, tag);
    return sv ? atoi(sv) : 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void lower_stmt(const tree_t *s)
{
    LabelTable *tbl = &g_labtab;
    if (s->t == TT_END) {
        const char *lbl = attr_str_of(s, ":lbl");
        if (lbl && lbl[0]) {
            int lbl_idx = sm_label_named(g_p, lbl);
            labtab_define(tbl, lbl, lbl_idx);
        }
        int stno   = attr_int_of(s, ":stno");
        int lineno = attr_int_of(s, ":line");
        sm_emit_ii(g_p, SM_STNO, (int64_t)stno, (int64_t)lineno);
        sm_emit(g_p, SM_HALT);
        return;
    }
    const char *label   = attr_str_of(s, ":lbl");
    int         lang    = attr_int_of(s, ":lang");
    g_lang = lang;
    int         stno    = attr_int_of(s, ":stno");
    int         lineno  = attr_int_of(s, ":line");
    tree_t      *subject = attr_expr_of(s, ":subj");
    tree_t      *pattern = attr_expr_of(s, ":pat");
    int         has_eq  = (stmt_attr_find(s, ":eq") != NULL);
    tree_t      *replacement = attr_expr_of(s, ":repl");
    tree_t      *go_s_attr = stmt_attr_find(s, ":goS");
    tree_t      *go_f_attr = stmt_attr_find(s, ":goF");
    tree_t      *go_u_attr = stmt_attr_find(s, ":go");
    const char *goto_s      = go_s_attr ? stmt_attr_str(go_s_attr)  : NULL;
    const char *goto_f      = go_f_attr ? stmt_attr_str(go_f_attr)  : NULL;
    const char *goto_u      = go_u_attr ? stmt_attr_str(go_u_attr)  : NULL;
    tree_t      *goto_s_expr = go_s_attr ? stmt_attr_expr(go_s_attr) : NULL;
    tree_t      *goto_f_expr = go_f_attr ? stmt_attr_expr(go_f_attr) : NULL;
    tree_t      *goto_u_expr = go_u_attr ? stmt_attr_expr(go_u_attr) : NULL;
    if ((!label || !label[0])
            && !subject && !pattern && !has_eq
            && !goto_u && !goto_u_expr
            && !goto_s && !goto_s_expr
            && !goto_f && !goto_f_expr)
        return;
    if (label && label[0]) {
        int lbl_idx = sm_label_named(g_p, label);
        labtab_define(tbl, label, lbl_idx);
        if (FUNC_IS_ENTRY_LABEL(label)) {
            g_p->instrs[g_p->count - 1].a[2].i = 1;
            sm_emit(g_p, SM_DEFINE_ENTRY);
            tbl->labels[tbl->nlabels - 1].instr_idx = g_p->count;
        }
    }
    sm_emit_ii(g_p, SM_STNO, (int64_t)stno, (int64_t)lineno);
    if (lang == LANG_ICN) return;
    if (lang == LANG_PL) {
        if (subject && subject->t == TT_CHOICE && subject->v.sval) {
            emit_prolog_call_arity0(subject->v.sval);
            goto emit_gotos;
        }
        if (subject && subject->t == TT_FNC && subject->v.sval
                 && strcmp(subject->v.sval, "initialization") == 0
                 && subject->n == 1 && subject->c[0] && subject->c[0]->v.sval) {
            goto emit_gotos;
        }
        else if (subject && subject->t == TT_FNC && subject->v.sval) {
            emit_push_expr(subject);
            sm_emit_si(g_p, SM_CALL_FN, "PL_BUILTIN", 0);
        }
        else {
            fprintf(stderr, "FATAL: lower_stmt LANG_PL unnamed subject kind=%d sval=%s — legacy SM_BB_ONCE deleted (PB-7)\n",
                    subject ? subject->t : -1, (subject && subject->v.sval) ? subject->v.sval : "(null)");
            abort();
        }
        goto emit_gotos;
    }
    if (pattern) {
        lower_pat_expr(pattern);
        if (subject) lower_expr(subject); else sm_emit(g_p, SM_PUSH_NULL);
        if (has_eq && replacement) lower_expr(replacement);
        else if (has_eq)           sm_emit_si(g_p, SM_PUSH_LIT_S, "", 0);
        else                       sm_emit_i(g_p, SM_PUSH_LIT_I, 0);
        const char *sname = (subject && (subject->t == TT_VAR
                              || subject->t == TT_KEYWORD)) ? subject->v.sval : NULL;
        IR_block_t *pat_dcg = IR_lower_pat(pattern);
        sm_emit_sii(g_p, SM_EXEC_STMT, sname, (int64_t)has_eq, (int64_t)sm_prog_dcg_add(g_p, pat_dcg));
        goto emit_gotos;
    }
    if (subject) {
        if (has_eq) {
            if (replacement) lower_expr(replacement); else sm_emit(g_p, SM_PUSH_NULL);
            emit_lhs_store(subject);
        } else {
            if (subject->t == TT_VAR && subject->v.sval) {
                if (strcasecmp(subject->v.sval, "RETURN")  == 0) { sm_emit(g_p, SM_RETURN);  goto emit_gotos; }
                if (strcasecmp(subject->v.sval, "FRETURN") == 0) { sm_emit(g_p, SM_FRETURN); goto emit_gotos; }
                if (strcasecmp(subject->v.sval, "NRETURN") == 0) { sm_emit(g_p, SM_NRETURN); goto emit_gotos; }
            }
            lower_expr(subject); sm_emit(g_p, SM_VOID_POP);
        }
    }
emit_gotos:
    if (!goto_u && !goto_u_expr && !goto_s && !goto_s_expr
            && !goto_f && !goto_f_expr) return;
    if (goto_u && goto_u[0]) { emit_goto(SM_JUMP,   goto_u); return; }
    if (goto_u_expr) { return; }
    if (goto_s && goto_s[0]) emit_goto(SM_JUMP_S, goto_s);
    if (goto_f && goto_f[0]) emit_goto(SM_JUMP_F, goto_f);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_expr_inner(const tree_t *t);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void lower_expr(const tree_t *t)
{
    if (!t) { sm_emit(g_p, SM_PUSH_NULL); return; }
    g_in_value_ctx++;
    lower_expr_inner(t);
    g_in_value_ctx--;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_expr_inner(const tree_t *t)
{
    switch (t->t) {
    case TT_QLIT:                             lower_strlit(t);        return;
    case TT_CSET: {
        if (g_lang == LANG_ICN) { sm_emit_i(g_p, SM_BB_EVAL, (int64_t)every_table_register((tree_t *)t)); return; }
        const char *raw = t->v.sval ? t->v.sval : "";
        const char *canon = icn_cset_canonical(raw);
        sm_emit_s(g_p, SM_PUSH_LIT_CS, canon);
        return;
    }
    case TT_ILIT:                             lower_ilit(t);          return;
    case TT_FLIT:                             lower_flit(t);          return;
    case TT_NUL:                              lower_nul(t);           return;
    case TT_VAR:                              lower_var(t);           return;
    case TT_KEYWORD:                          lower_keyword(t);       return;
    case TT_INDIRECT:                         lower_indirect(t);      return;
    case TT_DEFER:                            lower_defer(t);         return;
    case TT_INTERROGATE:                      lower_interrogate(t);   return;
    case TT_NAME:                             lower_name(t);          return;
    case TT_MNS:   lower_mns(t);   return;    case TT_PLS: lower_pls(t); return;
    case TT_ADD:   lower_add(t);   return;    case TT_SUB: lower_sub(t); return;
    case TT_MUL:   lower_mul(t);   return;    case TT_DIV: lower_div(t); return;
    case TT_MOD:   lower_mod(t);   return;    case TT_POW: lower_pow(t); return;
    case TT_VLIST:                            lower_vlist(t);         return;
    case TT_CAT: case TT_SEQ:                lower_cat_seq(t);       return;
    case TT_ALT:                              lower_pat_expr(t);      return;
    case TT_OPSYN:                            lower_opsyn(t);         return;
    case TT_ARB:    case TT_ARBNO:  case TT_POS:    case TT_RPOS:
    case TT_ANY:    case TT_NOTANY: case TT_SPAN:   case TT_BREAK:  case TT_BREAKX:
    case TT_LEN:    case TT_TAB:    case TT_RTAB:   case TT_REM:
    case TT_FAIL:   case TT_SUCCEED:case TT_FENCE:  case TT_ABORT:  case TT_BAL:
    case TT_CAPT_COND_ASGN: case TT_CAPT_IMMED_ASGN: case TT_CAPT_CURSOR:
                                               lower_pat_expr(t);      return;
    case TT_FNC:                              lower_fnc(t);           return;
    case TT_IDX:                              lower_idx(t);           return;
    case TT_ASSIGN:                           lower_assign(t);        return;
    case TT_SCAN:                             lower_scan(t);          return;
    case TT_SWAP:                             lower_swap(t);          return;
    case TT_LT: case TT_LE: case TT_GT: case TT_GE: case TT_EQ: case TT_NE:
                                               lower_acomp(t);         return;
    case TT_LLT: case TT_LLE: case TT_LGT: case TT_LGE: case TT_LEQ: case TT_LNE:
                                               lower_lcomp(t);         return;
    case TT_CSET_UNION: ICN_BB_EVAL(t); lower_expr(T0(t)); lower_expr(T1(t)); sm_emit_si(g_p, SM_CALL_FN, "++", 2); return;
    case TT_CSET_DIFF:  ICN_BB_EVAL(t); lower_expr(T0(t)); lower_expr(T1(t)); sm_emit_si(g_p, SM_CALL_FN, "--", 2); return;
    case TT_CSET_INTER: ICN_BB_EVAL(t); lower_expr(T0(t)); lower_expr(T1(t)); sm_emit_si(g_p, SM_CALL_FN, "**", 2); return;
    case TT_CSET_COMPL: ICN_BB_EVAL(t); lower_expr(T0(t));                    sm_emit_si(g_p, SM_CALL_FN, "~", 1); return;
    case TT_LCONCAT:                          lower_lconcat(t);       return;
    case TT_NONNULL:                          lower_nonnull(t);       return;
    case TT_NULL:                             lower_null(t);          return;
    case TT_NOT:                              lower_not(t);           return;
    case TT_SIZE:                             lower_size(t);          return;
    case TT_RANDOM:                           lower_random(t);        return;
    case TT_IDENTICAL:                        lower_identical(t);     return;
    case TT_AUGOP:                            lower_augop(t);         return;
    case TT_SEQ_EXPR:                         lower_seq_expr(t);      return;
    case TT_IF:                               lower_if(t);            return;
    case TT_WHILE:                            lower_while(t);         return;
    case TT_UNTIL:                            lower_until(t);         return;
    case TT_REPEAT:                           lower_repeat(t);        return;
    case TT_LOOP_BREAK:                       lower_loop_break(t);    return;
    case TT_LOOP_NEXT:                        lower_loop_next(t);     return;
    case TT_RETURN:                           lower_return(t);        return;
    case TT_PROC_FAIL:                        lower_proc_fail(t);     return;
    case TT_CASE:                             lower_case(t);          return;
    case TT_MAKELIST:                         lower_makelist(t);      return;
    case TT_RECORD:                           lower_record(t);        return;
    case TT_FIELD:                            lower_field(t);         return;
    case TT_GLOBAL:                           lower_global(t);        return;
    case TT_INITIAL:                          lower_initial(t);       return;
    case TT_SECTION:       { ICN_BB_EVAL(t); lower_section_3(t, "ICN_SECTION_RANGE"); return; }
    case TT_SECTION_PLUS:  { ICN_BB_EVAL(t); lower_section_3(t, "ICN_SECTION_PLUS");  return; }
    case TT_SECTION_MINUS: { ICN_BB_EVAL(t); lower_section_3(t, "ICN_SECTION_MINUS"); return; }
    case TT_BANG_BINARY:                      lower_bang_binary(t);   return;
    case TT_SUSPEND:                          lower_suspend(t);       return;
    case TT_TO:                               lower_to(t);            return;
    case TT_TO_BY:                            lower_to_by(t);         return;
    case TT_LIMIT:                            lower_limit(t);         return;
    case TT_ALTERNATE:
        if (g_lang != LANG_ICN) { lower_unhandled(t); return; }
        sm_emit_i(g_p, SM_BB_EVAL, (int64_t)every_table_register((tree_t *)t)); return;
    case TT_ITERATE:             lower_iterate(t);       return;
    case TT_EVERY:                            lower_every(t);         return;
    case TT_CHOICE:                           lower_choice(t);        return;
    case TT_UNIFY: emit_push_expr(t); sm_emit_si(g_p, SM_CALL_FN, "PL_UNIFY", 0); return;
    case TT_CUT:   emit_push_expr(t); sm_emit_si(g_p, SM_CALL_FN, "PL_CUT", 0);   return;
    case TT_TRAIL_MARK:   sm_emit_si(g_p, SM_CALL_FN, "PL_TRAIL_MARK",   0); return;
    case TT_TRAIL_UNWIND: sm_emit_si(g_p, SM_CALL_FN, "PL_TRAIL_UNWIND", 0); return;
    case TT_CLAUSE: lower_unhandled(t); return;
    default:                                   lower_unhandled(t);     return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void build_proc_scope(IcnScope *sc, const tree_t *proc, int body_start)
{
    int nparams = proc->_id;
    sc->n = 0;
    for (int i = 0; i < nparams && i < FRAME_SLOT_MAX; i++) {
        tree_t *pn = proc->c[1+i];
        if (pn && pn->v.sval) scope_add(sc, pn->v.sval);
    }
    for (int i = body_start; i < proc->n; i++)
        expression_scope_walk(sc, proc->c[i]);
    for (int i = body_start; i < proc->n; i++) {
        tree_t *ch = proc->c[i];
        if (!ch || ch->t != TT_INITIAL) continue;
        for (int ai = 0; ai < ch->n; ai++) {
            tree_t *as = ch->c[ai];
            if (!as || as->t != TT_ASSIGN || as->n < 1) continue;
            tree_t *lhs = as->c[0];
            if (!lhs || lhs->t != TT_VAR || !lhs->v.sval) continue;
            int w = 0;
            for (int r = 0; r < sc->n; r++) {
                if (sc->e[r].name && strcmp(sc->e[r].name, lhs->v.sval) == 0) continue;
                if (w != r) sc->e[w] = sc->e[r]; w++;
            }
            sc->n = w;
            for (int k = 0; k < sc->n; k++) sc->e[k].slot = k;
        }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_proc_stub(const char *name)
{
    int skip = sm_emit_i(g_p, SM_JUMP, 0);
    sm_label_named(g_p, name);
    sm_emit(g_p, SM_RETURN);
    sm_patch_jump(g_p, skip, sm_label(g_p));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void lower_proc_skeletons(void)
{
    for (int pi = 0; pi < proc_count; pi++) {
        const char *nm = proc_table[pi].name;
        if (!nm || !*nm) continue;
        tree_t *proc = proc_table[pi].proc;
        int skip = sm_emit_i(g_p, SM_JUMP, 0);
        sm_label_named(g_p, nm);
        if (proc) {
            int body_start = 1 + proc->_id;
            IcnScope sc; build_proc_scope(&sc, proc, body_start);
            proc_table[pi].lower_sc = sc;
            g_proc_scope = &sc; g_in_proc_body = 1;
            g_lang = LANG_ICN;
            for (int i = body_start; i < proc->n; i++) {
                if (!proc->c[i]) continue;
                lower_expr( proc->c[i]); sm_emit(g_p, SM_VOID_POP);
            }
            g_lang = 0;
            g_in_proc_body = 0; g_proc_scope = NULL;
        }
        sm_emit(g_p, SM_RETURN);
        sm_patch_jump(g_p, skip, sm_label(g_p));
    }
    for (int b = 0; b < PL_PRED_TABLE_SIZE_FWD; b++)
        for (Pl_PredEntry *e = g_pl_pred_table.buckets[b]; e; e = e->next)
            if (e->key && *e->key) emit_proc_stub(e->key);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
SM_Program *lower(const tree_t *prog)
{
    if (!prog || prog->t != TT_PROGRAM) return NULL;
    g_p            = sm_prog_new();
    g_in_proc_body = 0;
    g_proc_scope   = NULL;
    labtab_init(&g_labtab);
    for (int i = 0; i < LOWER_UNHANDLED_WORDS; i++) g_unhandled_kinds[i] = 0;
    lower_proc_skeletons();
    int stno = 0, has_icn = 0;
    for (int ci = 0; ci < prog->n; ci++) {
        const tree_t *s = prog->c[ci];
        if (!s) continue;
        int s_lang = (s->t == TT_STMT) ? attr_int_of(s, ":lang") : 0;
        if (s->t == TT_STMT && s_lang == LANG_ICN) {
            has_icn = 1;
            sm_stno_label_record(g_p, ++stno, NULL);
            continue;
        }
        lower_stmt(s);
        const char *lbl = (s->t == TT_STMT || s->t == TT_END)
                          ? attr_str_of(s, ":lbl") : NULL;
        sm_stno_label_record(g_p, ++stno, (lbl && lbl[0]) ? lbl : NULL);
    }
    if (has_icn) sm_emit_si(g_p, SM_BB_PUMP_PROC, "main", 0);
    if (g_p->count == 0 || g_p->instrs[g_p->count - 1].op != SM_HALT) sm_emit(g_p, SM_HALT);
    labtab_resolve(&g_labtab, g_p);
    labtab_free(&g_labtab);
    int any = 0;
    for (int w = 0; w < LOWER_UNHANDLED_WORDS; w++) if (g_unhandled_kinds[w]) { any = 1; break; }
    if (any) {
        fprintf(stderr, "sm_lower: unhandled AST kinds:");
        for (int k = 0; k < TT_KIND_COUNT; k++) {
            int w = k/64, b = k%64;
            if (w < LOWER_UNHANDLED_WORDS && (g_unhandled_kinds[w] >> b) & 1)
                fprintf(stderr, " %s", tt_e_name[k]);
        }
        fprintf(stderr, "\n");
    }
    return g_p;
}
