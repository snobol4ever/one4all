/*
 * lower.c — AST → SM_Program compiler pass
 *
 * Six frontends produce a shared AST. This file walks that AST and emits
 * a flat SM_Program (stack-machine instruction sequence) consumed by four
 * execution backends: IR-interp, SM-interp, JIT-exec, native-emit.
 *
 * Entry point: SM_Program *lower(const tree_t *prog)  — prog is TT_PROGRAM
 *
 * Three phases inside lower():
 *   1. lower_proc_skeletons — JUMP/label/RETURN stubs for every procedure
 *      and Prolog predicate so forward calls resolve before bodies land.
 *   2. lower_stmt loop — walks TT_PROGRAM children (TT_STMT / TT_END);
 *      lower_stmt() routes each statement to pattern-match, assignment,
 *      or expression paths; lower_expr() dispatches via g_handlers[].
 *   3. SM_HALT + labtab_resolve — patches forward GOTO targets; reports any
 *      AST kinds that hit lower_unhandled() (diagnostic, normally silent).
 *
 * File-scope state: g_p (SM_Program), g_labtab (LabelTable),
 * g_in_proc_body + g_proc_scope (Icon frame-slot context), g_unhandled_kinds (diagnostic).
 * Naming: p = SM_Program*, t = tree_t*, s = TT_STMT / TT_END node.
 * Handler signature: static void lower_foo(const tree_t *t)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#define IR_DEFINE_NAMES   /* enable tt_e_name[] in ast.h */

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

static void emit_pat_nary(const tree_t *t, sm_opcode_t op);
void lower_expr    (const tree_t *t);
void lower_pat_expr(const tree_t *t);
void lower_stmt    (const tree_t *s);

/*── File-scope lowering state ───────────────────────────────────────────────
 * lower() initializes these before the pass; all handlers read/write them.
 * Only one lowering pass runs at a time (lower() is not reentrant).
 *────────────────────────────────────────────────────────────────────────────*/
#define LOWER_UNHANDLED_WORDS 4
static SM_Program  *g_p;
static LabelTable   g_labtab;
static int          g_in_proc_body;
static IcnScope    *g_proc_scope;
static unsigned long long g_unhandled_kinds[LOWER_UNHANDLED_WORDS];
/* GOAL-ICON-BB-COMPLETE rung13: per-compile slot counter for SM_GEN_TICK.
 * Each lower_every call claims a unique slot in FRAME.every_gen[]. Reset in sm_lower(). */
static int          g_every_gen_slot_next = 0;
/* GOAL-ICON-BB-COMPLETE rung13: hoisted alternate context for lower_every */
static const tree_t *g_hoist_alt   = NULL;
static int           g_hoist_entry = -1;
static int           g_hoist_slot  = -1;

/* GOAL-ICON-BB-COMPLETE rung13: Icon-aware suspendable check that includes TT_SEQ.
 * is_suspendable() (coro_runtime.c) intentionally excludes TT_SEQ to avoid breaking
 * the interp_eval TT_EVERY augop path. We need TT_SEQ here in the lowering layer only. */
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

extern int g_lang;   /* set per-statement in lower_stmt; read by lower_cat_seq */

static void emit_push_expr(const tree_t *t)
{
    sm_emit_ptr(g_p, SM_PUSH_EXPR, (void *)ast_gc_clone(t));
}

static void lower_unhandled(const tree_t *t)
{
    if (!g_in_proc_body && t->t >= 0 && t->t < TT_KIND_COUNT) {
        int w = t->t / 64, b = t->t % 64;
        if (w < LOWER_UNHANDLED_WORDS) g_unhandled_kinds[w] |= (1ULL << b);
    }
    sm_emit(g_p, SM_PUSH_NULL);
}

static int emit_goto(sm_opcode_t op, const char *target)
{
    if (!target) return -1;
    /* RETURN/FRETURN/NRETURN as goto-targets: pick the row, then the column
     * (op == JUMP_S → success, JUMP_F → fail, else unconditional). */
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

/* Inline frame-slot load for a named variable, falling back to NV store. */
static void emit_var_load(const char *vn)
{
    if (g_in_proc_body && g_proc_scope && vn[0] && vn[0] != '&') {
        int slot = scope_get(g_proc_scope, vn);
        if (slot >= 0) { sm_emit_i(g_p, SM_LOAD_FRAME, slot); return; }
    }
    sm_emit_s(g_p, SM_PUSH_VAR, vn);
}

static void emit_var_store(const char *vn)
{
    if (g_in_proc_body && g_proc_scope && vn[0] && vn[0] != '&') {
        int slot = scope_get(g_proc_scope, vn);
        if (slot >= 0) { sm_emit_i(g_p, SM_STORE_FRAME, slot); return; }
    }
    sm_emit_s(g_p, SM_STORE_VAR, vn);
}

/* Emit a thunked SM expression (JUMP/body/RETURN/PUSH_EXPRESSION).
 * Used by DEFER, EVAL, and pattern-capture argument lowering. */
static void emit_thunk(const tree_t *body)
{
    int skip = sm_emit_i(g_p, SM_JUMP, 0);
    int entry = sm_label(g_p);
    if (body) lower_expr(body); else sm_emit(g_p, SM_PUSH_NULL);
    sm_emit(g_p, SM_RETURN);
    sm_patch_jump(g_p, skip, sm_label(g_p));
    sm_emit_ii(g_p, SM_PUSH_EXPRESSION, (int64_t)entry, 0);
}

/*── Literals ────────────────────────────────────────────────────────────────*/

static void lower_strlit(const tree_t *t) { sm_emit_s(g_p, SM_PUSH_LIT_S, t->v.sval ? t->v.sval : ""); }
static void lower_ilit  (const tree_t *t) { sm_emit_i(g_p, SM_PUSH_LIT_I, (int64_t)t->v.ival); }
static void lower_flit  (const tree_t *t) { sm_emit_f(g_p, SM_PUSH_LIT_F, t->v.dval); }
static void lower_nul   (const tree_t *t) { (void)t; sm_emit(g_p, SM_PUSH_NULL); }

/*── Variable references ─────────────────────────────────────────────────────*/

static void lower_var(const tree_t *t)     { emit_var_load(t->v.sval ? t->v.sval : ""); }
static void lower_keyword(const tree_t *t) { sm_emit_s(g_p, SM_PUSH_VAR, kw_canonicalize(t->v.sval)); }

static void lower_indirect(const tree_t *t)
{
    /* $.var[idx] fast path: bypass INDIR_GET, emit PUSH_VAR + IDX directly. */
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

static void lower_defer(const tree_t *t)
{
    /* *expr in value context — thunk the child, push a DT_E descriptor. */
    emit_thunk(T0(t));
}

/*── Arithmetic ──────────────────────────────────────────────────────────────*/

static void lower_interrogate(const tree_t *t) { lower_expr(T0(t)); }

static void lower_name(const tree_t *t)
{
    const char *vname = (T0(t) && T0(t)->v.sval) ? T0(t)->v.sval : "";
    sm_emit_s(g_p, SM_PUSH_LIT_S, vname);
    sm_emit_si(g_p, SM_CALL_FN, "NAME_PUSH", 1);
}

static void lower_mns(const tree_t *t) { LOWER1_VAL(SM_NEG); }
static void lower_pls(const tree_t *t) { LOWER1_VAL(SM_COERCE_NUM); }
static void lower_add(const tree_t *t) { LOWER2(SM_ADD); }
static void lower_sub(const tree_t *t) { LOWER2(SM_SUB); }
static void lower_mul(const tree_t *t) { LOWER2(SM_MUL); }
static void lower_div(const tree_t *t) { LOWER2(SM_DIV); }
static void lower_mod(const tree_t *t) { LOWER2(SM_MOD); }
static void lower_pow(const tree_t *t) { LOWER2(SM_EXP); }

/*── Sequences and alternation ───────────────────────────────────────────────*/

static void lower_vlist(const tree_t *t)
{
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

static void lower_cat_seq(const tree_t *t)
{
    /* Icon & conjunction uses TT_SEQ but is goal-directed, not string concat.
     * When lowering an Icon statement, emit JUMP_F between children so that
     * a failing operand short-circuits the whole conjunction.
     * SNOBOL4/Snocone TT_SEQ (pattern concatenation) is unaffected. */
    if (t->t == TT_SEQ && g_lang == LANG_ICN) {
        if (t->n == 0) { sm_emit(g_p, SM_PUSH_NULL); return; }
        if (t->n == 1) { lower_expr(t->c[0]); return; }
        int njumps = t->n - 1;
        int *fail_jumps = (int *)GC_MALLOC((size_t)njumps * sizeof(int));
        for (int i = 0; i < t->n; i++) {
            lower_expr(t->c[i]);
            if (i < t->n - 1) {
                fail_jumps[i] = sm_emit_i(g_p, SM_JUMP_F, 0); /* -> done with FAILDESCR */
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

static void lower_opsyn(const tree_t *t)
{
    const char *raw = t->v.sval ? t->v.sval : "&";
    char op_buf[4];   /* local — lower_expr is re-entrant; static would be clobbered */
    const char *op = raw;
    const char *lp = strchr(raw, '(');
    if (lp && lp[1] && lp[2] == ')') { op_buf[0] = lp[1]; op_buf[1] = '\0'; op = op_buf; }
    else if (strcmp(raw, "BARFN")  == 0) op = "|";
    else if (strcmp(raw, "AROWFN") == 0) op = "^";
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(g_p, SM_CALL_FN, op, (int64_t)t->n);
}

/*── Pattern-context lowering ────────────────────────────────────────────────*/

/* Build a tab-separated argument name list from *fn(var,...) for SM_PAT_CAPTURE_FN.
 * Returns NULL if any arg is not a plain TT_VAR (caller falls back to stack args). */
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

/* Emit args for a *fn(arg,...) pattern-capture call.
 * Literal args go inline; non-literal args are thunked. */
static void emit_pat_fn_args(const tree_t *fnc)
{
    for (int i = 0; i < fnc->n; i++) {
        tree_t *arg = fnc->c[i];
        if (arg && arg->t == TT_QLIT) lower_expr(arg);
        else                              emit_thunk(arg);
    }
}

/* Emit a pattern capture of kind mode (0=conditional .V, 1=immediate $V, 2=cursor @V).
 * Handles variable, *fn(), and *fn(args) targets. */
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

/* Lower all children in pattern context then emit (n-1) copies of op. */
static void emit_pat_nary(const tree_t *t, sm_opcode_t op)
{
    for (int i = 0; i < t->n; i++) lower_pat_expr(t->c[i]);
    for (int i = 1; i < t->n; i++) sm_emit(g_p, op);
}

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
        else                    sm_emit(g_p, SM_PAT_FENCE);
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
        /* *fn() in pattern — run fn at each match position via SM_PAT_USERCALL. */
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
        /* *var — push by name so self-recursive patterns resolve at match time. */
        if (ch && ch->t == TT_VAR && ch->v.sval) { sm_emit_s(g_p, SM_PAT_REFNAME, ch->v.sval); return; }
        lower_expr(ch); sm_emit(g_p, SM_PAT_DEREF);
        return;
    }
    default:
        lower_expr(t); sm_emit(g_p, SM_PAT_DEREF);
        return;
    }
}

/*── Function calls, assignment, scanning ────────────────────────────────────*/

static void lower_fnc(const tree_t *t)
{
    int nargs = t->n;

    /* EVAL(*expr) — thunk the expression, call it via SM_CALL_EXPRESSION. */
    if (nargs == 1 && t->v.sval && strcmp(t->v.sval, "EVAL") == 0
            && t->c[0] && t->c[0]->t == TT_DEFER) {
        emit_thunk(T0(t->c[0]));
        g_p->instrs[g_p->count - 1].op = SM_CALL_EXPRESSION;
        return;
    }
    /* Icon-style call: sval==NULL, children[0] is the callee name node. */
    if (!t->v.sval && nargs >= 1 && t->c[0] && t->c[0]->v.sval) {
        const char *fn = t->c[0]->v.sval;
        for (int i = 1; i < nargs; i++) lower_expr(t->c[i]);
        sm_emit_si(g_p, SM_CALL_FN, fn, (int64_t)(nargs - 1));
        return;
    }
    for (int i = 0; i < nargs; i++) lower_expr(t->c[i]);
    sm_emit_si(g_p, SM_CALL_FN, t->v.sval ? t->v.sval : "", (int64_t)nargs);
}

static void lower_idx(const tree_t *t)
{
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(g_p, SM_CALL_FN, "IDX", (int64_t)t->n);
}

/* Store TOS into lhs (value already on stack). Handles all lhs shapes. */
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
    lower_expr(lhs);
    sm_emit_si(g_p, SM_CALL_FN, "ASGN", 2);
}

static void lower_assign(const tree_t *t)
{
    lower_expr(T1(t));   /* rhs first */
    emit_lhs_store(T0(t));
}

static void lower_scan(const tree_t *t)
{
    if (t->n < 1) { sm_emit(g_p, SM_PUSH_NULL); return; }
    lower_expr(t->c[0]);
    sm_emit_si(g_p, SM_CALL_FN, "ICN_SCAN_PUSH", 1);
    sm_emit(g_p, SM_VOID_POP);
    if (t->n > 1) lower_expr(t->c[1]); else sm_emit(g_p, SM_PUSH_NULL);
    sm_emit_si(g_p, SM_CALL_FN, "ICN_SCAN_POP", 1);
}

static void lower_swap(const tree_t *t)
{
    /* Inline fast path for two plain variables: save→copy→restore.
     * Uses SM_PUSH_VAR / SM_STORE_VAR (not emit_var_load/store) intentionally —
     * __icn_swap_tmp__ is a synthetic global scratch and must never land in a frame slot. */
    if (t->n >= 2 && T0(t) && T1(t)
            && T0(t)->t == TT_VAR && T1(t)->t == TT_VAR) {
        const char *ln = T0(t)->v.sval ? T0(t)->v.sval : "";
        const char *rn = T1(t)->v.sval ? T1(t)->v.sval : "";
        emit_var_load( ln); sm_emit_s(g_p, SM_STORE_VAR, "__icn_swap_tmp__"); sm_emit(g_p, SM_VOID_POP);
        emit_var_load( rn); emit_var_store( ln);
        sm_emit_s(g_p, SM_PUSH_VAR, "__icn_swap_tmp__"); emit_var_store( rn);
        sm_emit(g_p, SM_VOID_POP);
        return;
    }
    lower_expr(T0(t)); lower_expr(T1(t));
    sm_emit_si(g_p, SM_CALL_FN, "SWAP", 2);
}

/*── Relational operators ────────────────────────────────────────────────────*/

static void lower_comp(const tree_t *t, sm_opcode_t op)
{
    lower_expr(T0(t)); lower_expr(T1(t));
    sm_emit_i(g_p, op, (int64_t)t->t);
}
static void lower_acomp(const tree_t *t) { lower_comp(t, SM_ACOMP); }
static void lower_lcomp(const tree_t *t) { lower_comp(t, SM_LCOMP); }

/*── Cset / list ops ─────────────────────────────────────────────────────────*/

static void lower_lconcat(const tree_t *t)
{
    int has_gen = 0;
    for (int i = 0; i < t->n && !has_gen; i++)
        if (is_suspendable(t->c[i])) has_gen = 1;
    if (!has_gen) {
        if (t->n < 1) { sm_emit(g_p, SM_PUSH_NULL); return; }
        for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
        for (int i = 1; i < t->n; i++) sm_emit(g_p, SM_CONCAT);
        return;
    }
    sm_emit_i(g_p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((tree_t *)t));
}

/*── Unary Icon ops ──────────────────────────────────────────────────────────*/

static void lower_nonnull (const tree_t *t) { lower_expr(T0(t)); sm_emit_si(g_p, SM_CALL_FN, "NONNULL",    1); }
static void lower_null    (const tree_t *t) { lower_expr(T0(t)); sm_emit_si(g_p, SM_CALL_FN, "ICN_NULL",  1); }
static void lower_size    (const tree_t *t) { lower_expr(T0(t)); sm_emit_si(g_p, SM_CALL_FN, "SIZE",      1); }
static void lower_identical(const tree_t *t){ lower_expr(T0(t)); lower_expr(T1(t)); sm_emit_si(g_p, SM_CALL_FN, "IDENTICAL", 2); }
static void lower_random  (const tree_t *t)
{
    if (t->n >= 1) { lower_expr(T0(t)); sm_emit_si(g_p, SM_CALL_FN, "ICN_RANDOM", 1); }
    else           { sm_emit(g_p, SM_PUSH_NULL); }
}

static void lower_not(const tree_t *t)
{
    lower_expr(T0(t));
    int js = sm_emit_i(g_p, SM_JUMP_S, 0);
    sm_emit(g_p, SM_VOID_POP); sm_emit(g_p, SM_PUSH_NULL);
    int jend = sm_emit_i(g_p, SM_JUMP, 0);
    int flbl = sm_label(g_p); sm_patch_jump(g_p, js, flbl);
    sm_emit(g_p, SM_VOID_POP); sm_emit_si(g_p, SM_CALL_FN, "FAIL", 0);
    sm_patch_jump(g_p, jend, sm_label(g_p));
}

/* Emit the store half of an augmented-assignment fast path. */
static void emit_augop_store(int lslot, int is_kw, const char *lname)
{
    if      (lslot >= 0) sm_emit_i(g_p, SM_STORE_FRAME, lslot);
    else if (is_kw)      sm_emit_s(g_p, SM_STORE_VAR, kw_canonicalize(lname));
    else                 sm_emit_s(g_p, SM_STORE_VAR, lname);
}

static void lower_augop(const tree_t *t)
{
    const tree_t *lhs = T0(t), *rhs = T1(t);
    int op = (int)t->v.ival;
    /* Fast path: simple variable or keyword lhs — inline load/op/store. */
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

/*── Control flow ────────────────────────────────────────────────────────────*/

static void lower_seq_expr(const tree_t *t)
{
    if (t->n == 0) { sm_emit(g_p, SM_PUSH_NULL); return; }
    for (int i = 0; i < t->n; i++) {
        lower_expr(t->c[i]);
        if (i < t->n - 1) sm_emit(g_p, SM_VOID_POP);
    }
}

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

/* Shared body for while/until — differs only in which jump exits. */
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
static void lower_while(const tree_t *t) { lower_while_until(t, 0); }
static void lower_until(const tree_t *t) { lower_while_until(t, 1); }

static void lower_repeat(const tree_t *t)
{
    int top = sm_label(g_p);
    if (t->n > 0) { lower_expr(t->c[0]); sm_emit(g_p, SM_VOID_POP); }
    sm_emit_i(g_p, SM_JUMP, top);
    sm_emit(g_p, SM_PUSH_NULL);
}

static void lower_loop_break(const tree_t *t)
{
    if (t->n > 0) lower_expr(t->c[0]); else sm_emit(g_p, SM_PUSH_NULL);
    sm_emit_i(g_p, SM_JUMP, g_p->count + 1);  /* sentinel: sm_interp detects self+1 as break */
}

/* TT_LOOP_NEXT is Icon `next` / Snocone `continue`.  The SM interp handles
 * the actual loop-top jump via the loop frame; we just need a value on the
 * stack to satisfy the trailing SM_VOID_POP in the proc-body loop. */
static void lower_loop_next(const tree_t *t) { (void)t; sm_emit(g_p, SM_PUSH_NULL); }

static void lower_return(const tree_t *t)
{
    if (t->n > 0) lower_expr(t->c[0]); else sm_emit(g_p, SM_PUSH_NULL);
    sm_emit(g_p, SM_RETURN);
}

static void lower_proc_fail(const tree_t *t)
{
    (void)t; sm_emit(g_p, SM_PUSH_NULL); sm_emit(g_p, SM_FRETURN);
}

static void lower_case(const tree_t *t)
{
    if (t->n < 1) { sm_emit(g_p, SM_PUSH_NULL); return; }

    /* Raku triple layout: (n-1) divisible by 3, child[1] is ILIT or NUL. */
    int is_raku = (t->n >= 4 && (t->n - 1) % 3 == 0
                   && t->c[1]
                   && (t->c[1]->t == TT_ILIT || t->c[1]->t == TT_NUL));

    if (!is_raku) {
        /* Icon pair layout: topic + (val,body)* + [default] */
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

    /* Raku triple layout: topic + (cmp_kind, val, body)* + [default triple] */
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

/*── Data constructors ───────────────────────────────────────────────────────*/

static void lower_makelist(const tree_t *t)
{
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(g_p, SM_CALL_FN, "MAKELIST", (int64_t)t->n);
}

static void lower_record(const tree_t *t)
{
    sm_emit_s(g_p, SM_PUSH_LIT_S, t->v.sval ? t->v.sval : "");
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(g_p, SM_CALL_FN, "RECORD_MAKE", (int64_t)t->n + 1);
}

static void lower_field(const tree_t *t)
{
    lower_expr(T0(t));
    sm_emit_s(g_p, SM_PUSH_LIT_S, t->v.sval ? t->v.sval : "");
    sm_emit_si(g_p, SM_CALL_FN, "FIELD_GET", 2);
}

static void lower_global(const tree_t *t) { (void)t; sm_emit(g_p, SM_PUSH_NULL); }

static void lower_initial(const tree_t *t)
{
    /* Once-on-first-call guard: NV sentinel per AST node pointer. */
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

/*── String sections ─────────────────────────────────────────────────────────*/

static void lower_section_3(const tree_t *t, const char *fn)
{
    if (t->n >= 3) {
        lower_expr(t->c[0]); lower_expr(t->c[1]); lower_expr(t->c[2]);
        sm_emit_si(g_p, SM_CALL_FN, fn, 3);
    } else sm_emit(g_p, SM_PUSH_NULL);
}
static void lower_bang_binary  (const tree_t *t)
{
    sm_emit_i(g_p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((tree_t *)t));
}

/*── Generator coroutines ────────────────────────────────────────────────────*/

/* Emit an SM coroutine body for integer range lo..hi [by step].
 * glocal slots: 0=lo, 1=hi, 2=cur, (3=step for to_by). */
static void emit_range_coroutine(const tree_t *lo_expr,
                                  const tree_t *hi_expr, const tree_t *step_expr)
{
    int skip = sm_emit_i(g_p, SM_JUMP, 0), entry = sm_label(g_p);
    sm_emit(g_p, SM_RESUME);
    if (lo_expr) lower_expr(lo_expr); else sm_emit_i(g_p, SM_PUSH_LIT_I, 0);
    sm_emit_i(g_p, SM_STORE_GLOCAL, 0); sm_emit(g_p, SM_VOID_POP);
    if (hi_expr) lower_expr(hi_expr); else sm_emit_i(g_p, SM_PUSH_LIT_I, 0);
    sm_emit_i(g_p, SM_STORE_GLOCAL, 1); sm_emit(g_p, SM_VOID_POP);
    if (step_expr) {
        lower_expr(step_expr);
        sm_emit_i(g_p, SM_STORE_GLOCAL, 3); sm_emit(g_p, SM_VOID_POP);
    }
    sm_emit_i(g_p, SM_LOAD_GLOCAL, 0);
    sm_emit_i(g_p, SM_STORE_GLOCAL, 2); sm_emit(g_p, SM_VOID_POP);
    int loop = sm_label(g_p);
    if (!step_expr) {
        /* Simple: exit when cur > hi */
        sm_emit_i(g_p, SM_LOAD_GLOCAL, 2); sm_emit_i(g_p, SM_LOAD_GLOCAL, 1);
        sm_emit(g_p, SM_ICMP_GT);
        int exit = sm_emit_i(g_p, SM_JUMP_S, 0);
        sm_emit_i(g_p, SM_LOAD_GLOCAL, 2); sm_emit(g_p, SM_SUSPEND);
        sm_emit_i(g_p, SM_LOAD_GLOCAL, 2); sm_emit_i(g_p, SM_INCR, 1);
        sm_emit_i(g_p, SM_STORE_GLOCAL, 2); sm_emit(g_p, SM_VOID_POP);
        sm_emit_i(g_p, SM_JUMP, loop);
        sm_patch_jump(g_p, exit, sm_label(g_p));
    } else {
        /* Stepped: branch on sign of step */
        sm_emit_i(g_p, SM_LOAD_GLOCAL, 3); sm_emit_i(g_p, SM_PUSH_LIT_I, 0);
        sm_emit(g_p, SM_ICMP_LT);
        int neg = sm_emit_i(g_p, SM_JUMP_S, 0);
        sm_emit_i(g_p, SM_LOAD_GLOCAL, 2); sm_emit_i(g_p, SM_LOAD_GLOCAL, 1);
        sm_emit(g_p, SM_ICMP_GT);
        int exit_pos = sm_emit_i(g_p, SM_JUMP_S, 0);
        int body = sm_emit_i(g_p, SM_JUMP, 0);
        sm_patch_jump(g_p, neg, sm_label(g_p));
        sm_emit_i(g_p, SM_LOAD_GLOCAL, 2); sm_emit_i(g_p, SM_LOAD_GLOCAL, 1);
        sm_emit(g_p, SM_ICMP_LT);
        int exit_neg = sm_emit_i(g_p, SM_JUMP_S, 0);
        sm_patch_jump(g_p, body, sm_label(g_p));
        sm_emit_i(g_p, SM_LOAD_GLOCAL, 2); sm_emit(g_p, SM_SUSPEND);
        sm_emit_i(g_p, SM_LOAD_GLOCAL, 2); sm_emit_i(g_p, SM_LOAD_GLOCAL, 3);
        sm_emit(g_p, SM_ADD); sm_emit_i(g_p, SM_STORE_GLOCAL, 2); sm_emit(g_p, SM_VOID_POP);
        sm_emit_i(g_p, SM_JUMP, loop);
        int exit_pc = sm_label(g_p);
        sm_patch_jump(g_p, exit_pos, exit_pc); sm_patch_jump(g_p, exit_neg, exit_pc);
    }
    sm_emit(g_p, SM_PUSH_NULL); sm_emit(g_p, SM_RETURN);
    sm_patch_jump(g_p, skip, sm_label(g_p));
    sm_emit_ii(g_p, SM_PUSH_EXPRESSION, (int64_t)entry, 0);
    sm_emit(g_p, SM_BB_PUMP_SM);
}

static void lower_to   (const tree_t *t) { emit_range_coroutine(T0(t), T1(t), NULL); }
static void lower_to_by(const tree_t *t) { emit_range_coroutine(T0(t), T1(t), T2(t)); }

/* GOAL-ICON-BB-COMPLETE rung13: find the first TT_ALTERNATE that is a direct
 * RHS of a TT_ASSIGN (or direct child of the gen expr root). Only these can be
 * safely hoisted — alternates nested inside TT_LIMIT/TT_FNC/etc. must not be hoisted
 * because the outer construct's semantics depend on driving the alternate. */
static const tree_t *find_first_alternate(const tree_t *t)
{
    if (!t) return NULL;
    /* Direct alternate at this level */
    if (t->t == TT_ALTERNATE) return t;
    /* TT_ASSIGN: only look in the RHS (c[1]) */
    if (t->t == TT_ASSIGN && t->n >= 2)
        return find_first_alternate(t->c[1]);
    /* TT_SEQ (Icon conjunction): search children for assign-level alternates */
    if (t->t == TT_SEQ) {
        for (int i = 0; i < t->n; i++) {
            const tree_t *found = find_first_alternate(t->c[i]);
            if (found) return found;
        }
    }
    /* Relops / arith: search only one level deep for direct TT_ASSIGN children */
    for (int i = 0; i < t->n; i++) {
        if (t->c[i] && t->c[i]->t == TT_ASSIGN) {
            const tree_t *found = find_first_alternate(t->c[i]);
            if (found) return found;
        }
    }
    return NULL;
}

/* GOAL-ICON-BB-COMPLETE rung13: emit TT_ALTERNATE as a pure-SM coroutine.
 * Each arm: lower_expr(arm_i) then SM_SUSPEND (yields arm value to outer SM_GEN_TICK).
 * After resume, stack is empty; the arm value is available via gs->yielded (pushed
 * by SM_GEN_TICK onto the outer stack). */
static void lower_alternate_gen(const tree_t *t)
{
    if (!t || t->n == 0) { sm_emit(g_p, SM_PUSH_NULL); return; }
    for (int i = 0; i < t->n; i++) {
        lower_expr(t->c[i]);
        sm_emit(g_p, SM_SUSPEND);
    }
    /* Exhausted — SM_PUSH_NULL + SM_RETURN emitted by caller (lower_every) */
}

/* GOAL-ICON-BB-COMPLETE rung13: pure-SM every loop.
 *
 * Hoists the first TT_ALTERNATE in gen_expr as an inner coroutine.
 * The outer loop:
 *   SM_GEN_TICK → get one arm value → run body → loop
 * where "body" is the full gen_expr with the TT_ALTERNATE replaced by
 * a SM_LOAD_GLOCAL from the hoisted value (which SM_GEN_TICK puts into
 * a per-tick glocal via SM_STORE_GLOCAL before the body runs).
 *
 * Actually: SM_GEN_TICK pushes the yielded value onto the outer stack.
 * The outer expression body then uses that TOS value via normal STORE_FRAME
 * (for the assignment). We intercept the TT_ALTERNATE dispatch below to
 * emit "pop TOS as the pre-yielded value" by checking g_hoist_alt.
 *
 * Layout:
 *   SM_JUMP skip                   ; over alt coroutine body
 *   alt_entry: SM_RESUME
 *   [lower_alternate_gen(alt_t)]   ; push each arm, SM_SUSPEND
 *   SM_PUSH_NULL; SM_RETURN        ; exhausted
 *   skip:
 *   loop_top:
 *   SM_GEN_TICK alt_entry slot     ; one arm value → TOS, last_ok
 *   SM_JUMP_F done                 ; exhausted
 *   [lower_expr(gen_expr) with g_hoist_alt set]  ; uses TOS value directly
 *   SM_VOID_POP                    ; discard gen_expr result (side-effects done)
 *   [if c[1]: lower_expr(body); SM_VOID_POP]
 *   SM_JUMP loop_top
 *   done:
 *   SM_PUSH_NULL                   ; every is void
 */
/* GOAL-ICON-BB-COMPLETE rung14: emit `every (E \ N) [do body]` as pure SM.
 * Two slots: slot_inner drives E (must be TT_ALTERNATE or TT_TO/TT_TO_BY);
 * slot_limit drives the limit-counting wrapper coroutine.
 * GLOCAL[0] inside the limit coroutine holds the remaining count (integer).
 *
 * Coroutine layout:
 *   [inner_alt coroutine: SM_JUMP skip_inner / entry_inner: SM_RESUME / lower_alternate_gen / SM_PUSH_NULL / SM_RETURN / skip_inner:]
 *   [limit coroutine:     SM_JUMP skip_limit / entry_limit: SM_RESUME / lower(limit_n)→STORE_GLOCAL[0] /
 *                          loop: SM_GEN_TICK entry_inner slot_inner / SM_JUMP_F done /
 *                                SM_LOAD_GLOCAL[0] / SM_PUSH_LIT_I 0 / SM_ICMP_GT / SM_JUMP_F done /
 *                                SM_LOAD_GLOCAL[0] / SM_DECR / SM_STORE_GLOCAL[0] / SM_VOID_POP /
 *                                SM_SUSPEND / SM_JUMP loop /
 *                         done: SM_PUSH_NULL / SM_RETURN / skip_limit:]
 *   [every loop:          loop_top: SM_GEN_TICK entry_limit slot_limit / SM_JUMP_F jdone /
 *                                   lower(body) / SM_VOID_POP / SM_JUMP loop_top /
 *                         jdone: SM_PUSH_NULL]
 */
static void lower_limit_every(const tree_t *limit_node, const tree_t *body)
{
    const tree_t *inner_expr = (limit_node->n >= 1) ? limit_node->c[0] : NULL;
    const tree_t *limit_n    = (limit_node->n >= 2) ? limit_node->c[1] : NULL;
    if (!inner_expr || !limit_n) {
        sm_emit_i(g_p, SM_BB_PUMP_EVERY, (int64_t)every_table_register((tree_t *)limit_node));
        return;
    }

    /* Find the alternate inside inner_expr (same rules as lower_every) */
    const tree_t *alt_t = find_first_alternate(inner_expr);
    if (!alt_t) {
        sm_emit_i(g_p, SM_BB_PUMP_EVERY, (int64_t)every_table_register((tree_t *)limit_node));
        return;
    }

    /* Allocate two gen slots: inner and limit wrapper */
    int slot_inner = g_every_gen_slot_next++;
    int slot_limit = g_every_gen_slot_next++;
    if (slot_limit >= EVERY_GEN_SLOT_MAX) {
        g_every_gen_slot_next -= 2;
        sm_emit_i(g_p, SM_BB_PUMP_EVERY, (int64_t)every_table_register((tree_t *)limit_node));
        return;
    }

    /* --- Inner alt coroutine --- */
    int skip_inner = sm_emit_i(g_p, SM_JUMP, 0);
    int entry_inner = sm_label(g_p);
    sm_emit(g_p, SM_RESUME);
    lower_alternate_gen(alt_t);
    sm_emit(g_p, SM_PUSH_NULL); sm_emit(g_p, SM_RETURN);
    sm_patch_jump(g_p, skip_inner, sm_label(g_p));

    /* --- Limit wrapper coroutine --- */
    int skip_limit = sm_emit_i(g_p, SM_JUMP, 0);
    int entry_limit = sm_label(g_p);
    sm_emit(g_p, SM_RESUME);
    /* Init counter from limit expression (runs once on first entry) */
    lower_expr(limit_n);
    sm_emit_i(g_p, SM_STORE_GLOCAL, 0); sm_emit(g_p, SM_VOID_POP);
    int loop_lim = sm_label(g_p);
    /* Drive inner generator one tick.
     * SM_GEN_TICK pushes yielded_val (ok) or FAILDESCR (fail); SM_ICMP_GT pops 2, sets last_ok only. */
    sm_emit_ii(g_p, SM_GEN_TICK, (int64_t)entry_inner, (int64_t)slot_inner);
    int done_lim1 = sm_emit_i(g_p, SM_JUMP_F, 0);   /* inner exhausted: TOS=FAILDESCR */
    /* TOS=yielded_val.  Check counter > 0 (ICMP_GT pops 2, no push). */
    sm_emit_i(g_p, SM_LOAD_GLOCAL, 0);
    sm_emit_i(g_p, SM_PUSH_LIT_I, 0);
    sm_emit(g_p, SM_ICMP_GT);
    int done_lim2 = sm_emit_i(g_p, SM_JUMP_F, 0);   /* counter==0: TOS=yielded_val */
    /* counter-- */
    sm_emit_i(g_p, SM_LOAD_GLOCAL, 0);
    sm_emit_i(g_p, SM_DECR, 1);
    sm_emit_i(g_p, SM_STORE_GLOCAL, 0); sm_emit(g_p, SM_VOID_POP);
    /* TOS = yielded_val; suspend (pops and yields it to outer SM_GEN_TICK) */
    sm_emit(g_p, SM_SUSPEND);
    sm_emit_i(g_p, SM_JUMP, loop_lim);
    /* done_lim1: TOS = FAILDESCR from failed SM_GEN_TICK — pop before exit */
    sm_patch_jump(g_p, done_lim1, sm_label(g_p));
    sm_emit(g_p, SM_VOID_POP);
    int skip_to_exit = sm_emit_i(g_p, SM_JUMP, 0);
    /* done_lim2: TOS = yielded_val from counter==0 check — pop before exit */
    sm_patch_jump(g_p, done_lim2, sm_label(g_p));
    sm_emit(g_p, SM_VOID_POP);
    sm_patch_jump(g_p, skip_to_exit, sm_label(g_p));
    sm_emit(g_p, SM_PUSH_NULL); sm_emit(g_p, SM_RETURN);
    sm_patch_jump(g_p, skip_limit, sm_label(g_p));

    /* --- Outer every loop drives limit coroutine --- */
    int loop_top = sm_label(g_p);
    sm_emit_ii(g_p, SM_GEN_TICK, (int64_t)entry_limit, (int64_t)slot_limit);
    int jdone = sm_emit_i(g_p, SM_JUMP_F, 0);
    /* TOS = yielded value; hoist so lower_expr(TT_LIMIT) is a no-op */
    const tree_t *prev_hoist = g_hoist_alt; int prev_entry = g_hoist_entry, prev_slot = g_hoist_slot;
    g_hoist_alt = alt_t; g_hoist_entry = entry_inner; g_hoist_slot = slot_inner;
    /* gen_expr is the TT_LIMIT node: lower it as if its value is already on stack.
     * We don't descend into it — the limit coroutine already handled it.
     * Just discard the yielded value after body. */
    g_hoist_alt = prev_hoist; g_hoist_entry = prev_entry; g_hoist_slot = prev_slot;
    sm_emit(g_p, SM_VOID_POP);   /* discard gen value (stmt context) */
    if (body) { lower_expr(body); sm_emit(g_p, SM_VOID_POP); }
    sm_emit_i(g_p, SM_JUMP, loop_top);
    sm_patch_jump(g_p, jdone, sm_label(g_p));
    sm_emit(g_p, SM_PUSH_NULL);
}

static void lower_every(const tree_t *t)
{
    if (t->n < 1) { sm_emit(g_p, SM_PUSH_NULL); return; }
    const tree_t *gen_expr = t->c[0];
    const tree_t *body     = (t->n > 1) ? t->c[1] : NULL;

    /* GOAL-ICON-BB-COMPLETE rung14: TT_LIMIT in generator — pure SM limit coroutine */
    if (g_lang == LANG_ICN && gen_expr->t == TT_LIMIT) {
        lower_limit_every(gen_expr, body);
        return;
    }

    /* Only apply pure-SM path for Icon with a TT_ALTERNATE generator */
    const tree_t *alt_t = (g_lang == LANG_ICN) ? find_first_alternate(gen_expr) : NULL;
    if (!alt_t) {        sm_emit_i(g_p, SM_BB_PUMP_EVERY, (int64_t)every_table_register((tree_t *)t));
        return;
    }

    /* Allocate per-call gen slot */
    int slot = g_every_gen_slot_next++;
    if (slot >= EVERY_GEN_SLOT_MAX) {
        sm_emit_i(g_p, SM_BB_PUMP_EVERY, (int64_t)every_table_register((tree_t *)t));
        return;
    }

    /* Emit the inner alternation coroutine */
    int skip = sm_emit_i(g_p, SM_JUMP, 0);
    int entry = sm_label(g_p);
    sm_emit(g_p, SM_RESUME);
    lower_alternate_gen(alt_t);
    sm_emit(g_p, SM_PUSH_NULL); sm_emit(g_p, SM_RETURN);
    sm_patch_jump(g_p, skip, sm_label(g_p));

    /* Emit the every loop.  Set g_hoist_alt so lower_expr(TT_ALTERNATE)
     * treats the hoisted alt as "value already on stack from SM_GEN_TICK". */
    int loop_top = sm_label(g_p);
    sm_emit_ii(g_p, SM_GEN_TICK, (int64_t)entry, (int64_t)slot);
    int jdone = sm_emit_i(g_p, SM_JUMP_F, 0);
    /* TOS = arm value from SM_GEN_TICK.  Set hoist context so lower_expr
     * of the TT_ALTERNATE inside gen_expr is a no-op (value already on stack). */
    const tree_t *prev_hoist = g_hoist_alt;
    int prev_entry = g_hoist_entry, prev_slot = g_hoist_slot;
    g_hoist_alt = alt_t; g_hoist_entry = entry; g_hoist_slot = slot;
    lower_expr(gen_expr);
    g_hoist_alt = prev_hoist; g_hoist_entry = prev_entry; g_hoist_slot = prev_slot;
    sm_emit(g_p, SM_VOID_POP);
    if (body) { lower_expr(body); sm_emit(g_p, SM_VOID_POP); }
    sm_emit_i(g_p, SM_JUMP, loop_top);
    sm_patch_jump(g_p, jdone, sm_label(g_p));
    sm_emit(g_p, SM_PUSH_NULL);
}

static void lower_suspend(const tree_t *t)
{
    if (t->n > 0 && t->c[0]) lower_expr(t->c[0]);
    else sm_emit(g_p, SM_PUSH_NULL);
    int jf = sm_emit_i(g_p, SM_JUMP_F, 0);
    sm_emit(g_p, SM_SUSPEND_VALUE);
    if (t->n > 1 && t->c[1]) { lower_expr(t->c[1]); sm_emit(g_p, SM_VOID_POP); }
    sm_emit(g_p, SM_PUSH_NULL);
    int jdone = sm_emit_i(g_p, SM_JUMP, 0);
    sm_patch_jump(g_p, jf, sm_label(g_p));
    sm_patch_jump(g_p, jdone, sm_label(g_p));
}

static void lower_bb_pump_ast(const tree_t *t)
{
    sm_emit_i(g_p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((tree_t *)t));
}
static void lower_limit(const tree_t *t) { emit_push_expr(t); sm_emit(g_p, SM_BB_PUMP); }

/*── Prolog ──────────────────────────────────────────────────────────────────*/

/* Emit SM_BB_ONCE_PROC for a named predicate, parsing arity from "name/n". */
static void emit_prolog_call(const char *sval)
{
    const char *sl = strrchr(sval, '/');
    sm_emit_si(g_p, SM_BB_ONCE_PROC, sval, (int64_t)(sl ? atoi(sl + 1) : 0));
}

static void lower_choice(const tree_t *t)
{
    if (t->v.sval) emit_prolog_call(t->v.sval);
    else           { emit_push_expr(t); sm_emit(g_p, SM_BB_ONCE); }
}

static void lower_prolog_child(const tree_t *t)
{
    emit_push_expr(t); sm_emit(g_p, SM_BB_ONCE);
}

/*── Statement lowering ──────────────────────────────────────────────────────
 * lower_stmt reads TT_STMT / TT_END nodes produced by a frontend.
 *
 * TT_STMT children are tagged attribute nodes (kind=TT_ATTR, sval=tag).
 * Access via stmt_attr_find(s, ":tag") → attr node; stmt_attr_expr(attr) → expr.
 * Tags: :lbl :lang :line :stno :subj :pat :eq :repl :goS :goF :go
 *────────────────────────────────────────────────────────────────────────────*/

/* Scan s->c for attribute tag; return expr child or NULL. */
static tree_t *attr_expr_of(const tree_t *s, const char *tag)
{
    tree_t *a = stmt_attr_find(s, tag);
    return a ? stmt_attr_expr(a) : NULL;
}

/* Return attribute string value (first child's sval), or NULL. */
static const char *attr_str_of(const tree_t *s, const char *tag)
{
    tree_t *a = stmt_attr_find(s, tag);
    return stmt_attr_str(a);
}

/* Return attribute integer value (parse first child's sval), or 0. */
static int attr_int_of(const tree_t *s, const char *tag)
{
    const char *sv = attr_str_of(s, tag);
    return sv ? atoi(sv) : 0;
}

void lower_stmt(const tree_t *s)
{
    LabelTable *tbl = &g_labtab;

    /* TT_END — the END statement */
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

    /* TT_STMT — read tagged attributes */
    const char *label   = attr_str_of(s, ":lbl");
    int         lang    = attr_int_of(s, ":lang");
    g_lang = lang;   /* propagates to lower_cat_seq: seq in Icon context = conjunction */
    int         stno    = attr_int_of(s, ":stno");
    int         lineno  = attr_int_of(s, ":line");
    tree_t      *subject = attr_expr_of(s, ":subj");
    tree_t      *pattern = attr_expr_of(s, ":pat");
    int         has_eq  = (stmt_attr_find(s, ":eq") != NULL);
    tree_t      *replacement = attr_expr_of(s, ":repl");

    /* Goto arms */
    tree_t      *go_s_attr = stmt_attr_find(s, ":goS");
    tree_t      *go_f_attr = stmt_attr_find(s, ":goF");
    tree_t      *go_u_attr = stmt_attr_find(s, ":go");

    const char *goto_s      = go_s_attr ? stmt_attr_str(go_s_attr)  : NULL;
    const char *goto_f      = go_f_attr ? stmt_attr_str(go_f_attr)  : NULL;
    const char *goto_u      = go_u_attr ? stmt_attr_str(go_u_attr)  : NULL;
    tree_t      *goto_s_expr = go_s_attr ? stmt_attr_expr(go_s_attr) : NULL;
    tree_t      *goto_f_expr = go_f_attr ? stmt_attr_expr(go_f_attr) : NULL;
    tree_t      *goto_u_expr = go_u_attr ? stmt_attr_expr(go_u_attr) : NULL;

    /* Skip blank lines entirely */
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
        }
    }

    sm_emit_ii(g_p, SM_STNO, (int64_t)stno, (int64_t)lineno);
    /* LANG_ICN statements are registered by polyglot_init and lowered as Icon proc
     * skeletons in lower_proc_skeletons(); they must not reach lower_stmt.
     * This guard is a safety net for any future code path that might slip through. */
    if (lang == LANG_ICN) return;

    if (lang == LANG_PL) {
        if (subject && subject->t == TT_CHOICE && subject->v.sval)
            emit_prolog_call(subject->v.sval);
        else {
            if (subject) lower_expr(subject); else sm_emit(g_p, SM_PUSH_NULL);
            sm_emit(g_p, SM_BB_ONCE);
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
        sm_emit_si(g_p, SM_EXEC_STMT, sname, (int64_t)has_eq);
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
    if (goto_u_expr) { sm_emit_s(g_p, SM_PUSH_LIT_S, "(computed-goto)"); sm_emit(g_p, SM_JUMP_INDIR); return; }
    if (goto_s && goto_s[0]) emit_goto(SM_JUMP_S, goto_s);
    if (goto_f && goto_f[0]) emit_goto(SM_JUMP_F, goto_f);
}

/*── Expression dispatcher ────────────────────────────────────────────────────
 * One switch — the compiler sees all cases, warns on missing ones (-Wswitch).
 * Pattern primitives all delegate to lower_pat_expr (they carry no extra state).
 * TT_REVASSIGN / TT_REVSWAP fall to default until implemented.
 *────────────────────────────────────────────────────────────────────────────*/
void lower_expr(const tree_t *t)
{
    if (!t) { sm_emit(g_p, SM_PUSH_NULL); return; }
    switch (t->t) {
    /* literals */
    case TT_QLIT: case TT_CSET:              lower_strlit(t);        return;
    case TT_ILIT:                             lower_ilit(t);          return;
    case TT_FLIT:                             lower_flit(t);          return;
    case TT_NUL:                              lower_nul(t);           return;
    /* references */
    case TT_VAR:                              lower_var(t);           return;
    case TT_KEYWORD:                          lower_keyword(t);       return;
    case TT_INDIRECT:                         lower_indirect(t);      return;
    case TT_DEFER:                            lower_defer(t);         return;
    /* arithmetic */
    case TT_INTERROGATE:                      lower_interrogate(t);   return;
    case TT_NAME:                             lower_name(t);          return;
    case TT_MNS:   lower_mns(t);   return;    case TT_PLS: lower_pls(t); return;
    case TT_ADD:   lower_add(t);   return;    case TT_SUB: lower_sub(t); return;
    case TT_MUL:   lower_mul(t);   return;    case TT_DIV: lower_div(t); return;
    case TT_MOD:   lower_mod(t);   return;    case TT_POW: lower_pow(t); return;
    /* sequences */
    case TT_VLIST:                            lower_vlist(t);         return;
    case TT_CAT: case TT_SEQ:                lower_cat_seq(t);       return;
    case TT_ALT:                              lower_pat_expr(t);      return;
    case TT_OPSYN:                            lower_opsyn(t);         return;
    /* pattern primitives — delegate to lower_pat_expr */
    case TT_ARB:    case TT_ARBNO:  case TT_POS:    case TT_RPOS:
    case TT_ANY:    case TT_NOTANY: case TT_SPAN:   case TT_BREAK:  case TT_BREAKX:
    case TT_LEN:    case TT_TAB:    case TT_RTAB:   case TT_REM:
    case TT_FAIL:   case TT_SUCCEED:case TT_FENCE:  case TT_ABORT:  case TT_BAL:
    case TT_CAPT_COND_ASGN: case TT_CAPT_IMMED_ASGN: case TT_CAPT_CURSOR:
                                               lower_pat_expr(t);      return;
    /* calls */
    case TT_FNC:                              lower_fnc(t);           return;
    case TT_IDX:                              lower_idx(t);           return;
    case TT_ASSIGN:                           lower_assign(t);        return;
    case TT_SCAN:                             lower_scan(t);          return;
    case TT_SWAP:                             lower_swap(t);          return;
    /* relops */
    case TT_LT: case TT_LE: case TT_GT: case TT_GE: case TT_EQ: case TT_NE:
                                               lower_acomp(t);         return;
    case TT_LLT: case TT_LLE: case TT_LGT: case TT_LGE: case TT_LEQ: case TT_LNE:
                                               lower_lcomp(t);         return;
    /* cset / list */
    case TT_CSET_COMPL: case TT_CSET_UNION: case TT_CSET_DIFF: case TT_CSET_INTER:
                                               emit_push_expr(t);      return;
    case TT_LCONCAT:                          lower_lconcat(t);       return;
    /* unary Icon */
    case TT_NONNULL:                          lower_nonnull(t);       return;
    case TT_NULL:                             lower_null(t);          return;
    case TT_NOT:                              lower_not(t);           return;
    case TT_SIZE:                             lower_size(t);          return;
    case TT_RANDOM:                           lower_random(t);        return;
    case TT_IDENTICAL:                        lower_identical(t);     return;
    case TT_AUGOP:                            lower_augop(t);         return;
    /* control */
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
    /* data */
    case TT_MAKELIST:                         lower_makelist(t);      return;
    case TT_RECORD:                           lower_record(t);        return;
    case TT_FIELD:                            lower_field(t);         return;
    case TT_GLOBAL:                           lower_global(t);        return;
    case TT_INITIAL:                          lower_initial(t);       return;
    /* sections */
    case TT_SECTION:                          lower_section_3(t, "ICN_SECTION_RANGE"); return;
    case TT_SECTION_PLUS:                     lower_section_3(t, "ICN_SECTION_PLUS");  return;
    case TT_SECTION_MINUS:                    lower_section_3(t, "ICN_SECTION_MINUS"); return;
    case TT_BANG_BINARY:                      lower_bang_binary(t);   return;
    /* generators */
    case TT_SUSPEND:                          lower_suspend(t);       return;
    case TT_TO:                               lower_to(t);            return;
    case TT_TO_BY:                            lower_to_by(t);         return;
    case TT_LIMIT:                            lower_limit(t);         return;
    /* GOAL-ICON-BB-COMPLETE rung13: if this is the hoisted alternate, value is
     * already on stack from SM_GEN_TICK — emit nothing. Otherwise legacy AST pump. */
    case TT_ALTERNATE:
        if (t == g_hoist_alt) return;   /* hoisted: TOS already has the yielded arm value */
        lower_bb_pump_ast(t); return;
    case TT_ITERATE:             lower_bb_pump_ast(t);   return;
    case TT_EVERY:                            lower_every(t);         return;
    /* Prolog */
    case TT_CHOICE:                           lower_choice(t);        return;
    case TT_CLAUSE: case TT_CUT: case TT_UNIFY:
    case TT_TRAIL_MARK: case TT_TRAIL_UNWIND: lower_prolog_child(t); return;
    /* not yet implemented (TT_REVASSIGN, TT_REVSWAP) */
    default:                                   lower_unhandled(t);     return;
    }
}

/*── Procedure skeletons ─────────────────────────────────────────────────────*/

static void build_proc_scope(IcnScope *sc, const tree_t *proc, int body_start)
{
    int nparams = proc->_id;   /* SI-13 fix: stored in _id, not v.ival (union alias) */
    sc->n = 0;
    for (int i = 0; i < nparams && i < FRAME_SLOT_MAX; i++) {
        tree_t *pn = proc->c[1+i];
        if (pn && pn->v.sval) scope_add(sc, pn->v.sval);
    }
    for (int i = body_start; i < proc->n; i++)
        expression_scope_walk(sc, proc->c[i]);
    /* Remove names assigned in initial{} — they must use NV, not frame slots. */
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

/* Emit a JUMP/label/RETURN/patch stub — used for bodyless Prolog predicate skeletons. */
static void emit_proc_stub(const char *name)
{
    int skip = sm_emit_i(g_p, SM_JUMP, 0);
    sm_label_named(g_p, name);
    sm_emit(g_p, SM_RETURN);
    sm_patch_jump(g_p, skip, sm_label(g_p));
}

static void lower_proc_skeletons(void)
{

    for (int pi = 0; pi < proc_count; pi++) {
        const char *nm = proc_table[pi].name;
        if (!nm || !*nm) continue;
        tree_t *proc = proc_table[pi].proc;
        int skip = sm_emit_i(g_p, SM_JUMP, 0);
        sm_label_named(g_p, nm);
        if (proc) {
            int body_start = 1 + proc->_id;   /* SI-13 fix: nparams in _id */
            IcnScope sc; build_proc_scope(&sc, proc, body_start);
            proc_table[pi].lower_sc = sc;
            g_proc_scope = &sc; g_in_proc_body = 1;
            /* All proc bodies use Icon conjunction-style lowering: TT_SEQ means
             * goal-directed conjunction (JUMP_F short-circuit), not string concat. */
            g_lang = LANG_ICN;
            for (int i = body_start; i < proc->n; i++) {
                if (!proc->c[i]) continue;
                lower_expr( proc->c[i]); sm_emit(g_p, SM_VOID_POP);
            }
            g_lang = 0;          /* restore to LANG_SNO */
            g_in_proc_body = 0; g_proc_scope = NULL;
        }
        sm_emit(g_p, SM_RETURN);
        sm_patch_jump(g_p, skip, sm_label(g_p));
    }

    for (int b = 0; b < PL_PRED_TABLE_SIZE_FWD; b++)
        for (Pl_PredEntry *e = g_pl_pred_table.buckets[b]; e; e = e->next)
            if (e->key && *e->key) emit_proc_stub(e->key);
}

/*── Public entry point ──────────────────────────────────────────────────────
 * lower() takes a TT_PROGRAM node produced by a frontend.
 * Children must be TT_STMT / TT_END nodes.
 *────────────────────────────────────────────────────────────────────────────*/

SM_Program *lower(const tree_t *prog)
{
    if (!prog || prog->t != TT_PROGRAM) return NULL;

    g_p            = sm_prog_new();
    g_in_proc_body = 0;
    g_proc_scope   = NULL;
    g_every_gen_slot_next = 0;   /* GOAL-ICON-BB-COMPLETE rung13: reset slot counter */
    labtab_init(&g_labtab);
    for (int i = 0; i < LOWER_UNHANDLED_WORDS; i++) g_unhandled_kinds[i] = 0;

    lower_proc_skeletons();

    int stno = 0, has_icn = 0;
    for (int ci = 0; ci < prog->n; ci++) {
        const tree_t *s = prog->c[ci];
        if (!s) continue;
        /* Icon defs are registered by polyglot_init; skip SM emission */
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
