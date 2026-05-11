/*
 * lower.c — AST → SM_Program compiler pass
 *
 * Six frontends produce a shared AST. This file walks that AST and emits
 * a flat SM_Program (stack-machine instruction sequence) consumed by four
 * execution backends: IR-interp, SM-interp, JIT-exec, native-emit.
 *
 * Entry point: SM_Program *lower(const tree_t *prog)  — prog is AST_PROGRAM
 *
 * Three phases inside lower():
 *   1. lower_proc_skeletons — JUMP/label/RETURN stubs for every procedure
 *      and Prolog predicate so forward calls resolve before bodies land.
 *   2. lower_stmt loop — walks AST_PROGRAM children (AST_STMT / AST_END);
 *      lower_stmt() routes each statement to pattern-match, assignment,
 *      or expression paths; lower_expr() dispatches via g_handlers[].
 *   3. SM_HALT + labtab_resolve — patches forward GOTO targets; reports any
 *      AST kinds that hit lower_unhandled() (diagnostic, normally silent).
 *
 * File-scope state: g_p (SM_Program), g_labtab (LabelTable),
 * g_in_proc_body + g_proc_scope (Icon frame-slot context), g_unhandled_kinds (diagnostic).
 * Naming: p = SM_Program*, t = tree_t*, s = AST_STMT / AST_END node.
 * Handler signature: static void lower_foo(const tree_t *t)
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

void lower_expr    (const tree_t *t);
void lower_pat_expr(const tree_t *t);
void lower_stmt    (const tree_t *s);  /* SI-3: s is AST_STMT or AST_END */

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

static void emit_push_expr(const tree_t *t)
{
    sm_emit_ptr(g_p, SM_PUSH_EXPR, (void *)ast_gc_clone(t));
}

static void lower_unhandled(const tree_t *t)
{
    if (!g_in_proc_body && t->t >= 0 && t->t < AST_KIND_COUNT) {
        int w = t->t / 64, b = t->t % 64;
        if (w < LOWER_UNHANDLED_WORDS) g_unhandled_kinds[w] |= (1ULL << b);
    }
    sm_emit(g_p, SM_PUSH_NULL);
}

static int emit_goto(sm_opcode_t op, const char *target)
{
    if (!target) return -1;
    if (strcasecmp(target, "RETURN")  == 0) {
        if (op == SM_JUMP_S) return sm_emit(g_p, SM_RETURN_S);
        if (op == SM_JUMP_F) return sm_emit(g_p, SM_RETURN_F);
        return sm_emit(g_p, SM_RETURN);
    }
    if (strcasecmp(target, "FRETURN") == 0) {
        if (op == SM_JUMP_S) return sm_emit(g_p, SM_FRETURN_S);
        if (op == SM_JUMP_F) return sm_emit(g_p, SM_FRETURN_F);
        return sm_emit(g_p, SM_FRETURN);
    }
    if (strcasecmp(target, "NRETURN") == 0) {
        if (op == SM_JUMP_S) return sm_emit(g_p, SM_NRETURN_S);
        if (op == SM_JUMP_F) return sm_emit(g_p, SM_NRETURN_F);
        return sm_emit(g_p, SM_NRETURN);
    }
    int idx = sm_emit_i(g_p, op, 0);
    int resolved = labtab_find(&g_labtab, target);
    if (resolved >= 0) sm_patch_jump(g_p, idx, resolved);
    else               labtab_patch_later(&g_labtab, idx, target);
    return idx;
}

/* Helpers: read a child safely. */
#define T0(t) ((t)->n > 0 ? (t)->c[0] : NULL)
#define T1(t) ((t)->n > 1 ? (t)->c[1] : NULL)
#define T2(t) ((t)->n > 2 ? (t)->c[2] : NULL)

/* One-liner handlers for arithmetic and unary ops. */
#define CALL1(fn) do { lower_expr(T0(t)); sm_emit_si(p,SM_CALL_FN,(fn),1); } while(0)
#define CALL2(fn) do { lower_expr(T0(t)); lower_expr(T1(t)); sm_emit_si(p,SM_CALL_FN,(fn),2); } while(0)

/* Inline frame-slot load for a named variable, falling back to NV store. */
static void emit_var_load(const char *vn)
{
    SM_Program *p = g_p;
    if (g_in_proc_body && g_proc_scope && vn[0] && vn[0] != '&') {
        int slot = scope_get(g_proc_scope, vn);
        if (slot >= 0) { sm_emit_i(p, SM_LOAD_FRAME, slot); return; }
    }
    sm_emit_s(p, SM_PUSH_VAR, vn);
}

/* Inline frame-slot store for a named variable, falling back to NV store. */
static void emit_var_store(const char *vn)
{
    SM_Program *p = g_p;
    if (g_in_proc_body && g_proc_scope && vn[0] && vn[0] != '&') {
        int slot = scope_get(g_proc_scope, vn);
        if (slot >= 0) { sm_emit_i(p, SM_STORE_FRAME, slot); return; }
    }
    sm_emit_s(p, SM_STORE_VAR, vn);
}

/* Emit a thunked SM expression (JUMP/body/RETURN/PUSH_EXPRESSION).
 * Used by DEFER, EVAL, and pattern-capture argument lowering. */
static void emit_thunk(const tree_t *body)
{
    SM_Program *p = g_p;
    int skip = sm_emit_i(p, SM_JUMP, 0);
    int entry = sm_label(p);
    if (body) lower_expr(body); else sm_emit(p, SM_PUSH_NULL);
    sm_emit(p, SM_RETURN);
    sm_patch_jump(p, skip, sm_label(p));
    sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry, 0);
}

/*── Literals ────────────────────────────────────────────────────────────────*/

static void lower_qlit(const tree_t *t) { sm_emit_s(g_p, SM_PUSH_LIT_S, t->v.sval ? t->v.sval : ""); }
static void lower_cset(const tree_t *t) { sm_emit_s(g_p, SM_PUSH_LIT_S, t->v.sval ? t->v.sval : ""); }
static void lower_ilit(const tree_t *t) { sm_emit_i(g_p, SM_PUSH_LIT_I, (int64_t)t->v.ival); }
static void lower_flit(const tree_t *t) { sm_emit_f(g_p, SM_PUSH_LIT_F, t->v.dval); }
static void lower_nul (const tree_t *t) { (void)t; sm_emit(g_p, SM_PUSH_NULL); }

/*── Variable references ─────────────────────────────────────────────────────*/

static void lower_var(const tree_t *t)     { emit_var_load(t->v.sval ? t->v.sval : ""); }
static void lower_keyword(const tree_t *t) { sm_emit_s(g_p, SM_PUSH_VAR, kw_canonicalize(t->v.sval)); }

static void lower_indirect(const tree_t *t)
{
    SM_Program *p = g_p;
    /* $.var[idx] fast path: bypass INDIR_GET, emit PUSH_VAR + IDX directly. */
    const tree_t *ch = T0(t);
    if (ch && ch->t == AST_NAME && ch->n == 1) {
        const tree_t *inner = ch->c[0];
        if (inner && inner->t == AST_IDX && inner->n >= 2
                && inner->c[0] && inner->c[0]->t == AST_VAR
                && inner->c[0]->v.sval) {
            sm_emit_s(p, SM_PUSH_VAR, inner->c[0]->v.sval);
            for (int i = 1; i < inner->n; i++) lower_expr( inner->c[i]);
            sm_emit_si(p, SM_CALL_FN, "IDX", (int64_t)inner->n);
            return;
        }
    }
    lower_expr(ch);
    sm_emit_si(p, SM_CALL_FN, "INDIR_GET", 1);
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
    SM_Program *p = g_p;
    const char *vname = (T0(t) && T0(t)->v.sval) ? T0(t)->v.sval : "";
    sm_emit_s(p, SM_PUSH_LIT_S, vname);
    sm_emit_si(p, SM_CALL_FN, "NAME_PUSH", 1);
}

static void lower_mns(const tree_t *t) { SM_Program *p = g_p; LOWER1_VAL(SM_NEG); }
static void lower_pls(const tree_t *t) { SM_Program *p = g_p; LOWER1_VAL(SM_COERCE_NUM); }
static void lower_add(const tree_t *t) { SM_Program *p = g_p; LOWER2(SM_ADD); }
static void lower_sub(const tree_t *t) { SM_Program *p = g_p; LOWER2(SM_SUB); }
static void lower_mul(const tree_t *t) { SM_Program *p = g_p; LOWER2(SM_MUL); }
static void lower_div(const tree_t *t) { SM_Program *p = g_p; LOWER2(SM_DIV); }
static void lower_mod(const tree_t *t) { SM_Program *p = g_p; LOWER2(SM_MOD); }
static void lower_pow(const tree_t *t) { SM_Program *p = g_p; LOWER2(SM_EXP); }

/*── Sequences and alternation ───────────────────────────────────────────────*/

static void lower_vlist(const tree_t *t)
{
    SM_Program *p = g_p;
    if (t->n == 0) { sm_emit(p, SM_PUSH_NULL); return; }
    if (t->n == 1) { lower_expr(t->c[0]); return; }
    int n = t->n - 1;
    int *jumps = (int *)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < t->n; i++) {
        lower_expr(t->c[i]);
        if (i < t->n - 1) { jumps[i] = sm_emit_i(p, SM_JUMP_S, 0); sm_emit(p, SM_VOID_POP); }
    }
    int done = sm_label(p);
    for (int i = 0; i < n; i++) sm_patch_jump(p, jumps[i], done);
    free(jumps);
}

static void lower_cat_seq(const tree_t *t)
{
    SM_Program *p = g_p;
    /* Icon & conjunction uses AST_SEQ but is goal-directed, not string concat.
     * When lowering an Icon statement, emit JUMP_F between children so that
     * a failing operand short-circuits the whole conjunction.
     * SNOBOL4/Snocone AST_SEQ (pattern concatenation) is unaffected. */
    extern int g_lang;
    if (t->t == AST_SEQ && g_lang == LANG_ICN) {
        if (t->n == 0) { sm_emit(p, SM_PUSH_NULL); return; }
        if (t->n == 1) { lower_expr(t->c[0]); return; }
        int njumps = t->n - 1;
        int *fail_jumps = (int *)GC_MALLOC((size_t)njumps * sizeof(int));
        for (int i = 0; i < t->n; i++) {
            lower_expr(t->c[i]);
            if (i < t->n - 1) {
                fail_jumps[i] = sm_emit_i(p, SM_JUMP_F, 0); /* -> done with FAILDESCR */
                sm_emit(p, SM_VOID_POP);
            }
        }
        int done_lbl = sm_label(p);
        for (int i = 0; i < njumps; i++) sm_patch_jump(p, fail_jumps[i], done_lbl);
        return;
    }
    int has_defer = 0;
    for (int i = 0; i < t->n && !has_defer; i++)
        if (t->c[i] && t->c[i]->t == AST_DEFER) has_defer = 1;
    if (has_defer) {
        for (int i = 0; i < t->n; i++) lower_pat_expr(t->c[i]);
        for (int i = 1; i < t->n; i++) sm_emit(p, SM_PAT_CAT);
    } else {
        for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
        for (int i = 1; i < t->n; i++) sm_emit(p, SM_CONCAT);
    }
}

static void lower_alt  (const tree_t *t) { lower_pat_expr(t); }

static void lower_opsyn(const tree_t *t)
{
    SM_Program *p = g_p;
    const char *raw = t->v.sval ? t->v.sval : "&";
    static char op_buf[4];
    const char *op = raw;
    const char *lp = strchr(raw, '(');
    if (lp && lp[1] && lp[2] == ')') { op_buf[0] = lp[1]; op_buf[1] = '\0'; op = op_buf; }
    else if (strcmp(raw, "BARFN")  == 0) op = "|";
    else if (strcmp(raw, "AROWFN") == 0) op = "^";
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(p, SM_CALL_FN, op, (int64_t)t->n);
}

/*── Pattern-context lowering ────────────────────────────────────────────────*/

/* Build a tab-separated argument name list from *fn(var,...) for SM_PAT_CAPTURE_FN.
 * Returns NULL if any arg is not a plain AST_VAR (caller falls back to stack args). */
const char *sm_pat_capture_fn_arg_names(const tree_t *fnc)
{
    if (!fnc || fnc->n <= 0) return NULL;
    size_t len = 0;
    for (int i = 0; i < fnc->n; i++) {
        const tree_t *a = fnc->c[i];
        if (!a || a->t != AST_VAR || !a->v.sval) return NULL;
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
        if (arg && arg->t == AST_QLIT) lower_expr(arg);
        else                              emit_thunk(arg);
    }
}

/* Emit a pattern capture of kind mode (0=conditional .V, 1=immediate $V, 2=cursor @V).
 * Handles variable, *fn(), and *fn(args) targets. */
static void emit_pat_capture(const tree_t *var_node, int mode)
{
    SM_Program *p = g_p;
    if (var_node && var_node->t == AST_DEFER
            && var_node->n > 0
            && var_node->c[0]
            && var_node->c[0]->t == AST_FNC
            && var_node->c[0]->v.sval) {
        const tree_t *fnc = var_node->c[0];
        const char *names = sm_pat_capture_fn_arg_names(fnc);
        if (names || fnc->n == 0) {
            int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN, fnc->v.sval);
            p->instrs[idx].a[1].i = mode; p->instrs[idx].a[2].s = names;
        } else {
            emit_pat_fn_args(fnc);
            int idx = sm_emit_s(p, SM_PAT_CAPTURE_FN_ARGS, fnc->v.sval);
            p->instrs[idx].a[1].i = mode; p->instrs[idx].a[2].i = fnc->n;
        }
    } else {
        int idx = sm_emit_s(p, SM_PAT_CAPTURE, var_node ? var_node->v.sval : "");
        p->instrs[idx].a[1].i = mode;
    }
}

void lower_pat_expr(const tree_t *t)
{
    SM_Program *p = g_p;
    if (!t) return;
    switch (t->t) {
    case AST_QLIT:  sm_emit_s(p, SM_PAT_LIT, t->v.sval ? t->v.sval : ""); return;
    case AST_VAR:   sm_emit_s(p, SM_PUSH_VAR, t->v.sval); sm_emit(p, SM_PAT_DEREF); return;
    case AST_ARB:   sm_emit(p, SM_PAT_ARB);    return;
    case AST_REM:   sm_emit(p, SM_PAT_REM);    return;
    case AST_FAIL:  sm_emit(p, SM_PAT_FAIL);   return;
    case AST_SUCCEED: sm_emit(p, SM_PAT_SUCCEED); return;
    case AST_ABORT: sm_emit(p, SM_PAT_ABORT);  return;
    case AST_BAL:   sm_emit(p, SM_PAT_BAL);    return;
    case AST_FENCE:
        if (t->n > 0) { lower_pat_expr(t->c[0]); sm_emit(p, SM_PAT_FENCE1); }
        else                    sm_emit(p, SM_PAT_FENCE);
        return;
    case AST_ANY:    lower_expr(T0(t)); sm_emit(p, SM_PAT_ANY);    return;
    case AST_NOTANY: lower_expr(T0(t)); sm_emit(p, SM_PAT_NOTANY); return;
    case AST_SPAN:   lower_expr(T0(t)); sm_emit(p, SM_PAT_SPAN);   return;
    case AST_BREAK:  lower_expr(T0(t)); sm_emit(p, SM_PAT_BREAK);  return;
    case AST_BREAKX: lower_expr(T0(t)); sm_emit(p, SM_PAT_BREAK);  return;
    case AST_LEN:    lower_expr(T0(t)); sm_emit(p, SM_PAT_LEN);    return;
    case AST_POS:    lower_expr(T0(t)); sm_emit(p, SM_PAT_POS);    return;
    case AST_RPOS:   lower_expr(T0(t)); sm_emit(p, SM_PAT_RPOS);   return;
    case AST_TAB:    lower_expr(T0(t)); sm_emit(p, SM_PAT_TAB);    return;
    case AST_RTAB:   lower_expr(T0(t)); sm_emit(p, SM_PAT_RTAB);   return;
    case AST_ARBNO:  { SM_Program *_p = p; LOWER1_PAT(SM_PAT_ARBNO); }
    case AST_SEQ:
    case AST_CAT:
        for (int i = 0; i < t->n; i++) lower_pat_expr(t->c[i]);
        for (int i = 1; i < t->n; i++) sm_emit(p, SM_PAT_CAT);
        return;
    case AST_ALT:
        for (int i = 0; i < t->n; i++) lower_pat_expr(t->c[i]);
        for (int i = 1; i < t->n; i++) sm_emit(p, SM_PAT_ALT);
        return;
    case AST_CAPT_COND_ASGN:
        lower_pat_expr(T0(t));
        if (t->n > 1) emit_pat_capture(t->c[1], 0);
        return;
    case AST_CAPT_IMMED_ASGN:
        lower_pat_expr(T0(t));
        if (t->n > 1) emit_pat_capture(t->c[1], 1);
        return;
    case AST_CAPT_CURSOR:
        if (t->n == 1) {
            sm_emit(p, SM_PAT_EPS);
            emit_pat_capture(t->c[0], 2);
        } else {
            lower_pat_expr(T0(t));
            if (t->n > 1) emit_pat_capture(t->c[1], 2);
        }
        return;
    case AST_DEFER: {
        const tree_t *ch = T0(t);
        /* *fn() in pattern — run fn at each match position via SM_PAT_USERCALL. */
        if (ch && ch->t == AST_FNC && ch->v.sval) {
            if (ch->n == 0) {
                int idx = sm_emit_s(p, SM_PAT_USERCALL, ch->v.sval);
                p->instrs[idx].a[2].s = NULL;
            } else {
                emit_pat_fn_args(ch);
                int idx = sm_emit_s(p, SM_PAT_USERCALL_ARGS, ch->v.sval);
                p->instrs[idx].a[1].i = ch->n;
            }
            return;
        }
        /* *var — push by name so self-recursive patterns resolve at match time. */
        if (ch && ch->t == AST_VAR && ch->v.sval) { sm_emit_s(p, SM_PAT_REFNAME, ch->v.sval); return; }
        lower_expr(ch); sm_emit(p, SM_PAT_DEREF);
        return;
    }
    default:
        lower_expr(t); sm_emit(p, SM_PAT_DEREF);
        return;
    }
}

/*── Function calls, assignment, scanning ────────────────────────────────────*/

static void lower_fnc(const tree_t *t)
{
    SM_Program *p = g_p;
    int nargs = t->n;

    /* EVAL(*expr) — inline the thunked expression, call SM_CALL_EXPRESSION. */
    if (nargs == 1 && t->v.sval && strcmp(t->v.sval, "EVAL") == 0
            && t->c[0] && t->c[0]->t == AST_DEFER) {
        const tree_t *inner = T0(t->c[0]);
        int skip = sm_emit_i(p, SM_JUMP, 0), entry = sm_label(p);
        if (inner) lower_expr(inner); else sm_emit(p, SM_PUSH_NULL);
        sm_emit(p, SM_RETURN);
        sm_patch_jump(p, skip, sm_label(p));
        sm_emit_ii(p, SM_CALL_EXPRESSION, (int64_t)entry, 0);
        return;
    }
    /* Icon-style call: sval==NULL, children[0] is the callee name node. */
    if (!t->v.sval && nargs >= 1 && t->c[0] && t->c[0]->v.sval) {
        const char *fn = t->c[0]->v.sval;
        for (int i = 1; i < nargs; i++) lower_expr(t->c[i]);
        sm_emit_si(p, SM_CALL_FN, fn, (int64_t)(nargs - 1));
        return;
    }
    for (int i = 0; i < nargs; i++) lower_expr(t->c[i]);
    sm_emit_si(p, SM_CALL_FN, t->v.sval ? t->v.sval : "", (int64_t)nargs);
}

static void lower_idx(const tree_t *t)
{
    SM_Program *p = g_p;
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(p, SM_CALL_FN, "IDX", (int64_t)t->n);
}

static void lower_assign(const tree_t *t)
{
    SM_Program *p = g_p;
    lower_expr(T1(t));   /* rhs first */
    const tree_t *lhs = T0(t);
    if (!lhs) return;
    if (lhs->t == AST_VAR)     { emit_var_store( lhs->v.sval ? lhs->v.sval : ""); return; }
    if (lhs->t == AST_KEYWORD) { sm_emit_s(p, SM_STORE_VAR, kw_canonicalize(lhs->v.sval)); return; }
    if (lhs->t == AST_FNC && lhs->v.sval) {
        lower_expr( T0(lhs));
        char set[256]; snprintf(set, sizeof set, "%s_SET", lhs->v.sval);
        sm_emit_si(p, SM_CALL_FN, set, 2); return;
    }
    if (lhs->t == AST_IDX) {
        for (int i = 0; i < lhs->n; i++) lower_expr( lhs->c[i]);
        sm_emit_si(p, SM_CALL_FN, "IDX_SET", (int64_t)(lhs->n + 1)); return;
    }
    if (lhs->t == AST_FIELD) {
        lower_expr( T0(lhs));
        sm_emit_s(p, SM_PUSH_LIT_S, lhs->v.sval ? lhs->v.sval : "");
        sm_emit_si(p, SM_CALL_FN, "FIELD_SET", 3); return;
    }
    lower_expr(lhs);
    sm_emit_si(p, SM_CALL_FN, "ASGN", 2);
}

static void lower_scan(const tree_t *t)
{
    SM_Program *p = g_p;
    if (t->n < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(t->c[0]);
    sm_emit_si(p, SM_CALL_FN, "ICN_SCAN_PUSH", 1);
    sm_emit(p, SM_VOID_POP);
    if (t->n > 1) lower_expr(t->c[1]); else sm_emit(p, SM_PUSH_NULL);
    sm_emit_si(p, SM_CALL_FN, "ICN_SCAN_POP", 1);
}

static void lower_swap(const tree_t *t)
{
    SM_Program *p = g_p;
    /* Inline fast path for two plain variables: save→copy→restore. */
    if (t->n >= 2 && T0(t) && T1(t)
            && T0(t)->t == AST_VAR && T1(t)->t == AST_VAR) {
        const char *ln = T0(t)->v.sval ? T0(t)->v.sval : "";
        const char *rn = T1(t)->v.sval ? T1(t)->v.sval : "";
        emit_var_load( ln); sm_emit_s(p, SM_STORE_VAR, "__icn_swap_tmp__"); sm_emit(p, SM_VOID_POP);
        emit_var_load( rn); emit_var_store( ln);
        sm_emit_s(p, SM_PUSH_VAR, "__icn_swap_tmp__"); emit_var_store( rn);
        sm_emit(p, SM_VOID_POP);
        return;
    }
    lower_expr(T0(t)); lower_expr(T1(t));
    sm_emit_si(p, SM_CALL_FN, "SWAP", 2);
}

/*── Relational operators ────────────────────────────────────────────────────*/

static void lower_acomp(const tree_t *t)
{
    SM_Program *p = g_p;
    lower_expr(T0(t)); lower_expr(T1(t));
    sm_emit_i(p, SM_ACOMP, (int64_t)t->t);
}
static void lower_lcomp(const tree_t *t)
{
    SM_Program *p = g_p;
    lower_expr(T0(t)); lower_expr(T1(t));
    sm_emit_i(p, SM_LCOMP, (int64_t)t->t);
}

/*── Cset / list ops ─────────────────────────────────────────────────────────*/

static void lower_cset_op(const tree_t *t) { emit_push_expr(t); }

static void lower_lconcat(const tree_t *t)
{
    SM_Program *p = g_p;
    int has_gen = 0;
    for (int i = 0; i < t->n && !has_gen; i++)
        if (is_suspendable(t->c[i])) has_gen = 1;
    if (!has_gen) {
        if (t->n < 1) { sm_emit(p, SM_PUSH_NULL); return; }
        for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
        for (int i = 1; i < t->n; i++) sm_emit(p, SM_CONCAT);
        return;
    }
    sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((tree_t *)t));
}

/*── Unary Icon ops ──────────────────────────────────────────────────────────*/

static void lower_nonnull (const tree_t *t) { SM_Program *p=g_p; CALL1("NONNULL"); }
static void lower_null    (const tree_t *t) { SM_Program *p=g_p; CALL1("ICN_NULL"); }
static void lower_size    (const tree_t *t) { SM_Program *p=g_p; CALL1("SIZE"); }
static void lower_identical(const tree_t *t){ SM_Program *p=g_p; CALL2("IDENTICAL"); }
static void lower_random  (const tree_t *t)
{
    SM_Program *p = g_p;
    if (t->n >= 1) { CALL1("ICN_RANDOM"); } else { sm_emit(p, SM_PUSH_NULL); }
}

static void lower_not(const tree_t *t)
{
    SM_Program *p = g_p;
    lower_expr(T0(t));
    int js = sm_emit_i(p, SM_JUMP_S, 0);
    sm_emit(p, SM_VOID_POP); sm_emit(p, SM_PUSH_NULL);
    int jend = sm_emit_i(p, SM_JUMP, 0);
    int flbl = sm_label(p); sm_patch_jump(p, js, flbl);
    sm_emit(p, SM_VOID_POP); sm_emit_si(p, SM_CALL_FN, "FAIL", 0);
    sm_patch_jump(p, jend, sm_label(p));
}

static void lower_augop(const tree_t *t)
{
    SM_Program *p = g_p;
    const tree_t *lhs = T0(t), *rhs = T1(t);
    int op = (int)t->v.ival;
    /* Fast path: simple variable or keyword lhs — inline load/op/store. */
    const char *lname = NULL; int lslot = -1, is_kw = 0;
    if (lhs && lhs->t == AST_VAR && lhs->v.sval) {
        lname = lhs->v.sval;
        if (g_in_proc_body && g_proc_scope && lname[0] && lname[0] != '&')
            lslot = scope_get(g_proc_scope, lname);
    } else if (lhs && lhs->t == AST_KEYWORD && lhs->v.sval) {
        lname = lhs->v.sval; is_kw = 1;
    }
    if (lslot >= 0 || lname) {
        if      (lslot >= 0) sm_emit_i(p, SM_LOAD_FRAME, lslot);
        else if (is_kw)      sm_emit_s(p, SM_PUSH_VAR, kw_canonicalize(lname));
        else                 sm_emit_s(p, SM_PUSH_VAR, lname);
        lower_expr(rhs);
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
    lower_expr(lhs); lower_expr(rhs);
    sm_emit_i(p, SM_PUSH_LIT_I, (int64_t)op);
    sm_emit_si(p, SM_CALL_FN, "AUGOP", 3);
}

/*── Control flow ────────────────────────────────────────────────────────────*/

static void lower_seq_expr(const tree_t *t)
{
    SM_Program *p = g_p;
    if (t->n == 0) { sm_emit(p, SM_PUSH_NULL); return; }
    for (int i = 0; i < t->n; i++) {
        lower_expr(t->c[i]);
        if (i < t->n - 1) sm_emit(p, SM_VOID_POP);
    }
}

static void lower_if(const tree_t *t)
{
    SM_Program *p = g_p;
    if (t->n < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(t->c[0]);
    int jf = sm_emit_i(p, SM_JUMP_F, 0);
    sm_emit(p, SM_VOID_POP);
    if (t->n > 1) lower_expr(t->c[1]); else sm_emit(p, SM_PUSH_NULL);
    int jend = sm_emit_i(p, SM_JUMP, 0);
    sm_patch_jump(p, jf, sm_label(p));
    sm_emit(p, SM_VOID_POP);
    if (t->n > 2) lower_expr(t->c[2]); else sm_emit(p, SM_PUSH_NULL);
    sm_patch_jump(p, jend, sm_label(p));
}

/* Shared body for while/until — differs only in which jump exits. */
static void lower_while_until(const tree_t *t, int exit_on_success)
{
    SM_Program *p = g_p;
    int top = sm_label(p);
    if (t->n < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(t->c[0]);
    int jx = exit_on_success ? sm_emit_i(p, SM_JUMP_S, 0) : sm_emit_i(p, SM_JUMP_F, 0);
    sm_emit(p, SM_VOID_POP);
    if (t->n > 1) { lower_expr(t->c[1]); sm_emit(p, SM_VOID_POP); }
    sm_emit_i(p, SM_JUMP, top);
    sm_patch_jump(p, jx, sm_label(p));
    sm_emit(p, SM_VOID_POP); sm_emit(p, SM_PUSH_NULL);
}
static void lower_while(const tree_t *t) { lower_while_until(t, 0); }
static void lower_until(const tree_t *t) { lower_while_until(t, 1); }

static void lower_repeat(const tree_t *t)
{
    SM_Program *p = g_p;
    int top = sm_label(p);
    if (t->n > 0) { lower_expr(t->c[0]); sm_emit(p, SM_VOID_POP); }
    sm_emit_i(p, SM_JUMP, top);
    sm_emit(p, SM_PUSH_NULL);
}

static void lower_loop_break(const tree_t *t)
{
    SM_Program *p = g_p;
    if (t->n > 0) lower_expr(t->c[0]); else sm_emit(p, SM_PUSH_NULL);
    sm_emit_i(p, SM_JUMP, p->count + 1);  /* sentinel: sm_interp detects self+1 as break */
}

static void lower_loop_next(const tree_t *t) { (void)t; sm_emit(g_p, SM_PUSH_NULL); }

static void lower_return(const tree_t *t)
{
    SM_Program *p = g_p;
    if (t->n > 0) lower_expr(t->c[0]); else sm_emit(p, SM_PUSH_NULL);
    sm_emit(p, SM_RETURN);
}

static void lower_proc_fail(const tree_t *t)
{
    (void)t; sm_emit(g_p, SM_PUSH_NULL); sm_emit(g_p, SM_FRETURN);
}

static void lower_case(const tree_t *t)
{
    SM_Program *p = g_p;
    if (t->n < 1) { sm_emit(p, SM_PUSH_NULL); return; }

    /* Raku triple layout: (n-1) divisible by 3, child[1] is ILIT or NUL. */
    int is_raku = (t->n >= 4 && (t->n - 1) % 3 == 0
                   && t->c[1]
                   && (t->c[1]->t == AST_ILIT || t->c[1]->t == AST_NUL));

    if (!is_raku) {
        /* Icon pair layout: topic + (val,body)* + [default] */
        int nc = t->n - 1, has_def = nc % 2, npairs = nc / 2;
        lower_expr(t->c[0]);
        sm_emit_s(p, SM_STORE_VAR, "__case_topic__"); sm_emit(p, SM_VOID_POP);
        int end_jumps[64], nend = 0;
        for (int i = 0; i < npairs && i < 32; i++) {
            sm_emit_s(p, SM_PUSH_VAR, "__case_topic__");
            lower_expr(t->c[1 + i*2]);
            sm_emit_si(p, SM_CALL_FN, "ICN_CASE_EQ", 2);
            int jf = sm_emit_i(p, SM_JUMP_F, 0);
            sm_emit(p, SM_VOID_POP);
            lower_expr(t->c[2 + i*2]);
            if (nend < 64) end_jumps[nend++] = sm_emit_i(p, SM_JUMP, 0);
            sm_patch_jump(p, jf, sm_label(p)); sm_emit(p, SM_VOID_POP);
        }
        if (has_def) lower_expr(t->c[t->n - 1]); else sm_emit(p, SM_PUSH_NULL);
        int end = sm_label(p);
        for (int i = 0; i < nend; i++) sm_patch_jump(p, end_jumps[i], end);
        return;
    }

    /* Raku triple layout: topic + (cmp_kind, val, body)* + [default triple] */
    #define CHUNK(expr) do { \
        int _s=sm_emit_i(p,SM_JUMP,0), _e=sm_label(p); \
        lower_expr((expr)); sm_emit(p,SM_RETURN); \
        sm_patch_jump(p,_s,sm_label(p)); \
        sm_emit_ii(p,SM_PUSH_EXPRESSION,(int64_t)_e,0); } while(0)

    int ntriples = (t->n - 1) / 3, has_def = 0, def_idx = -1;
    if (ntriples > 0) {
        tree_t *last_cmp = t->c[1 + (ntriples-1)*3];
        if (last_cmp && last_cmp->t == AST_NUL) { has_def = 1; def_idx = ntriples - 1; }
    }
    CHUNK(t->c[0]);
    for (int i = 0; i < ntriples; i++) {
        if (i == def_idx) continue;
        int base = 1 + i*3;
        tree_t *cmp = t->c[base];
        sm_emit_i(p, SM_PUSH_LIT_I, (int64_t)((cmp && cmp->t == AST_ILIT) ? cmp->v.ival : AST_EQ));
        CHUNK(t->c[base+1]); CHUNK(t->c[base+2]);
    }
    if (has_def) { CHUNK(t->c[1 + def_idx*3 + 2]); }
    sm_emit_ii(p, SM_BB_PUMP_CASE, (int64_t)(ntriples - has_def), (int64_t)has_def);
    #undef CHUNK
}

/*── Data constructors ───────────────────────────────────────────────────────*/

static void lower_makelist(const tree_t *t)
{
    SM_Program *p = g_p;
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(p, SM_CALL_FN, "MAKELIST", (int64_t)t->n);
}

static void lower_record(const tree_t *t)
{
    SM_Program *p = g_p;
    sm_emit_s(p, SM_PUSH_LIT_S, t->v.sval ? t->v.sval : "");
    for (int i = 0; i < t->n; i++) lower_expr(t->c[i]);
    sm_emit_si(p, SM_CALL_FN, "RECORD_MAKE", (int64_t)t->n + 1);
}

static void lower_field(const tree_t *t)
{
    SM_Program *p = g_p;
    lower_expr(T0(t));
    sm_emit_s(p, SM_PUSH_LIT_S, t->v.sval ? t->v.sval : "");
    sm_emit_si(p, SM_CALL_FN, "FIELD_GET", 2);
}

static void lower_global(const tree_t *t) { (void)t; sm_emit(g_p, SM_PUSH_NULL); }

static void lower_initial(const tree_t *t)
{
    /* Once-on-first-call guard: NV sentinel per AST node pointer. */
    SM_Program *p = g_p;
    char sentinel[64];
    snprintf(sentinel, sizeof sentinel, "__initial_%lx__", (unsigned long)(uintptr_t)t);
    sm_emit_s(p, SM_PUSH_VAR, sentinel);
    sm_emit_si(p, SM_CALL_FN, "NONNULL", 1);
    int skip = sm_emit_i(p, SM_JUMP_S, 0);
    sm_emit(p, SM_VOID_POP);
    for (int i = 0; i < t->n; i++) {
        if (!t->c[i]) continue;
        lower_expr(t->c[i]); sm_emit(p, SM_VOID_POP);
    }
    sm_emit_i(p, SM_PUSH_LIT_I, 1);
    sm_emit_s(p, SM_STORE_VAR, sentinel); sm_emit(p, SM_VOID_POP);
    int done = sm_emit_i(p, SM_JUMP, 0);
    sm_patch_jump(p, skip, sm_label(p)); sm_emit(p, SM_VOID_POP);
    sm_patch_jump(p, done, sm_label(p)); sm_emit(p, SM_PUSH_NULL);
}

/*── String sections ─────────────────────────────────────────────────────────*/

static void lower_section_3(const tree_t *t, const char *fn)
{
    SM_Program *p = g_p;
    if (t->n >= 3) {
        lower_expr(t->c[0]); lower_expr(t->c[1]); lower_expr(t->c[2]);
        sm_emit_si(p, SM_CALL_FN, fn, 3);
    } else sm_emit(p, SM_PUSH_NULL);
}
static void lower_section      (const tree_t *t) { lower_section_3(t, "ICN_SECTION_RANGE"); }
static void lower_section_plus (const tree_t *t) { lower_section_3(t, "ICN_SECTION_PLUS");  }
static void lower_section_minus(const tree_t *t) { lower_section_3(t, "ICN_SECTION_MINUS"); }
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
    SM_Program *p = g_p;
    int skip = sm_emit_i(p, SM_JUMP, 0), entry = sm_label(p);
    sm_emit(p, SM_RESUME);
    if (lo_expr) lower_expr(lo_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 0); sm_emit(p, SM_VOID_POP);
    if (hi_expr) lower_expr(hi_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 1); sm_emit(p, SM_VOID_POP);
    if (step_expr) {
        lower_expr(step_expr);
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

static void lower_to   (const tree_t *t) { emit_range_coroutine(T0(t), T1(t), NULL); }
static void lower_to_by(const tree_t *t) { emit_range_coroutine(T0(t), T1(t), T2(t)); }

static void lower_every(const tree_t *t)
{
    sm_emit_i(g_p, SM_BB_PUMP_EVERY, (int64_t)every_table_register((tree_t *)t));
}

static void lower_suspend(const tree_t *t)
{
    SM_Program *p = g_p;
    if (t->n > 0 && t->c[0]) lower_expr(t->c[0]);
    else sm_emit(p, SM_PUSH_NULL);
    int jf = sm_emit_i(p, SM_JUMP_F, 0);
    sm_emit(p, SM_SUSPEND_VALUE);
    if (t->n > 1 && t->c[1]) { lower_expr(t->c[1]); sm_emit(p, SM_VOID_POP); }
    sm_emit(p, SM_PUSH_NULL);
    int jdone = sm_emit_i(p, SM_JUMP, 0);
    sm_patch_jump(p, jf, sm_label(p));
    sm_patch_jump(p, jdone, sm_label(p));
}

static void lower_bb_pump_ast(const tree_t *t)
{
    sm_emit_i(g_p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((tree_t *)t));
}
static void lower_limit(const tree_t *t) { emit_push_expr(t); sm_emit(g_p, SM_BB_PUMP); }

/*── Prolog ──────────────────────────────────────────────────────────────────*/

static void lower_choice(const tree_t *t)
{
    SM_Program *p = g_p;
    if (t->v.sval) {
        const char *sl = strrchr(t->v.sval, '/');
        int arity = sl ? atoi(sl + 1) : 0;
        sm_emit_si(p, SM_BB_ONCE_PROC, t->v.sval, (int64_t)arity);
    } else {
        emit_push_expr(t); sm_emit(p, SM_BB_ONCE);
    }
}

static void lower_prolog_child(const tree_t *t)
{
    emit_push_expr(t); sm_emit(g_p, SM_BB_ONCE);
}

/*── Statement lowering ──────────────────────────────────────────────────────
 * SI-3 (pure tree): lower_stmt reads AST_STMT / AST_END.
 *
 * Pure tree shape: t/v/n/c (kind/sval-or-ival-or-dval/nchildren/children[]).
 * AST_STMT children are tagged attribute nodes (kind=AST_ATTR, sval=tag).
 * Access via stmt_attr_find(s, ":tag") → attr node; stmt_attr_expr(attr) → expr.
 *
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
    SM_Program *p = g_p;
    LabelTable *tbl = &g_labtab;

    /* AST_END — the END statement */
    if (s->t == AST_END) {
        const char *lbl = attr_str_of(s, ":lbl");
        if (lbl && lbl[0]) {
            int lbl_idx = sm_label_named(p, lbl);
            labtab_define(tbl, lbl, lbl_idx);
        }
        int stno   = attr_int_of(s, ":stno");
        int lineno = attr_int_of(s, ":line");
        sm_emit_ii(p, SM_STNO, (int64_t)stno, (int64_t)lineno);
        sm_emit(p, SM_HALT);
        return;
    }

    /* AST_STMT — read tagged attributes */
    const char *label   = attr_str_of(s, ":lbl");
    int         lang    = attr_int_of(s, ":lang");   /* 0 = LANG_SNO if absent */
    { extern int g_lang; g_lang = lang; }             /* propagate to lower_cat_seq etc. */
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
        int lbl_idx = sm_label_named(p, label);
        labtab_define(tbl, label, lbl_idx);
        if (FUNC_IS_ENTRY_LABEL(label)) {
            p->instrs[p->count - 1].a[2].i = 1;
            sm_emit(p, SM_DEFINE_ENTRY);
        }
    }

    sm_emit_ii(p, SM_STNO, (int64_t)stno, (int64_t)lineno);
    if (lang == LANG_ICN) return;

    if (lang == LANG_PL) {
        if (subject && subject->t == AST_CHOICE && subject->v.sval) {
            const char *sl = strrchr(subject->v.sval, '/');
            sm_emit_si(p, SM_BB_ONCE_PROC, subject->v.sval, (int64_t)(sl ? atoi(sl+1) : 0));
        } else {
            if (subject) lower_expr(subject); else sm_emit(p, SM_PUSH_NULL);
            sm_emit(p, SM_BB_ONCE);
        }
        goto emit_gotos;
    }

    if (pattern) {
        lower_pat_expr(pattern);
        if (subject) lower_expr(subject); else sm_emit(p, SM_PUSH_NULL);
        if (has_eq && replacement) lower_expr(replacement);
        else if (has_eq)           sm_emit_si(p, SM_PUSH_LIT_S, "", 0);
        else                       sm_emit_i(p, SM_PUSH_LIT_I, 0);
        const char *sname = (subject && (subject->t == AST_VAR
                              || subject->t == AST_KEYWORD)) ? subject->v.sval : NULL;
        sm_emit_si(p, SM_EXEC_STMT, sname, (int64_t)has_eq);
        goto emit_gotos;
    }

    if (subject) {
        if (has_eq) {
            if (replacement) lower_expr(replacement); else sm_emit(p, SM_PUSH_NULL);
            const tree_t *lhs = subject;
            if (lhs->t == AST_VAR || lhs->t == AST_KEYWORD) {
                sm_emit_s(p, SM_STORE_VAR, lhs->v.sval ? lhs->v.sval : "");
            } else if (lhs->t == AST_INDIRECT) {
                lower_expr(T0(lhs)); sm_emit_si(p, SM_CALL_FN, "ASGN_INDIR", 2);
            } else if (lhs->t == AST_IDX) {
                for (int i = 0; i < lhs->n; i++) lower_expr(lhs->c[i]);
                sm_emit_si(p, SM_CALL_FN, "IDX_SET", (int64_t)(lhs->n + 1));
            } else if (lhs->t == AST_FNC && lhs->v.sval) {
                if (lhs->n == 0) {
                    sm_emit_si(p, SM_CALL_FN, "NRETURN_ASGN", 1);
                    p->instrs[p->count - 1].a[1].s = GC_strdup(lhs->v.sval);
                } else if (strcasecmp(lhs->v.sval, "ITEM") == 0) {
                    for (int i = 0; i < lhs->n; i++) lower_expr(lhs->c[i]);
                    sm_emit_si(p, SM_CALL_FN, "ITEM_SET", (int64_t)(lhs->n + 1));
                } else {
                    lower_expr(T0(lhs));
                    char set[256]; snprintf(set, sizeof set, "%s_SET", lhs->v.sval);
                    sm_emit_si(p, SM_CALL_FN, set, 2);
                }
            } else {
                lower_expr(lhs); sm_emit_si(p, SM_CALL_FN, "ASGN", 2);
            }
        } else {
            if (subject->t == AST_VAR && subject->v.sval) {
                if (strcasecmp(subject->v.sval, "RETURN")  == 0) { sm_emit(p, SM_RETURN);  goto emit_gotos; }
                if (strcasecmp(subject->v.sval, "FRETURN") == 0) { sm_emit(p, SM_FRETURN); goto emit_gotos; }
                if (strcasecmp(subject->v.sval, "NRETURN") == 0) { sm_emit(p, SM_NRETURN); goto emit_gotos; }
            }
            lower_expr(subject); sm_emit(p, SM_VOID_POP);
        }
    }

emit_gotos:
    if (!goto_u && !goto_u_expr && !goto_s && !goto_s_expr
            && !goto_f && !goto_f_expr) return;
    if (goto_u && goto_u[0]) { emit_goto(SM_JUMP,   goto_u); return; }
    if (goto_u_expr) { sm_emit_s(p, SM_PUSH_LIT_S, "(computed-goto)"); sm_emit(p, SM_JUMP_INDIR); return; }
    if (goto_s && goto_s[0]) emit_goto(SM_JUMP_S, goto_s);
    if (goto_f && goto_f[0]) emit_goto(SM_JUMP_F, goto_f);
}

/*── Expression dispatcher ────────────────────────────────────────────────────
 * One switch — the compiler sees all cases, warns on missing ones (-Wswitch).
 * Pattern primitives all delegate to lower_pat_expr (they carry no extra state).
 * AST_REVASSIGN / AST_REVSWAP fall to default until implemented.
 *────────────────────────────────────────────────────────────────────────────*/
void lower_expr(const tree_t *t)
{
    if (!t) { sm_emit(g_p, SM_PUSH_NULL); return; }
    switch (t->t) {
    /* literals */
    case AST_QLIT: case AST_CSET:              lower_qlit(t);          return;
    case AST_ILIT:                             lower_ilit(t);          return;
    case AST_FLIT:                             lower_flit(t);          return;
    case AST_NUL:                              lower_nul(t);           return;
    /* references */
    case AST_VAR:                              lower_var(t);           return;
    case AST_KEYWORD:                          lower_keyword(t);       return;
    case AST_INDIRECT:                         lower_indirect(t);      return;
    case AST_DEFER:                            lower_defer(t);         return;
    /* arithmetic */
    case AST_INTERROGATE:                      lower_interrogate(t);   return;
    case AST_NAME:                             lower_name(t);          return;
    case AST_MNS:   lower_mns(t);   return;    case AST_PLS: lower_pls(t); return;
    case AST_ADD:   lower_add(t);   return;    case AST_SUB: lower_sub(t); return;
    case AST_MUL:   lower_mul(t);   return;    case AST_DIV: lower_div(t); return;
    case AST_MOD:   lower_mod(t);   return;    case AST_POW: lower_pow(t); return;
    /* sequences */
    case AST_VLIST:                            lower_vlist(t);         return;
    case AST_CAT: case AST_SEQ:                lower_cat_seq(t);       return;
    case AST_ALT:                              lower_alt(t);           return;
    case AST_OPSYN:                            lower_opsyn(t);         return;
    /* pattern primitives — delegate to lower_pat_expr */
    case AST_ARB:    case AST_ARBNO:  case AST_POS:    case AST_RPOS:
    case AST_ANY:    case AST_NOTANY: case AST_SPAN:   case AST_BREAK:  case AST_BREAKX:
    case AST_LEN:    case AST_TAB:    case AST_RTAB:   case AST_REM:
    case AST_FAIL:   case AST_SUCCEED:case AST_FENCE:  case AST_ABORT:  case AST_BAL:
    case AST_CAPT_COND_ASGN: case AST_CAPT_IMMED_ASGN: case AST_CAPT_CURSOR:
                                               lower_pat_expr(t);      return;
    /* calls */
    case AST_FNC:                              lower_fnc(t);           return;
    case AST_IDX:                              lower_idx(t);           return;
    case AST_ASSIGN:                           lower_assign(t);        return;
    case AST_SCAN:                             lower_scan(t);          return;
    case AST_SWAP:                             lower_swap(t);          return;
    /* relops */
    case AST_LT: case AST_LE: case AST_GT: case AST_GE: case AST_EQ: case AST_NE:
                                               lower_acomp(t);         return;
    case AST_LLT: case AST_LLE: case AST_LGT: case AST_LGE: case AST_LEQ: case AST_LNE:
                                               lower_lcomp(t);         return;
    /* cset / list */
    case AST_CSET_COMPL: case AST_CSET_UNION: case AST_CSET_DIFF: case AST_CSET_INTER:
                                               lower_cset_op(t);       return;
    case AST_LCONCAT:                          lower_lconcat(t);       return;
    /* unary Icon */
    case AST_NONNULL:                          lower_nonnull(t);       return;
    case AST_NULL:                             lower_null(t);          return;
    case AST_NOT:                              lower_not(t);           return;
    case AST_SIZE:                             lower_size(t);          return;
    case AST_RANDOM:                           lower_random(t);        return;
    case AST_IDENTICAL:                        lower_identical(t);     return;
    case AST_AUGOP:                            lower_augop(t);         return;
    /* control */
    case AST_SEQ_EXPR:                         lower_seq_expr(t);      return;
    case AST_IF:                               lower_if(t);            return;
    case AST_WHILE:                            lower_while(t);         return;
    case AST_UNTIL:                            lower_until(t);         return;
    case AST_REPEAT:                           lower_repeat(t);        return;
    case AST_LOOP_BREAK:                       lower_loop_break(t);    return;
    case AST_LOOP_NEXT:                        lower_loop_next(t);     return;
    case AST_RETURN:                           lower_return(t);        return;
    case AST_PROC_FAIL:                        lower_proc_fail(t);     return;
    case AST_CASE:                             lower_case(t);          return;
    /* data */
    case AST_MAKELIST:                         lower_makelist(t);      return;
    case AST_RECORD:                           lower_record(t);        return;
    case AST_FIELD:                            lower_field(t);         return;
    case AST_GLOBAL:                           lower_global(t);        return;
    case AST_INITIAL:                          lower_initial(t);       return;
    /* sections */
    case AST_SECTION:                          lower_section(t);       return;
    case AST_SECTION_PLUS:                     lower_section_plus(t);  return;
    case AST_SECTION_MINUS:                    lower_section_minus(t); return;
    case AST_BANG_BINARY:                      lower_bang_binary(t);   return;
    /* generators */
    case AST_SUSPEND:                          lower_suspend(t);       return;
    case AST_TO:                               lower_to(t);            return;
    case AST_TO_BY:                            lower_to_by(t);         return;
    case AST_LIMIT:                            lower_limit(t);         return;
    case AST_ALTERNATE: case AST_ITERATE:      lower_bb_pump_ast(t);   return;
    case AST_EVERY:                            lower_every(t);         return;
    /* Prolog */
    case AST_CHOICE:                           lower_choice(t);        return;
    case AST_CLAUSE: case AST_CUT: case AST_UNIFY:
    case AST_TRAIL_MARK: case AST_TRAIL_UNWIND: lower_prolog_child(t); return;
    /* not yet implemented (AST_REVASSIGN, AST_REVSWAP) */
    default:                                   lower_unhandled(t);     return;
    }
}

/*── Procedure skeletons ─────────────────────────────────────────────────────*/

static void build_proc_scope(IcnScope *sc, const tree_t *proc, int body_start)
{
    int nparams = (int)proc->v.ival;
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
        if (!ch || ch->t != AST_INITIAL) continue;
        for (int ai = 0; ai < ch->n; ai++) {
            tree_t *as = ch->c[ai];
            if (!as || as->t != AST_ASSIGN || as->n < 1) continue;
            tree_t *lhs = as->c[0];
            if (!lhs || lhs->t != AST_VAR || !lhs->v.sval) continue;
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

static void lower_proc_skeletons(void)
{
    SM_Program *p = g_p;

    for (int pi = 0; pi < proc_count; pi++) {
        const char *nm = proc_table[pi].name;
        if (!nm || !*nm) continue;
        tree_t *proc = proc_table[pi].proc;
        int skip = sm_emit_i(p, SM_JUMP, 0);
        sm_label_named(p, nm);
        if (proc) {
            int body_start = 1 + (int)proc->v.ival;
            IcnScope sc; build_proc_scope(&sc, proc, body_start);
            proc_table[pi].lower_sc = sc;
            g_proc_scope = &sc; g_in_proc_body = 1;
            { extern int g_lang; g_lang = LANG_ICN; }  /* Icon proc body — enable conjunction lowering */
            for (int i = body_start; i < proc->n; i++) {
                if (!proc->c[i]) continue;
                lower_expr( proc->c[i]); sm_emit(p, SM_VOID_POP);
            }
            { extern int g_lang; g_lang = 0; }          /* restore to LANG_SNO */
            g_in_proc_body = 0; g_proc_scope = NULL;
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

/*── Public entry point ──────────────────────────────────────────────────────
 * SI-3: lower() takes AST_PROGRAM node produced by code_to_ast() (SI-2 shim)
 * or directly by a frontend (SI-4+).  No CODE_t / STMT_t access here.
 *────────────────────────────────────────────────────────────────────────────*/

SM_Program *lower(const tree_t *prog)
{
    if (!prog || prog->t != AST_PROGRAM) return NULL;

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
        /* Icon defs are registered by polyglot_init; skip SM emission */
        int s_lang = (s->t == AST_STMT) ? attr_int_of(s, ":lang") : 0;
        if (s->t == AST_STMT && s_lang == LANG_ICN) {
            has_icn = 1;
            sm_stno_label_record(g_p, ++stno, NULL);
            continue;
        }
        lower_stmt(s);
        const char *lbl = (s->t == AST_STMT || s->t == AST_END)
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
        for (int k = 0; k < AST_KIND_COUNT; k++) {
            int w = k/64, b = k%64;
            if (w < LOWER_UNHANDLED_WORDS && (g_unhandled_kinds[w] >> b) & 1)
                fprintf(stderr, " %s", ast_e_name[k]);
        }
        fprintf(stderr, "\n");
    }

    return g_p;
}
