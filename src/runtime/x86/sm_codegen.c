/*
 * sm_codegen.c — SM_Program → x86-64 in-memory code (M-JIT-RUN)
 *
 * Architecture: per-instruction native blob emitter (ME-3, 2026-05-10)
 * ────────────────────────────────────────────────────────────────────
 * SEG_CODE holds, in order: one shared trampoline, then one fixed-size
 * 30-byte native blob per SM instruction.  Each blob does its work and
 * ends with `jmp trampoline`; SM_HALT's blob ends with `ret` (exiting
 * sm_jit_run); SM_JUMP's blob sets pc and direct-jumps to the target
 * blob (rel32, patched in pass 2 of sm_codegen).
 *
 * The trampoline reads st->pc (via [r12+20]) and indirect-jumps to
 * blob[pc] = blobs_base + pc * ME3_BLOB_SIZE.  No C dispatch loop.  No
 * per-opcode tail-call thunk in SEG_DISPATCH (that mechanism removed
 * in ME-3).
 *
 * Execution model:
 *   - g_jit_prog   points to the SM_Program (read by handlers for operands)
 *   - g_jit_pc     is the logical SM program counter (kept in st->pc; r12
 *                  points at SM_State so blobs read it via [r12+20])
 *   - g_jit_state  is a shared SM_State (stack, last_ok)
 *   - The 5 ME-3-named opcodes (SM_HALT, SM_PUSH_LIT_S, SM_PUSH_LIT_I,
 *     SM_VOID_POP, SM_JUMP) have dedicated blobs.  SM_HALT and SM_JUMP
 *     are pure native; SM_PUSH_LIT_S, SM_PUSH_LIT_I, SM_VOID_POP use the
 *     Standard blob (which calls the C handler).  ME-4+ replaces those
 *     Standards with inline native sequences.
 *   - All other opcodes use the Standard blob: inc pc, mov rax handler,
 *     call rax, jmp trampoline.  Same threaded-call shape as before
 *     ME-3 but laid out per-instruction in SEG_CODE rather than via a
 *     uint8_t** pointer array.
 *
 * ME-2 register convention (GOAL-MODE3-EMIT, 2026-05-10):
 *   r12 = SM_State*   — anchor for value stack, last_ok, pc.  Loaded by
 *                        sm_jit_run before transferring control to the
 *                        trampoline, survives across calls because r12
 *                        is callee-saved in the System V x86-64 C-ABI.
 *   r10 = per-glob data-region ptr (bb_flat.c convention; unchanged here)
 *   rbp = chunk frame for DEFINE'd chunks (ME-6, future)
 *   rbx, r13, r14, r15 — callee-saved working regs per bb_boxes.s convention
 *   rax rdi rsi rdx rcx r8 r9 r11 — caller-saved scratch
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-04-07 (M-JIT-RUN), 2026-05-10 (ME-2 r12 reservation, ME-3 native blobs)
 */

#include "sm_codegen.h"
#include "sm_image.h"
#include "sm_prog.h"
#include "sm_interp.h"   /* SM_State, sm_push, sm_pop, sm_state_init */
#include "snobol4.h"
#include "sil_macros.h"
#include "../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"  /* AST_t, AST_FNC for SM_PAT_CAPTURE_FN */
#include "bb_broker.h"   /* SN-9b: SM_BB_PUMP / SM_BB_ONCE handlers */

/* GOAL-ICON-BB-COMPLETE Phase A: file-scope externs for tripwire + bridge counter.
 * g_sm_dispatch_active: A0 tripwire — set while SM dispatch is running.
 * g_ast_pump_active: re-entrant suppression for intentional coro_eval bridges. */
extern int g_sm_dispatch_active;
extern int g_ast_pump_active;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "../../runtime/common/coerce.h"  /* shared_arith (F-1 RS-7) */
#include <setjmp.h>
#include <gc/gc.h>

/* ── JIT execution state (globals shared with handler functions) ──────── */

static SM_Program *g_jit_prog   = NULL;
static SM_State   *g_jit_state  = NULL;
static int         g_jit_halted = 0;

/* IM-5: JIT step-limit for in-process sync monitor */
int      g_jit_step_limit = 0;   /* 0 = unlimited; N = stop after N stmts */
int      g_jit_steps_done = 0;
jmp_buf  g_jit_step_jmp;

/* ME-1: jit_pat_stack removed — SM_PAT_* ops use value stack (PUSH/POP) */

/* ── Externs from snobol4 runtime ────────────────────────────────────── */

extern DESCR_t  NV_GET_fn(const char *name);
extern DESCR_t  NV_SET_fn(const char *name, DESCR_t val);  /* RT-5 */
extern char    *VARVAL_fn(DESCR_t d);
extern DESCR_t  CONCAT_fn(DESCR_t l, DESCR_t r);
extern DESCR_t  INVOKE_fn(const char *name, DESCR_t *args, int nargs);
extern DESCR_t  sc_dat_field_call(const char *name, DESCR_t *args, int nargs);
extern DESCR_t  NAME_fn(const char *name);
extern int      exec_stmt(const char *subj_name, DESCR_t *subj_var,
                          DESCR_t pat, DESCR_t *repl, int has_repl);
extern void     comm_stno(int n);

/* subscript helpers */
extern DESCR_t subscript_get(DESCR_t base, DESCR_t idx);
extern DESCR_t subscript_get2(DESCR_t base, DESCR_t i, DESCR_t j);
extern int     subscript_set(DESCR_t base, DESCR_t idx, DESCR_t val);    /* 1=ok, 0=fail */
extern int     subscript_set2(DESCR_t base, DESCR_t i, DESCR_t j, DESCR_t val); /* 1=ok, 0=fail */

/* pattern constructors */
extern DESCR_t pat_lit(const char *s);
extern DESCR_t pat_span(const char *chars);
extern DESCR_t pat_break_(const char *chars);
extern DESCR_t pat_breakx(const char *chars);
extern DESCR_t pat_any_cs(const char *chars);
extern DESCR_t pat_notany(const char *chars);
extern DESCR_t pat_len(int64_t n);
extern DESCR_t pat_pos(int64_t n);
extern DESCR_t pat_rpos(int64_t n);
extern DESCR_t pat_tab(int64_t n);
extern DESCR_t pat_rtab(int64_t n);
extern DESCR_t pat_arb(void);
extern DESCR_t pat_arbno(DESCR_t inner);
extern DESCR_t pat_arbno(DESCR_t inner);
extern DESCR_t pat_rem(void);
extern DESCR_t pat_fence(void);
extern DESCR_t pat_fail(void);
extern DESCR_t pat_abort(void);
extern DESCR_t pat_succeed(void);
extern DESCR_t pat_bal(void);
extern DESCR_t pat_epsilon(void);
extern DESCR_t pat_cat(DESCR_t left, DESCR_t right);
extern DESCR_t pat_alt(DESCR_t left, DESCR_t right);
extern DESCR_t pat_ref(const char *name);
extern DESCR_t pat_assign_imm(DESCR_t child, DESCR_t var);
extern DESCR_t pat_assign_cond(DESCR_t child, DESCR_t var);
extern DESCR_t pat_at_cursor(const char *varname);

/* F-1 RS-7: jit_arith / to_int / to_real replaced by shared_arith() in runtime/common/coerce.c */

/* ── Per-opcode handler functions ────────────────────────────────────── */
/*
 * Each handler is called with no arguments.
 * It reads g_jit_prog->instrs[g_jit_state->pc - 1] for its operands
 * (pc was already incremented by the dispatch loop before calling).
 * Jumps: set g_jit_state->pc directly.
 */

#define CUR_INS  (&g_jit_prog->instrs[g_jit_state->pc - 1])
#define STATE    (g_jit_state)
#define PUSH(d)  sm_push(STATE, (d))
#define POP()    sm_pop(STATE)

static void h_label(void)    { /* no-op */ }
static void h_halt(void)     { g_jit_halted = 1; }
static void h_define(void)   { /* handled by prescan */ }
/* RS-9c: SM-frame-aware return handlers — mirror sm_interp.c SM_RETURN/SM_FRETURN/SM_NRETURN */
static void h_return_impl(int is_fret, int is_nret)
{
    if (STATE->call_depth > 0) {
        SmCallFrame *fr = &STATE->call_stack[--STATE->call_depth];
        /* CHUNKS-step02: chunk thunks have retval_name==NULL — use stack top */
        DESCR_t retval  = fr->retval_name
            ? NV_GET_fn(fr->retval_name)
            : ((STATE->sp > 0) ? STATE->stack[STATE->sp - 1] : FAILDESCR);
        for (int k = fr->nsaved - 1; k >= 0; k--)
            NV_SET_fn(fr->saved_names[k], fr->saved_vals[k]);
        /* RS-9c: restore caller's value stack, then push return value on top */
        if (fr->caller_sp > 0 && fr->caller_stack) {
            if (fr->caller_sp > STATE->stack_cap) {
                STATE->stack = GC_realloc(STATE->stack, fr->caller_sp * sizeof(DESCR_t));
                STATE->stack_cap = fr->caller_sp;
            }
            memcpy(STATE->stack, fr->caller_stack, fr->caller_sp * sizeof(DESCR_t));
        }
        STATE->sp = fr->caller_sp;
        if (is_fret) {
            PUSH(FAILDESCR); STATE->last_ok = 0;
            strncpy(kw_rtntype, "FRETURN", sizeof(kw_rtntype)-1); /* RS-11 */
        } else if (is_nret) {
            /* SN-33b-nreturn: see sm_interp.c h_return SM_NRETURN — same fix.
             * NRETURN's retval (NV_GET_fn(retval_name)) is a NAME descriptor;
             * push the NAME-DEREFed value to match IR-run convention
             * (interp_eval.c:2771).  The previous push of NAMEVAL(retval_name)
             * substituted the function's own name for the actual return value. */
            DESCR_t deref = retval;
            if (IS_NAMEPTR(deref))      deref = NAME_DEREF_PTR(deref);
            else if (IS_NAMEVAL(deref)) deref = NV_GET_fn(deref.s);
            PUSH(deref);
            STATE->last_ok = 1;
            strncpy(kw_rtntype, "NRETURN", sizeof(kw_rtntype)-1); /* RS-11 */
        } else {
            PUSH(retval); STATE->last_ok = (retval.v != DT_FAIL);
            strncpy(kw_rtntype, "RETURN",  sizeof(kw_rtntype)-1); /* RS-11 */
        }
        STATE->pc = fr->ret_pc;
    } else {
        g_jit_halted = 1;
    }
}
static void h_return(void)    { h_return_impl(0, 0); }
static void h_freturn(void)   { h_return_impl(1, 0); }
static void h_nreturn(void)   { h_return_impl(0, 1); }
static void h_return_s(void)  { if ( STATE->last_ok) h_return_impl(0, 0); }
static void h_return_f(void)  { if (!STATE->last_ok) h_return_impl(0, 0); }
static void h_freturn_s(void) { if ( STATE->last_ok) h_return_impl(1, 0); }
static void h_freturn_f(void) { if (!STATE->last_ok) h_return_impl(1, 0); }
static void h_nreturn_s(void) { if ( STATE->last_ok) h_return_impl(0, 1); }
static void h_nreturn_f(void) { if (!STATE->last_ok) h_return_impl(0, 1); }

static void h_stno(void) {
    /* SN-32a-stno: source stno carried as operand by sm_lower (mirror of
     * SM_STNO's interp-side fix in sm_interp.c). */
    int sm_stno = (int)CUR_INS->a[0].i;
    comm_stno(sm_stno);
    kw_stno = sm_stno;
    /* SN-26-bridge-coverage-f: fire MWK_LABEL on every statement entry. */
    {
        extern void mon_emit_label_bin(int64_t stno);
        mon_emit_label_bin((int64_t)sm_stno);
    }
    /* IM-5: step-limit — longjmp out when limit reached */
    if (g_jit_step_limit > 0 && g_jit_steps_done++ >= g_jit_step_limit)
        longjmp(g_jit_step_jmp, 1);
}

static void h_jump(void)   { STATE->pc = (int)CUR_INS->a[0].i; }
static void h_jump_s(void) { if ( STATE->last_ok) STATE->pc = (int)CUR_INS->a[0].i; }
static void h_jump_f(void) { if (!STATE->last_ok) STATE->pc = (int)CUR_INS->a[0].i; }

static void h_push_lit_s(void)
{
    const char *s = CUR_INS->a[0].s ? CUR_INS->a[0].s : "";
    int64_t     n = CUR_INS->a[1].i;
    DESCR_t d; d.v = DT_S; d.s = (char *)s; d.slen = (n > 0) ? (uint32_t)n : 0;
    PUSH(d);
}
static void h_push_lit_i(void) { PUSH(INTVAL(CUR_INS->a[0].i)); }
static void h_push_lit_f(void) { PUSH(REALVAL(CUR_INS->a[0].f)); }
static void h_push_null(void)  { PUSH(NULVCL); STATE->last_ok = 1; }

static void h_push_var(void)
{
    /* SN-9c-c: SN-6 parity with sm_interp.c:261-268.  Keyword reads (e.g.
     * INPUT at EOF) return FAILDESCR; last_ok must reflect that so the
     * statement's :F branch fires.  Without this, a prior iteration's
     * last_ok=1 from a successful match bleeds across the loop-back goto
     * and makes `LINE = INPUT :F(END)` never fire at EOF — the JIT-only
     * failure mode of word1.sno / wordcount.sno. */
    DESCR_t val = NV_GET_fn(CUR_INS->a[0].s);
    PUSH(val);
    STATE->last_ok = (val.v != DT_FAIL);
}

/* SN-9a: frozen DT_E descriptor for *expr / EVAL().  Mirrors sm_interp.c
 * SM_PUSH_EXPR handler.  Note the union-aliasing hazard — d.s and d.ptr
 * share a union, so .ptr must be written last after .slen = 0 (same
 * ordering rule as the four constructor sites fixed at SN-6b). */
static void h_push_expr(void)
{
    DESCR_t d;
    d.v    = DT_E;
    d.slen = 0;
    d.ptr  = CUR_INS->a[0].ptr;
    PUSH(d);
    STATE->last_ok = 1;
}

/* CHUNKS-step02: SM_PUSH_EXPRESSION — push DT_E expression descriptor (slen=1, i=entry_pc). */
static void h_push_chunk(void)
{
    DESCR_t d;
    d.v    = DT_E;
    d.slen = 1;
    d.i    = (int64_t)CUR_INS->a[0].i;
    PUSH(d);
    STATE->last_ok = 1;
}
/* CHUNKS-step02: SM_CALL_EXPRESSION — push minimal SmCallFrame, jump to entry_pc.
 * Mirrors SM_CALL_EXPRESSION handler in sm_interp.c. */
static void h_call_chunk(void)
{
    int entry_pc = (int)CUR_INS->a[0].i;
    if (entry_pc < 0 || STATE->call_depth >= SM_CALL_STACK_MAX) {
        PUSH(FAILDESCR); STATE->last_ok = 0;
        return;
    }
    SmCallFrame *fr = &STATE->call_stack[STATE->call_depth++];
    fr->ret_pc = STATE->pc;
    fr->ret_ok = 1;
    fr->retval_name = NULL;
    fr->nsaved = 0;
    fr->caller_sp = STATE->sp;
    if (STATE->sp > 0) {
        fr->caller_stack = GC_malloc(STATE->sp * sizeof(DESCR_t));
        memcpy(fr->caller_stack, STATE->stack, STATE->sp * sizeof(DESCR_t));
    } else {
        fr->caller_stack = NULL;
    }
    STATE->sp = 0;
    STATE->pc = entry_pc;
}

/* SN-9b: Byrd-box broker opcodes — Icon (SM_BB_PUMP) / Prolog (SM_BB_ONCE).
 * Direct ports of sm_interp.c:612-635.  Polyglot programs emit these for
 * LANG_ICN and LANG_PL statements; codegen previously left both as
 * h_unimpl, silently producing last_ok=0 on every Icon/Prolog statement.
 *
 * coro_eval and pump_print live in scrip.c / sm_interp.c — declared
 * extern here so the handler bodies are identical to the sm_interp cases. */
extern bb_node_t coro_eval(AST_t *e);
static void jit_pump_print(DESCR_t val, void *arg)
{
    (void)arg;
    char *s = VARVAL_fn(val);
    if (s) printf("%s\n", s);
}

static void h_bb_pump(void)
{
    DESCR_t expr_d = POP();
    AST_t *expr   = (AST_t *)expr_d.ptr;
    if (!expr) { STATE->last_ok = 0; return; }
    bb_node_t node = coro_eval(expr);
    int ticks = bb_broker(node, BB_PUMP, jit_pump_print, NULL);
    STATE->last_ok = (ticks > 0);
}

static void h_bb_once(void)
{
    DESCR_t expr_d = POP();
    AST_t *expr   = (AST_t *)expr_d.ptr;
    if (!expr) { STATE->last_ok = 0; return; }
    bb_node_t node = coro_eval(expr);
    int ticks = bb_broker(node, BB_ONCE, NULL, NULL);
    STATE->last_ok = (ticks > 0);
}

/* CH-17f: Prolog name-driven BB_ONCE — mirror of SM_BB_ONCE_PROC handler
 * in sm_interp.c.  No AST_t* pushed or walked at the SM layer.
 * IR fallback path until expression bodies are filled in a later rung. */
#include "../../frontend/prolog/pl_broker.h"
#include "../../runtime/interp/pl_runtime.h"
static void h_bb_once_proc(void)
{
    const char   *key   = CUR_INS->a[0].s;
    int           arity = (int)CUR_INS->a[1].i;
    AST_t       *choice = key ? pl_pred_table_lookup_global(key) : NULL;
    bb_node_t     node   = choice ? pl_box_choice(choice, g_pl_env, arity)
                                  : pl_box_fail();
    int ticks = bb_broker(node, BB_ONCE, NULL, NULL);
    STATE->last_ok = (ticks > 0);
}

/* CHUNKS-step12: name-driven Icon proc BB pump — mirror of SM_BB_PUMP_PROC
 * handler in sm_interp.c. The synthesised AST_FNC + emit_push_expr path is
 * gone; the lowerer emits SM_BB_PUMP_PROC "main", 0 directly. */
extern bb_node_t coro_pump_proc_by_name(const char *name, DESCR_t *args, int nargs);
static void h_bb_pump_proc(void)
{
    const char *name  = CUR_INS->a[0].s;
    int         nargs = (int)CUR_INS->a[1].i;
    DESCR_t *args = NULL;
    if (nargs > 0) {
        args = calloc(nargs, sizeof(DESCR_t));
        for (int k = nargs - 1; k >= 0; k--) args[k] = POP();
    }
    bb_node_t node = coro_pump_proc_by_name(name, args, nargs);
    if (!node.fn) {
        if (args) free(args);
        STATE->last_ok = 0;
        return;
    }
    g_ast_pump_active++;
    int ticks = bb_broker(node, BB_PUMP, jit_pump_print, NULL);
    g_ast_pump_active--;
    STATE->last_ok = (ticks > 0);
}

/* CHUNKS-step13: Raku CASE dispatch — JIT mirror of SM_BB_PUMP_CASE
 * handler in sm_interp.c. Stack discipline and comparison semantics
 * are identical; see sm_interp.c for the exhaustive comment. The two
 * handlers must stay in lockstep — any change to one needs the same
 * change to the other. */
static void h_bb_pump_case(void)
{
    int ncases      = (int)CUR_INS->a[0].i;
    int has_default = (int)CUR_INS->a[1].i;

    int default_pc = -1;
    if (has_default) {
        DESCR_t d = POP();
        default_pc = (d.v == DT_E && d.slen == 1) ? (int)d.i : -1;
    }

    int *cmp_kinds = (int*)GC_malloc(sizeof(int) * (ncases > 0 ? ncases : 1));
    int *val_pcs   = (int*)GC_malloc(sizeof(int) * (ncases > 0 ? ncases : 1));
    int *body_pcs  = (int*)GC_malloc(sizeof(int) * (ncases > 0 ? ncases : 1));
    for (int k = ncases - 1; k >= 0; k--) {
        DESCR_t b = POP();
        DESCR_t v = POP();
        DESCR_t c = POP();
        body_pcs[k]  = (b.v == DT_E && b.slen == 1) ? (int)b.i : -1;
        val_pcs[k]   = (v.v == DT_E && v.slen == 1) ? (int)v.i : -1;
        cmp_kinds[k] = (c.v == DT_I) ? (int)c.i : (int)AST_EQ;
    }

    DESCR_t topic_d = POP();
    int topic_pc = (topic_d.v == DT_E && topic_d.slen == 1) ? (int)topic_d.i : -1;
    DESCR_t topic = (topic_pc >= 0) ? sm_call_expression(topic_pc) : NULVCL;

    DESCR_t result = NULVCL;
    int matched = 0;
    for (int k = 0; k < ncases; k++) {
        if (val_pcs[k] < 0 || body_pcs[k] < 0) continue;
        DESCR_t wval = sm_call_expression(val_pcs[k]);
        int match = 0;
        if ((AST_e)cmp_kinds[k] == AST_LEQ) {
            const char *ts = IS_STR_fn(topic) ? topic.s : VARVAL_fn(topic);
            const char *ws = IS_STR_fn(wval)  ? wval.s  : VARVAL_fn(wval);
            match = (ts && ws && strcmp(ts, ws) == 0);
        } else {
            if (IS_INT_fn(topic) && IS_INT_fn(wval)) {
                match = (topic.i == wval.i);
            } else {
                const char *ts = VARVAL_fn(topic);
                const char *ws = VARVAL_fn(wval);
                match = (ts && ws && strcmp(ts, ws) == 0);
            }
        }
        if (match) {
            result = sm_call_expression(body_pcs[k]);
            matched = 1;
            break;
        }
    }
    if (!matched && default_pc >= 0) {
        result = sm_call_expression(default_pc);
        matched = 1;
    }

    PUSH(result);
    STATE->last_ok = matched;
}

/* CHUNKS-step15: BB pump for an SM generator expression — JIT mirror of
 * SM_BB_PUMP_SM handler in sm_interp.c.  The expression body itself runs
 * interpreted via bb_broker_drive_sm → sm_interp_run, identical to how
 * h_bb_pump_case routes per-arm expressions through sm_call_expression → the
 * interpreter.  No JIT-of-the-expression-body needed in this rung; that is
 * M5/EM-10 territory.  The two handlers must stay in lockstep. */
static void h_bb_pump_sm(void)
{
    DESCR_t d = POP();
    if (d.v != DT_E || d.slen != 1) {
        STATE->last_ok = 0;
        return;
    }
    int entry_pc = (int)d.i;
    SM_Program *prog = g_current_sm_prog;
    if (!prog || entry_pc < 0 || entry_pc >= prog->count) {
        STATE->last_ok = 0;
        return;
    }
    SmGenState *gs = sm_gen_state_new(entry_pc);
    int ticks = bb_broker_drive_sm(gs, jit_pump_print, NULL);
    STATE->last_ok = (ticks > 0);
}

/* CHUNKS-step17i-every-suspend: JIT mirror of SM_BB_PUMP_EVERY in sm_interp.c.
 * Looks up AST by id from g_every_table, builds a drivable bb_node_t via
 * coro_eval (existing IR-side every box), drives via bb_broker(BB_PUMP).
 * Body-fn is NULL — the AST_EVERY do-clause (the user's body, e.g.
 * `write(v)`) already runs via bb_exec_stmt inside coro_bb_every; passing
 * jit_pump_print would double-print yielded values.
 * Pushes DT_NUL to balance the trailing SM_VOID_POP from proc-body lowering. */
static void h_bb_pump_every(void)
{
    int every_id = (int)CUR_INS->a[0].i;
    AST_t *every_ast = every_table_lookup(every_id);
    if (!every_ast) {
        STATE->last_ok = 0;
        PUSH(NULVCL);
        return;
    }
    g_ast_pump_active++;
    bb_node_t node = coro_eval(every_ast);
    int ticks = bb_broker(node, BB_PUMP, NULL, NULL);
    g_ast_pump_active--;
    STATE->last_ok = (ticks > 0);
    PUSH(NULVCL);
}

/* GOAL-ICON-BB-COMPLETE Phase A: JIT mirror of SM_BB_PUMP_AST in sm_interp.c.
 * Drives one alpha step of a legacy-fallthrough kind (BANG_BINARY, LCONCAT-gen,
 * ITERATE, LIMIT, RANDOM, SECTION*) via coro_eval -> bb_node alpha.
 * Pushes the result on success, NULVCL on fail. */
static void h_bb_pump_ast(void)
{
    int ast_id = (int)CUR_INS->a[0].i;
    AST_t *ast = ast_pump_table_lookup(ast_id);
    if (!ast) {
        STATE->last_ok = 0;
        PUSH(NULVCL);
        return;
    }
    int saved = g_sm_dispatch_active;
    g_sm_dispatch_active = 0;
    g_ast_pump_active++;
    bb_node_t node = coro_eval(ast);
    DESCR_t result = node.fn(node.ζ, α);
    g_ast_pump_active--;
    g_sm_dispatch_active = saved;
    if (IS_FAIL_fn(result)) {
        STATE->last_ok = 0;
        PUSH(NULVCL);
    } else {
        STATE->last_ok = 1;
        PUSH(result);
    }
}

/* CHUNKS-step17i-suspend: JIT mirror of SM_SUSPEND_VALUE in sm_interp.c.
 * Same yield-to-caller protocol — sm_yield_to_caller (in coro_runtime.c) does
 * the swapcontext to active_coro's caller_ctx.  See SM_SUSPEND_VALUE doc in
 * sm_prog.h for the lowering shape and rationale. */
extern int sm_yield_to_caller(DESCR_t v);   /* coro_runtime.c */
static void h_suspend_value(void)
{
    DESCR_t v = POP();
    if (sm_yield_to_caller(v)) {
        STATE->last_ok = 1;
    } else {
        /* No coroutine — push value back; outer SM_VOID_POP discards it. */
        PUSH(v);
        STATE->last_ok = !IS_FAIL_fn(v);
    }
}

static void h_store_var(void)
{
    DESCR_t val = POP();
    if (val.v == DT_FAIL) {
        /* SN-32c-store-fail: push FAILDESCR so enclosing calls (e.g.
         * DIFFER(sno = Pop()) where Pop() FRETURNs) see a balanced
         * stack.  Without the push, the enclosing SM_CALL_FN pops a
         * stale value, corrupting the arg and mis-setting last_ok.
         * Mirrors SN-32b-store-fail fix in sm_interp.c. */
        PUSH(FAILDESCR);
        STATE->last_ok = 0;
        return;
    }
    /* SN-32c-store-val: push `val` (the RHS value), NOT the return value of
     * NV_SET_fn.  The IR path (interp.c AST_ASSIGN, line 2844) always returns
     * `val` regardless of what NV_SET_fn stores — NV_SET_fn's return value
     * is unreliable for DT_DATA objects (returns SNUL on the second call
     * for the same variable).  Pushing `val` ensures DIFFER(sno=Pop()) sees
     * the actual DATA value, not a stripped SNUL.  Mirrors SN-32b-store-val
     * fix in sm_interp.c. */
    NV_SET_fn(CUR_INS->a[0].s, val);
    PUSH(val);
    /* SN-9c-c: SN-6 parity with sm_interp.c:296-301.  Successful assignment
     * sets last_ok=1 so a prior failure (e.g. pattern-match FAIL in the
     * previous iteration) does not bleed into this statement's :F branch
     * across a loop-back goto.  Root cause of JIT-only word1/wordcount
     * premature termination. */
    STATE->last_ok = 1;
}

static void h_pop(void) { POP(); }

static void h_arith(void)
{
    DESCR_t r = POP(), l = POP();
    /* SN-9c-c-bis: full parity with sm_interp.c:321-331.  Three pieces JIT
     * was missing: FAIL propagation (e.g. CHARS + SIZE(INPUT) at EOF) and
     * DT_SNUL→INTVAL(0) coercion (unset variables read as null string).
     * Without the SNUL coercion, `N + 1` with unset N left l.v=DT_SNUL,
     * jit_arith fell through to the REALVAL branch, and the result
     * propagated as DT_R — formatter then emitted `2.` instead of `2`. */
    if (l.v == DT_FAIL || r.v == DT_FAIL) {
        PUSH(FAILDESCR);
        STATE->last_ok = 0;
        return;
    }
    if (l.v == DT_S) l = INTVAL(to_int(l));
    if (r.v == DT_S) r = INTVAL(to_int(r));
    if (l.v == DT_SNUL) l = INTVAL(0);
    if (r.v == DT_SNUL) r = INTVAL(0);
    DESCR_t result = shared_arith(l, r, CUR_INS->op);
    PUSH(result);
    STATE->last_ok = (result.v != DT_FAIL);
}

static void h_neg(void)
{
    DESCR_t v = POP();
    if (v.v == DT_I) PUSH(INTVAL(-v.i));
    else              PUSH(REALVAL(-to_real(v)));
}

static void h_concat(void)
{
    DESCR_t r = POP(), l = POP();
    DESCR_t result = CONCAT_fn(l, r);
    PUSH(result);
    STATE->last_ok = (result.v != DT_FAIL);
}

static void h_coerce_num(void)
{
    DESCR_t v = POP();
    if (v.v == DT_S) {
        int64_t iv = to_int(v);
        if (iv != 0 || (v.s && v.s[0] == '0')) PUSH(INTVAL(iv));
        else PUSH(REALVAL(to_real(v)));
    } else { PUSH(v); }
    STATE->last_ok = 1;
}

/* ── ME-4 value-in/value-out helpers ─────────────────────────────────────
 *
 * The me4_* family is called directly by ME-4 inline-native blobs.  Each
 * function takes its operands by value (via DESCR_t struct-pass in
 * rdi:rsi / rdx:rcx etc.) and returns the result by value (rax:rdx for
 * 16-byte DESCR_t).  No stack access — the blob does pop/push around
 * the call.  Side effect: `g_jit_state->last_ok` is updated to reflect
 * the operation's success.  Mirrors h_arith / h_concat / h_coerce_num
 * semantics so byte-identical output is guaranteed across `--sm-run` and
 * `--jit-run`.
 *
 * Why a parallel function family (instead of refactoring h_*):
 *   - The h_* family is called via the indirect `mov rax, imm64 ; call rax`
 *     blob shape and reads/writes STATE->stack implicitly.  Switching its
 *     ABI would also touch sm_jit_run_plain and any future debug path that
 *     calls handlers via the g_handlers[] table.
 *   - me4_* is a clean island: blob ↔ me4_* with one ABI, no ambiguity.
 *   - The two paths share underlying primitives (shared_arith, CONCAT_fn,
 *     coercion table) so logic isn't duplicated. */

DESCR_t me4_arith(DESCR_t l, DESCR_t r, sm_opcode_t op)
{
    if (l.v == DT_FAIL || r.v == DT_FAIL) {
        g_jit_state->last_ok = 0;
        return FAILDESCR;
    }
    if (l.v == DT_S)    l = INTVAL(to_int(l));
    if (r.v == DT_S)    r = INTVAL(to_int(r));
    if (l.v == DT_SNUL) l = INTVAL(0);
    if (r.v == DT_SNUL) r = INTVAL(0);
    DESCR_t result = shared_arith(l, r, op);
    g_jit_state->last_ok = (result.v != DT_FAIL);
    return result;
}

DESCR_t me4_concat(DESCR_t l, DESCR_t r)
{
    DESCR_t result = CONCAT_fn(l, r);
    g_jit_state->last_ok = (result.v != DT_FAIL);
    return result;
}

DESCR_t me4_coerce_num(DESCR_t v)
{
    g_jit_state->last_ok = 1;
    if (v.v == DT_S) {
        int64_t iv = to_int(v);
        if (iv != 0 || (v.s && v.s[0] == '0')) return INTVAL(iv);
        return REALVAL(to_real(v));
    }
    return v;
}

DESCR_t me4_push_var(const char *name)
{
    DESCR_t val = NV_GET_fn(name);
    g_jit_state->last_ok = (val.v != DT_FAIL);
    return val;
}

/* SM_STORE_VAR — pops val off the stack, stores it into NV[name], pushes
 * val back onto the stack as the assignment's value.  This helper does
 * the NV write and last_ok update; the blob does pop-arg / push-result. */
DESCR_t me4_store_var(const char *name, DESCR_t val)
{
    if (val.v == DT_FAIL) {
        g_jit_state->last_ok = 0;
        return FAILDESCR;
    }
    NV_SET_fn(name, val);
    g_jit_state->last_ok = 1;
    return val;
}

/* Pure-data push helpers used by SM_PUSH_NULL.  Returns the constant
 * NULVCL; setting last_ok=1.  Inline blob calls this once (no args) and
 * pushes the result. */
DESCR_t me4_push_null(void)
{
    g_jit_state->last_ok = 1;
    return NULVCL;
}

static void h_pat_lit(void)
{
    PUSH(pat_lit(CUR_INS->a[0].s ? CUR_INS->a[0].s : ""));
}
static void h_pat_any(void)
{
    DESCR_t arg = POP(); const char *cs = VARVAL_fn(arg);
    PUSH(pat_any_cs(cs ? cs : ""));
}
static void h_pat_notany(void)
{
    DESCR_t arg = POP(); const char *cs = VARVAL_fn(arg);
    PUSH(pat_notany(cs ? cs : ""));
}
static void h_pat_span(void)
{
    DESCR_t arg = POP(); const char *cs = VARVAL_fn(arg);
    PUSH(pat_span(cs ? cs : ""));
}
static void h_pat_break(void)
{
    DESCR_t arg = POP(); const char *cs = VARVAL_fn(arg);
    PUSH(pat_break_(cs ? cs : ""));
}
static void h_pat_len(void)
{
    DESCR_t arg = POP();
    PUSH(pat_len(arg.v == DT_I ? arg.i : 0));
}
static void h_pat_pos(void)
{
    DESCR_t arg = POP();
    PUSH(pat_pos(arg.v == DT_I ? arg.i : 0));
}
static void h_pat_rpos(void)
{
    DESCR_t arg = POP();
    PUSH(pat_rpos(arg.v == DT_I ? arg.i : 0));
}
static void h_pat_tab(void)
{
    DESCR_t arg = POP();
    PUSH(pat_tab(arg.v == DT_I ? arg.i : 0));
}
static void h_pat_rtab(void)
{
    DESCR_t arg = POP();
    PUSH(pat_rtab(arg.v == DT_I ? arg.i : 0));
}
static void h_pat_arb(void)     { PUSH(pat_arb()); }
static void h_pat_arbno(void)   { DESCR_t _inner = POP(); PUSH(pat_arbno(_inner)); }
static void h_pat_rem(void)     { PUSH(pat_rem()); }
static void h_pat_fail(void)    { PUSH(pat_fail()); }
static void h_pat_succeed(void) { PUSH(pat_succeed()); }
static void h_pat_eps(void)     { PUSH(pat_epsilon()); }
static void h_pat_fence(void)   { PUSH(pat_fence()); }
static void h_pat_fence1(void)  { DESCR_t _ch = POP(); PUSH(pat_fence_p(_ch)); }
static void h_pat_abort(void)   { PUSH(pat_abort()); }
static void h_pat_bal(void)     { PUSH(pat_bal()); }

static void h_pat_cat(void)
{
    DESCR_t right = POP(), left = POP();
    PUSH(pat_cat(left, right));
}
static void h_pat_alt(void)
{
    DESCR_t right = POP(), left = POP();
    PUSH(pat_alt(left, right));
}
/* h_pat_boxval deleted by ME-1 — SM_PAT_BOXVAL opcode removed */

static void h_pat_deref(void)
{
    DESCR_t v = POP();
    if (v.v == DT_P) {
        PUSH(v);
    } else if (v.v == DT_S && v.s) {
        PUSH(pat_lit(v.s));
    } else {
        const char *name = VARVAL_fn(v);
        PUSH(pat_ref(name ? name : ""));
    }
}

static void h_pat_refname(void)
{
    /* SN-6: *var in pattern context — build XDSAR from the NAME,
     * never fetching variable's current value at build time.
     * Mirrors sm_interp.c case SM_PAT_REFNAME. */
    const char *name = CUR_INS->a[0].s ? CUR_INS->a[0].s : "";
    PUSH(pat_ref(name));
}

static void h_pat_capture(void)
{
    DESCR_t child  = POP();
    const char *vn = CUR_INS->a[0].s ? CUR_INS->a[0].s : "";
    DESCR_t var    = NAME_fn(vn);
    int kind       = (int)CUR_INS->a[1].i;
    if (kind == 1)
        PUSH(pat_assign_imm(child, var));
    else if (kind == 2)
        PUSH(pat_cat(child, pat_at_cursor(vn)));
    else
        PUSH(pat_assign_cond(child, var));
}

static void h_pat_capture_fn(void)
{
    /* . *func() or $ *func() — a[0].s = function name.
     * a[2].s (TL-2): optional '\t'-separated arg *names* for flush-time
     * resolution — set when every arg of *func() is a plain AST_VAR.
     * Use pat_assign_callcap → XCALLCAP, lowered to bb_cap with NM_CALL
     * NameKind_t (SN-21d).  The old DT_E/pat_assign_cond approach only
     * worked via materialise() which is not used in the byrd-box
     * (--sm-run / --jit-emit) path. */
    DESCR_t child  = POP();
    const char *fname = CUR_INS->a[0].s ? CUR_INS->a[0].s : "";
    const char *namelist = CUR_INS->a[2].s;
    if (namelist && namelist[0]) {
        int nnames = 1;
        for (const char *q = namelist; *q; q++) if (*q == '\t') nnames++;
        char **names = (char **)GC_MALLOC((size_t)nnames * sizeof(char *));
        int ni = 0;
        const char *start = namelist;
        for (const char *q = namelist; ; q++) {
            if (*q == '\t' || *q == '\0') {
                size_t len = (size_t)(q - start);
                char *nm = (char *)GC_MALLOC(len + 1);
                memcpy(nm, start, len);
                nm[len] = '\0';
                names[ni++] = nm;
                if (*q == '\0') break;
                start = q + 1;
            }
        }
        int is_imm = (int)CUR_INS->a[1].i;  /* SN-26c-parseerr-f */
        PUSH(is_imm
            ? pat_assign_callcap_named_imm(child, fname, NULL, 0, names, nnames)
            : pat_assign_callcap_named(child, fname, NULL, 0, names, nnames));
    } else {
        int is_imm = (int)CUR_INS->a[1].i;  /* SN-26c-parseerr-f */
        PUSH(is_imm
            ? pat_assign_callcap_named_imm(child, fname, NULL, 0, NULL, 0)
            : pat_assign_callcap(child, fname, NULL, 0));
    }
}

static void h_pat_capture_fn_args(void)
{
    /* SN-8a: . *func(args) / $ *func(args) — args-on-stack form.
     * a[0].s = fname, a[1].i = kind, a[2].i = nargs.  Args were pushed in
     * order 0..nargs-1 onto the value stack; pop into positions nargs-1..0
     * to reconstruct original order.  Then pop child pattern and build
     * pat_assign_callcap(child, fname, values, nargs). */
    int nargs = (int)CUR_INS->a[2].i;
    DESCR_t *argv = nargs > 0
        ? (DESCR_t *)GC_MALLOC((size_t)nargs * sizeof(DESCR_t))
        : NULL;
    for (int i = nargs - 1; i >= 0; i--) argv[i] = POP();
    DESCR_t child = POP();
    const char *fname = CUR_INS->a[0].s ? CUR_INS->a[0].s : "";
    int is_imm = (int)CUR_INS->a[1].i;  /* SN-26c-parseerr-f: 0=cond(.) 1=imm($) */
    PUSH(is_imm
        ? pat_assign_callcap_named_imm(child, fname, argv, nargs, NULL, 0)
        : pat_assign_callcap(child, fname, argv, nargs));
}

static void h_pat_usercall(void)
{
    /* SN-17a: bare *func() in pattern context.
     * a[0].s = function name; a[2].s = '\t'-separated arg names (or NULL).
     * No child pattern is popped — bare *fn() wraps nothing.
     * Build XATP deferred-usercall node so the match engine invokes func()
     * per position; func's FAIL propagates as pattern FAIL (landing in SN-17d). */
    const char *fname = CUR_INS->a[0].s ? CUR_INS->a[0].s : "";
    PUSH(pat_user_call(fname, NULL, 0));
}

static void h_pat_usercall_args(void)
{
    /* SN-8a: bare *func(args) in pattern context — args-on-stack form.
     * a[0].s = fname, a[1].i = nargs.  Pop nargs values (last-pushed = last
     * arg), build XATP deferred-usercall with the evaluated args.
     * No child pattern is popped — bare *fn() wraps nothing. */
    int nargs = (int)CUR_INS->a[1].i;
    DESCR_t *argv = nargs > 0
        ? (DESCR_t *)GC_MALLOC((size_t)nargs * sizeof(DESCR_t))
        : NULL;
    for (int i = nargs - 1; i >= 0; i--) argv[i] = POP();
    const char *fname = CUR_INS->a[0].s ? CUR_INS->a[0].s : "";
    PUSH(pat_user_call(fname, argv, nargs));
}

static void h_exec_stmt(void)
{
    /* ME-1: pattern is now on the value stack; sm_lower push order: pat, subj, repl; pop: repl, subj, pat */
    int has_repl   = (int)CUR_INS->a[1].i;
    DESCR_t repl   = POP();    /* replacement or INTVAL(0) — top of stack */
    DESCR_t subj_d = POP();    /* subject descriptor */
    DESCR_t pat_d  = POP();    /* pattern (DT_P) — pushed first by sm_lower */
    const char *sn = CUR_INS->a[0].s;
    int ok = exec_stmt(sn, &subj_d, pat_d, has_repl ? &repl : NULL, has_repl);
    STATE->last_ok = ok;
}

static void h_call(void)
{
    const char *name  = CUR_INS->a[0].s;
    int         nargs = (int)CUR_INS->a[1].i;

    if (name && strcmp(name, "INDIR_GET") == 0) {
        /* $expr: pop descriptor, look up variable, push its value.  Must fold
         * the name to SNOBOL4 canonical case (SN-19) or $'bal' won't find the
         * variable the parser stored as BAL.  Parity with sm_interp.c:644. */
        DESCR_t name_d = POP(), val;
        if (IS_NAMEPTR(name_d)) {
            val = NAME_DEREF_PTR(name_d);
        } else if (IS_NAMEVAL(name_d)) {
            char *fn = GC_strdup(name_d.s); sno_fold_name(fn);  /* SN-19 */
            val = NV_GET_fn(fn);
        } else {
            const char *vn0 = VARVAL_fn(name_d);
            char *vn = (vn0 && *vn0) ? GC_strdup(vn0) : NULL;
            if (vn) sno_fold_name(vn);                          /* SN-19 */
            val = (vn && *vn) ? NV_GET_fn(vn) : NULVCL;
        }
        PUSH(val); STATE->last_ok = 1; return;
    }
    if (name && strcmp(name, "NAME_PUSH") == 0) {
        /* .X: pop name string, push DT_N NAMEVAL descriptor with folded name
         * so downstream lookups hit the same NV key the parser produced. */
        DESCR_t nd = POP();
        const char *vn0 = VARVAL_fn(nd);
        char *vn = GC_strdup(vn0 ? vn0 : "");
        sno_fold_name(vn);                                      /* SN-19 */
        PUSH(NAMEVAL(vn)); STATE->last_ok = 1; return;
    }
    if (name && strcmp(name, "ASGN_INDIR") == 0) {
        /* $name = val: same folding rule — must hit the same NV key. */
        DESCR_t nd = POP(), val = POP();
        int ok = 0;
        if (IS_NAMEPTR(nd)) {
            *(DESCR_t*)nd.ptr = val; ok = 1;
        } else if (IS_NAMEVAL(nd)) {
            char *fn = GC_strdup(nd.s); sno_fold_name(fn);      /* SN-19 */
            NV_SET_fn(fn, val); ok = 1;
        } else {
            const char *vn0 = VARVAL_fn(nd);
            char *vn = (vn0 && *vn0) ? GC_strdup(vn0) : NULL;
            if (vn) sno_fold_name(vn);                          /* SN-19 */
            if (vn && *vn) { NV_SET_fn(vn, val); ok = 1; }
        }
        PUSH(val); STATE->last_ok = ok; return;
    }
    if (name && strcmp(name, "NRETURN_ASGN") == 0) {
        /* NRETURN lvalue assignment: fname() = rhs
         * Encoding: a[0].s = "NRETURN_ASGN", a[1].s = function name (the
         * sm_lower pass overwrites a[1].s after sm_emit_si set a[1].i=1).
         * Stack: [rhs].  Call zero-param user fn; if it returns DT_N write
         * through the name, else try fname_SET(rhs, result) field mutator.
         * Parity with sm_interp.c:704. */
        const char *fname = CUR_INS->a[1].s;
        DESCR_t rhs = POP();
        DESCR_t fres = INVOKE_fn(fname, NULL, 0);
        int ok = 0;
        if (IS_NAMEPTR(fres)) { NAME_DEREF_PTR(fres) = rhs; ok = 1; }
        else if (IS_NAMEVAL(fres)) {
            char *fn = GC_strdup(fres.s); sno_fold_name(fn);    /* SN-19 */
            NV_SET_fn(fn, rhs); ok = 1;
        }
        else {
            /* Field mutator fallback: fname_SET(rhs, obj) */
            char setname[256];
            snprintf(setname, sizeof(setname), "%s_SET", fname ? fname : "");
            DESCR_t sargs[2] = { rhs, fres };
            DESCR_t sr = INVOKE_fn(setname, sargs, 2);
            ok = (sr.v != DT_FAIL);
        }
        PUSH(rhs); STATE->last_ok = ok; return;
    }
    if (name && strcmp(name, "IDX") == 0) {
        if (nargs == 2) {
            DESCR_t idx = POP(), base = POP();
            DESCR_t r = subscript_get(base, idx);
            PUSH(r); STATE->last_ok = (r.v != DT_FAIL);
        } else if (nargs == 3) {
            DESCR_t j = POP(), i = POP(), base = POP();
            DESCR_t r = subscript_get2(base, i, j);
            PUSH(r); STATE->last_ok = (r.v != DT_FAIL);
        } else {
            /* N-dim (nargs >= 4): sm_lower pushed base first, then indices.
             * Stack top→bot: idx[n-1]...idx[0], base. Pop n items. */
            int n = nargs;
            DESCR_t raw[32];
            for (int k = 0; k < n; k++) raw[k] = POP();
            /* raw[0]=last_idx, raw[n-2]=first_idx, raw[n-1]=base */
            DESCR_t base = raw[n-1];
            DESCR_t fargs[32]; fargs[0] = base;
            for (int k = 0; k < n-1; k++) fargs[k+1] = raw[n-2-k];
            DESCR_t r = INVOKE_fn("ITEM", fargs, n);
            PUSH(r); STATE->last_ok = (r.v != DT_FAIL);
        }
        return;
    }
    if (name && strcmp(name, "IDX_SET") == 0) {
        extern void comm_var(const char *, DESCR_t);
        if (nargs == 3) {
            DESCR_t i = POP(), base = POP(), val = POP();
            STATE->last_ok = subscript_set(base, i, val);
            comm_var("<lval>", val);   /* SN-32a-idxset: parity with SM/IR */
            PUSH(val);
        } else if (nargs == 4) {
            DESCR_t j = POP(), i = POP(), base = POP(), val = POP();
            STATE->last_ok = subscript_set2(base, i, j, val);
            comm_var("<lval>", val);
            PUSH(val);
        } else {
            /* N-dim (nargs >= 5): sm_lower pushed rhs, base, then indices.
             * Stack top→bot: idx[n-1]...idx[0], base, rhs(val). ndim=nargs-2. */
            int ndim = nargs - 2;
            DESCR_t idx[32];
            for (int k = ndim - 1; k >= 0; k--) idx[k] = POP();
            DESCR_t base = POP(), val = POP();
            DESCR_t fargs[32]; fargs[0] = val; fargs[1] = base;
            for (int k = 0; k < ndim; k++) fargs[k+2] = idx[k];
            DESCR_t r = INVOKE_fn("ITEM_SET", fargs, ndim + 2);
            STATE->last_ok = (r.v != DT_FAIL);
            comm_var("<lval>", val);
            PUSH(val);
        }
        return;
    }

    DESCR_t args[32];
    if (nargs > 32) nargs = 32;
    for (int k = nargs - 1; k >= 0; k--) args[k] = POP();
    /* SN-9c-d: SN-6 parity with sm_interp.c:799-810.  SNOBOL4 semantics — if
     * any argument is FAIL, the call fails without invoking the function.
     * This is what allows CHARS + SIZE(INPUT) :F(DONE) to branch when INPUT
     * hits EOF: INPUT returns FAILDESCR → SIZE receives it → SIZE would
     * swallow it and return 0, but we catch the FAIL here before the call.
     * Without this, the loop-exit :F branch never fires and fileinfo.sno
     * hangs (accumulator stays at same value forever). */
    for (int k = 0; k < nargs; k++) {
        if (args[k].v == DT_FAIL) {
            PUSH(FAILDESCR);
            STATE->last_ok = 0;
            return;
        }
    }
    /* DATA field accessor/mutator/constructor: give DATA dispatch priority over
     * same-named builtins (e.g. field 'real' vs REAL() builtin). */
    DESCR_t result = FAILDESCR;
    int _data_first = (nargs >= 1 && args[0].v == DT_DATA);
    int _data_set   = (nargs >= 2 && args[1].v == DT_DATA && name &&
                       strlen(name) > 4 &&
                       strcasecmp(name + strlen(name) - 4, "_SET") == 0);
    if (_data_first || _data_set)
        result = sc_dat_field_call(name, args, nargs);

    /* RS-9c: SM-native user-function dispatch (mirrors sm_interp.c RS-9a path).
     * If DATA dispatch failed/skipped, look for an SM body before INVOKE_fn. */
    if (result.v == DT_FAIL || (!_data_first && !_data_set)) {
        int body_pc = -1;
        if (!_data_first && !_data_set && name) {
            body_pc = sm_label_pc_lookup(g_jit_prog, name);
            if (body_pc < 0) {
                char uname[128]; size_t nl = strlen(name);
                if (nl >= sizeof(uname)) nl = sizeof(uname) - 1;
                for (size_t _i = 0; _i <= nl; _i++)
                    uname[_i] = (char)toupper((unsigned char)name[_i]);
                body_pc = sm_label_pc_lookup(g_jit_prog, uname);
            }
            if (body_pc < 0) {
                const char *entry = FUNC_ENTRY_fn(name);
                if (entry) body_pc = sm_label_pc_lookup(g_jit_prog, entry);
            }
        }
        if (body_pc >= 0 && STATE->call_depth < SM_CALL_STACK_MAX) {
            /* Push JIT call frame — same layout as sm_interp RS-9a */
            SmCallFrame *fr = &STATE->call_stack[STATE->call_depth++];
            fr->ret_pc = STATE->pc;   /* resume after this SM_CALL_FN */
            fr->ret_ok = 1;
            /* RS-9c: save caller's value stack so SM_STNO resets inside callee don't wipe it */
            fr->caller_sp = STATE->sp;
            if (STATE->sp > 0) {
                fr->caller_stack = GC_malloc(STATE->sp * sizeof(DESCR_t));
                memcpy(fr->caller_stack, STATE->stack, STATE->sp * sizeof(DESCR_t));
            } else {
                fr->caller_stack = NULL;
            }
            STATE->sp = 0;  /* callee starts with empty stack */
            /* Retval NV slot */
            const char *entry2  = FUNC_ENTRY_fn(name);
            const char *retname = (entry2 && strcmp(entry2, name) != 0
                                   && FNCEX_fn(entry2)) ? entry2 : name;
            fr->retval_name = GC_strdup(retname);
            /* Save + bind params and locals */
            int np  = FUNC_NPARAMS_fn(name);
            int nl2 = FUNC_NLOCALS_fn(name);
            if (np  > 64) np  = 64;
            if (nl2 > 64) nl2 = 64;
            int ns = 0;
            /* Save retval slot */
            if (ns < SM_SAVED_NV_MAX) {
                fr->saved_names[ns] = GC_strdup(retname);
                fr->saved_vals [ns] = NV_GET_fn(retname);
                ns++;
            }
            NV_SET_fn(retname, STRVAL(""));
            /* Save + bind params */
            for (int k = 0; k < np && ns < SM_SAVED_NV_MAX; k++) {
                const char *pname = FUNC_PARAM_fn(name, k);
                if (!pname) pname = "";
                fr->saved_names[ns] = GC_strdup(pname);
                fr->saved_vals [ns] = NV_GET_fn(pname);
                ns++;
                NV_SET_fn(pname, k < nargs ? args[k] : NULVCL);
            }
            /* Save + clear locals */
            for (int k = 0; k < nl2 && ns < SM_SAVED_NV_MAX; k++) {
                const char *lname = FUNC_LOCAL_fn(name, k);
                if (!lname) lname = "";
                fr->saved_names[ns] = GC_strdup(lname);
                fr->saved_vals [ns] = NV_GET_fn(lname);
                ns++;
                NV_SET_fn(lname, NULVCL);
            }
            fr->nsaved = ns;
            comm_call(retname);
            STATE->pc = body_pc;  /* jump into body; dispatch loop continues */
            return;               /* do NOT fall through to INVOKE_fn */
        }
        /* No SM body — fall through to INVOKE_fn (builtins + IR hook) */
        if (result.v == DT_FAIL || (!_data_first && !_data_set))
            result = INVOKE_fn(name, args, nargs);
    }
    if (IS_NAMEPTR(result))      result = NAME_DEREF_PTR(result);
    else if (IS_NAMEVAL(result)) result = NV_GET_fn(result.s);
    PUSH(result);
    STATE->last_ok = (result.v != DT_FAIL);
}

static void h_incr(void) { DESCR_t v = POP(); PUSH(INTVAL(v.i + CUR_INS->a[0].i)); }
static void h_decr(void) { DESCR_t v = POP(); PUSH(INTVAL(v.i - CUR_INS->a[0].i)); }

/* CHUNKS-step14: SM_SUSPEND / SM_RESUME codegen stubs.
 * Full JIT support for generators is M5 territory (Step 19 / EM-10+).
 * For now, abort with a named FATAL so that any attempt to JIT-compile a
 * generator expression is caught loudly rather than silently miscompiling. */
static void h_suspend(void) {
    fprintf(stderr, "sm_codegen FATAL: SM_SUSPEND reached in jit-run — "
            "generator JIT not yet implemented (CHUNKS M5/EM-10)\n");
    STATE->last_ok = 0;
}
static void h_resume(void) {
    fprintf(stderr, "sm_codegen FATAL: SM_RESUME reached in jit-run — "
            "generator JIT not yet implemented (CHUNKS M5/EM-10)\n");
    STATE->last_ok = 0;
}

/* CHUNKS-step14b: SM_LOAD_GLOCAL / SM_STORE_GLOCAL JIT mirrors.
 * Identical semantics to the sm_interp.c handlers — see those for the
 * exhaustive comment.  Like SM_BB_PUMP_SM, these execute against the
 * shared g_current_gen_state owned by bb_broker_drive_sm; nothing
 * JIT-specific. */
static void h_load_glocal(void)
{
    int slot = (int)CUR_INS->a[0].i;
    if (g_current_gen_state && slot >= 0 && slot < SM_GEN_LOCAL_MAX) {
        PUSH(g_current_gen_state->locals[slot]);
        STATE->last_ok = 1;
    } else {
        PUSH(FAILDESCR);
        STATE->last_ok = 0;
    }
}

static void h_store_glocal(void)
{
    int slot = (int)CUR_INS->a[0].i;
    DESCR_t v = POP();
    if (g_current_gen_state && slot >= 0 && slot < SM_GEN_LOCAL_MAX) {
        g_current_gen_state->locals[slot] = v;
        PUSH(v);
        STATE->last_ok = 1;
    } else {
        PUSH(FAILDESCR);
        STATE->last_ok = 0;
    }
}

/* CHUNKS-step15a: SM_ICMP_GT — named FATAL stub; JIT codegen is M5 territory. */
static void h_icmp_gt(void)
{
    fprintf(stderr, "FATAL: SM_ICMP_GT reached in JIT codegen — M5 not yet implemented\n");
    STATE->last_ok = 0;
}

/* CHUNKS-step15a: SM_ICMP_LT — named FATAL stub; JIT codegen is M5 territory. */
static void h_icmp_lt(void)
{
    fprintf(stderr, "FATAL: SM_ICMP_LT reached in JIT codegen — M5 not yet implemented\n");
    STATE->last_ok = 0;
}

/* CHUNKS-step17b'' (CH-17b''): SM_LOAD_FRAME — named FATAL stub.
 * JIT codegen for frame-slot ops is M5 territory; until then, the JIT path
 * emits this stub which prints a clear FATAL.  Today it is unreachable
 * because expressions (the only emit site for SM_LOAD_FRAME) are dead code:
 * forward-jumped over by SM_JUMP, never executed.  CH-17c will flip the
 * consumer to dispatch via entry_pc — at which point this opcode becomes
 * live in --sm-run / --jit-run via the SM dispatch loop only; JIT-emit
 * (mode 4, --jit-emit --x64) extends the stub to real native codegen later. */
static void h_load_frame(void)
{
    fprintf(stderr, "FATAL: SM_LOAD_FRAME reached in JIT codegen — M5 not yet implemented\n");
    STATE->last_ok = 0;
}

/* CHUNKS-step17b'' (CH-17b''): SM_STORE_FRAME — named FATAL stub (mirror of LOAD). */
static void h_store_frame(void)
{
    fprintf(stderr, "FATAL: SM_STORE_FRAME reached in JIT codegen — M5 not yet implemented\n");
    STATE->last_ok = 0;
}

/* Unimplemented stubs — emit warning, set last_ok=0 */
static void h_unimpl(void)
{
    fprintf(stderr, "sm_codegen: unimplemented opcode %d (%s) at sm-pc=%d\n",
            (int)CUR_INS->op, sm_opcode_name(CUR_INS->op), STATE->pc - 1);
    STATE->last_ok = 0;
}

/* ── Handler dispatch table ──────────────────────────────────────────── */

typedef void (*handler_fn_t)(void);

static handler_fn_t g_handlers[SM_OPCODE_COUNT];

static void init_handler_table(void)
{
    for (int i = 0; i < SM_OPCODE_COUNT; i++) g_handlers[i] = h_unimpl;

    g_handlers[SM_LABEL]      = h_label;
    g_handlers[SM_JUMP]       = h_jump;
    g_handlers[SM_JUMP_S]     = h_jump_s;
    g_handlers[SM_JUMP_F]     = h_jump_f;
    g_handlers[SM_HALT]       = h_halt;
    g_handlers[SM_STNO]       = h_stno;

    g_handlers[SM_PUSH_LIT_S] = h_push_lit_s;
    g_handlers[SM_PUSH_LIT_I] = h_push_lit_i;
    g_handlers[SM_PUSH_LIT_F] = h_push_lit_f;
    g_handlers[SM_PUSH_NULL]  = h_push_null;
    g_handlers[SM_PUSH_VAR]   = h_push_var;
    g_handlers[SM_PUSH_EXPR]  = h_push_expr;
    g_handlers[SM_PUSH_EXPRESSION] = h_push_chunk;  /* CHUNKS-step01 stub */
    g_handlers[SM_CALL_EXPRESSION] = h_call_chunk;  /* CHUNKS-step01 stub */
    g_handlers[SM_STORE_VAR]  = h_store_var;
    g_handlers[SM_VOID_POP]        = h_pop;

    g_handlers[SM_ADD]        = h_arith;
    g_handlers[SM_SUB]        = h_arith;
    g_handlers[SM_MUL]        = h_arith;
    g_handlers[SM_DIV]        = h_arith;
    g_handlers[SM_MOD]        = h_arith;   /* OC-1 RS-6 */
    g_handlers[SM_EXP]        = h_arith;
    g_handlers[SM_CONCAT]     = h_concat;
    g_handlers[SM_COERCE_NUM] = h_coerce_num;
    g_handlers[SM_NEG]        = h_neg;

    g_handlers[SM_PAT_LIT]     = h_pat_lit;
    g_handlers[SM_PAT_ANY]     = h_pat_any;
    g_handlers[SM_PAT_NOTANY]  = h_pat_notany;
    g_handlers[SM_PAT_SPAN]    = h_pat_span;
    g_handlers[SM_PAT_BREAK]   = h_pat_break;
    g_handlers[SM_PAT_LEN]     = h_pat_len;
    g_handlers[SM_PAT_POS]     = h_pat_pos;
    g_handlers[SM_PAT_RPOS]    = h_pat_rpos;
    g_handlers[SM_PAT_TAB]     = h_pat_tab;
    g_handlers[SM_PAT_RTAB]    = h_pat_rtab;
    g_handlers[SM_PAT_ARB]     = h_pat_arb;
    g_handlers[SM_PAT_ARBNO]   = h_pat_arbno;
    g_handlers[SM_PAT_REM]     = h_pat_rem;
    g_handlers[SM_PAT_BAL]     = h_pat_bal;
    g_handlers[SM_PAT_FENCE]   = h_pat_fence;
    g_handlers[SM_PAT_FENCE1]  = h_pat_fence1;
    g_handlers[SM_PAT_ABORT]   = h_pat_abort;
    g_handlers[SM_PAT_FAIL]    = h_pat_fail;
    g_handlers[SM_PAT_SUCCEED] = h_pat_succeed;
    g_handlers[SM_PAT_EPS]     = h_pat_eps;
    g_handlers[SM_PAT_ALT]     = h_pat_alt;
    g_handlers[SM_PAT_CAT]     = h_pat_cat;
    g_handlers[SM_PAT_DEREF]   = h_pat_deref;
    g_handlers[SM_PAT_REFNAME] = h_pat_refname;
    g_handlers[SM_PAT_CAPTURE]    = h_pat_capture;
    g_handlers[SM_PAT_CAPTURE_FN] = h_pat_capture_fn;
    g_handlers[SM_PAT_CAPTURE_FN_ARGS] = h_pat_capture_fn_args;
    g_handlers[SM_PAT_USERCALL]   = h_pat_usercall;
    g_handlers[SM_PAT_USERCALL_ARGS] = h_pat_usercall_args;
    /* SM_PAT_BOXVAL handler removed by ME-1 */

    g_handlers[SM_EXEC_STMT]   = h_exec_stmt;
    g_handlers[SM_CALL_FN]        = h_call;
    g_handlers[SM_RETURN]      = h_return;
    g_handlers[SM_FRETURN]     = h_freturn;
    g_handlers[SM_NRETURN]     = h_nreturn;
    g_handlers[SM_RETURN_S]    = h_return_s;
    g_handlers[SM_RETURN_F]    = h_return_f;
    g_handlers[SM_FRETURN_S]   = h_freturn_s;
    g_handlers[SM_FRETURN_F]   = h_freturn_f;
    g_handlers[SM_NRETURN_S]   = h_nreturn_s;
    g_handlers[SM_NRETURN_F]   = h_nreturn_f;
    g_handlers[SM_DEFINE]      = h_define;
    g_handlers[SM_INCR]        = h_incr;
    g_handlers[SM_DECR]        = h_decr;

    /* SN-9b: BB broker — Icon (PUMP) and Prolog (ONCE) generator dispatch. */
    g_handlers[SM_BB_PUMP]      = h_bb_pump;
    g_handlers[SM_BB_ONCE]      = h_bb_once;
    /* CH-17f: Prolog name-driven BB_ONCE dispatch — replaces the legacy
     * lower_expr(AST_CHOICE) + SM_BB_ONCE wrapper. */
    g_handlers[SM_BB_ONCE_PROC] = h_bb_once_proc;
    /* CHUNKS-step12: name-driven Icon proc BB pump — replaces the synthesised
     * AST_FNC + SM_PUSH_EXPR + SM_BB_PUMP wrapper for top-level call_main. */
    g_handlers[SM_BB_PUMP_PROC] = h_bb_pump_proc;
    g_handlers[SM_BB_PUMP_CASE] = h_bb_pump_case;
    g_handlers[SM_BB_PUMP_SM]   = h_bb_pump_sm;
    g_handlers[SM_BB_PUMP_EVERY] = h_bb_pump_every;
    g_handlers[SM_BB_PUMP_AST]   = h_bb_pump_ast;   /* GOAL-ICON-BB-COMPLETE Phase A */
    g_handlers[SM_SUSPEND_VALUE] = h_suspend_value;   /* CHUNKS-step17i-suspend */
    g_handlers[SM_SUSPEND]      = h_suspend;   /* CHUNKS-step14: named FATAL — JIT gen is M5 */
    g_handlers[SM_RESUME]       = h_resume;    /* CHUNKS-step14: named FATAL — JIT gen is M5 */
    g_handlers[SM_LOAD_GLOCAL]  = h_load_glocal;   /* CHUNKS-step14b */
    g_handlers[SM_STORE_GLOCAL] = h_store_glocal;  /* CHUNKS-step14b */
    g_handlers[SM_ICMP_GT]      = h_icmp_gt;       /* CHUNKS-step15a: named FATAL — JIT gen is M5 */
    g_handlers[SM_ICMP_LT]      = h_icmp_lt;       /* CHUNKS-step15a: named FATAL — JIT gen is M5 */
    g_handlers[SM_LOAD_FRAME]   = h_load_frame;    /* CHUNKS-step17b'': named FATAL — JIT gen is M5 */
    g_handlers[SM_STORE_FRAME]  = h_store_frame;   /* CHUNKS-step17b'': named FATAL — JIT gen is M5 */
    /* Opcodes still stubbed as h_unimpl — by design, not by omission:
     *   SM_JUMP_INDIR     — computed gotos `:($expr)`.  sm_lower emits
     *     this from AST_COMPUTED_GOTO, but the SNOBOL4 parser currently
     *     treats computed gotos as undefined labels (Error 24) in all
     *     three modes.  Not a JIT-specific gap; cross-mode issue tracked
     *     outside SN-9.
     *   SM_TRIM, SM_SPCINT, SM_SPREAL, SM_SELBRA, SM_STATE_PUSH,
     *   SM_STATE_POP, SM_RCOMP — never emitted by current sm_lower.
     *
     * SM_ACOMP / SM_LCOMP — handlers landed sess 2026-05-09
     * (CH-17g-runtime-bridge-acomp / -lcomp); sm_interp.c has them; JIT
     * codegen is M5 territory (named FATAL pattern).
     */
}

/* ── ME-3: per-instruction native blob emitter ───────────────────────── */
/*
 * GOAL-MODE3-EMIT ME-3 (2026-05-10): SEG_CODE now holds per-instruction
 * native x86 blobs, not a uint8_t** pointer array.  Each blob is a fixed
 * 22-byte sequence; the program runs by transferring control from one
 * blob to the next via a small native trampoline.
 *
 * Blob layout (30 bytes uniformly, NOP-padded as needed):
 *
 *   Standard (covers most opcodes, including SM_PUSH_LIT_S/I and SM_VOID_POP):
 *     41 ff 44 24 14        inc  dword [r12+20]       ; st->pc++
 *     48 b8 <imm64>         mov  rax, handler_addr
 *     48 83 ec 08           sub  rsp, 8               ; 16-byte align for call
 *     ff d0                 call rax
 *     48 83 c4 08           add  rsp, 8               ; restore rsp
 *     e9 <rel32>            jmp  trampoline
 *
 *   SM_HALT (returns out of sm_jit_run via ret):
 *     41 ff 44 24 14        inc  dword [r12+20]       ; advance past HALT
 *     c3                    ret                       ; return to sm_jit_run
 *     24 × 0x90             nop padding
 *
 *   SM_JUMP (sets pc to target, jumps directly to target blob):
 *     41 c7 44 24 14 <i32>  mov  dword [r12+20], target_pc
 *     e9 <rel32>            jmp  blob_for_target_pc   ; patched in pass 2
 *     16 × 0x90             nop padding
 *
 *   The trampoline is emitted ONCE at the head of SEG_CODE, before any
 *   instruction blob.  It reads pc from [r12+20] and indirect-jumps to
 *   blob[pc] via arithmetic on the fixed blob size:
 *
 *     trampoline:
 *       8b 44 24 14         mov   eax, dword [r12+20]    ; eax = pc
 *       3d <imm32>          cmp   eax, prog->count       ; bound check
 *       73 01               jae   .exit
 *       48 6b c0 16         imul  rax, rax, 22           ; rax = pc*22
 *       48 b9 <imm64>       mov   rcx, blobs_base
 *       48 01 c8            add   rax, rcx
 *       ff e0               jmp   rax
 *     .exit:
 *       c3                  ret
 *
 *   sm_jit_run's job becomes: set r12, jump to trampoline, run until any
 *   blob's `ret` returns.  The threaded-call C dispatch loop is gone.
 *
 * Per-PC side-table (g_blob_offsets[i] = byte offset of blob[i] from
 * SEG_CODE base; g_blob_count = total) is built as bytes are laid down.
 * Today used for SM_JUMP rel32 patching in pass 2.  ME-mode-4 will reuse
 * it to map each SM PC back to a SEG_CODE address range for disassembly
 * emission.
 *
 * Opcodes outside the ME-3 "covered" set (SM_HALT, SM_PUSH_LIT_S/I,
 * SM_VOID_POP, SM_JUMP) get the Standard blob — same per-instruction
 * native-blob shape, but the work is performed by calling the existing
 * C handler.  ME-4+ will replace those Standard blobs with inline native
 * sequences for the opcode set each rung names.
 */

/* GOAL-MODE3-EMIT ME-4: variable-size blobs ────────────────────────────
 *
 * ME-3 used a fixed 30-byte blob stride so the trampoline could compute
 * blob[pc] via `imul rax, rax, 30`.  ME-4's inline-native opcodes require
 * larger blobs for arithmetic / string ops; rather than bump the uniform
 * stride and waste bytes on NOP padding, we switch to a per-PC dispatch
 * table.  Each `SM_Instr` blob is exactly as big as it needs to be.
 *
 * Table: g_blob_addrs[] — uint8_t*[prog->count], filled in pass 1 as
 * blobs are laid down.  Each entry is the absolute address of the first
 * byte of the blob for that pc.  Stable for the lifetime of the program
 * (SEG_CODE is mmap-fixed; the C array is heap-stable until next codegen).
 *
 * Trampoline: read pc, bound-check, indirect-jmp through
 * g_blob_addrs[pc].  One memory load instead of an imul + add; same
 * dispatch latency in practice.
 *
 * Mode-4 future: g_blob_addrs[] becomes the per-pc label list in the
 * serialized .s — `mov rax, [.Lblob_addrs + pc*8] ; jmp rax`, with
 * `.Lblob_addrs` a `.quad` table of the assembler-emitted blob labels.
 */

/* Per-PC dispatch table built during sm_codegen.
 * g_blob_addrs[i] = absolute address of blob[i] in SEG_CODE. */
static uint8_t **g_blob_addrs = NULL;
static int       g_blob_count = 0;
static size_t    g_trampoline_offset = 0;  /* byte offset of trampoline in SEG_CODE */

/*
 * emit_trampoline — emit the shared dispatch trampoline at the current
 * SEG_CODE write head.  Returns its byte offset from SEG_CODE base.
 *
 * Variable-size dispatch (ME-4): looks up g_blob_addrs[pc] (absolute
 * address) and indirect-jumps.  The C array's base is baked as imm64;
 * it's stable until the next sm_codegen call.
 */
static size_t emit_trampoline(SM_Program *prog)
{
    size_t off = seg_offset(SEG_CODE);

    /* mov eax, dword [r12+20]    (41 8b 44 24 14)  5 bytes — load st->pc */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x14);

    /* cmp eax, prog->count       (3d <imm32>)      5 bytes */
    seg_byte(SEG_CODE, 0x3d);
    seg_u32(SEG_CODE, (uint32_t)prog->count);

    /* jae .exit (rel8 = +16, skipping mov/mov/jmp):
     *   mov rcx, &g_blob_addrs[0]   48 b9 <imm64>   10 bytes
     *   mov rax, [rcx + rax*8]      48 8b 04 c1      4 bytes
     *   jmp rax                     ff e0            2 bytes
     * Total skip = 16 bytes; rel8 = 0x10.
     */
    seg_byte(SEG_CODE, 0x73); seg_byte(SEG_CODE, 0x10);

    /* mov rcx, &g_blob_addrs[0]  (48 b9 <imm64>) — baked C array base */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xb9);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)g_blob_addrs);

    /* mov rax, qword [rcx + rax*8]  (48 8b 04 c1) — rax = g_blob_addrs[pc] */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0xc1);

    /* jmp rax  (ff e0)  2 bytes */
    seg_byte(SEG_CODE, 0xff); seg_byte(SEG_CODE, 0xe0);

    /* .exit: ret  (c3) — reached only if pc out-of-bounds, shouldn't happen
     * on well-formed programs (SM_HALT terminates via its own blob ret). */
    seg_byte(SEG_CODE, 0xc3);

    return off;
}

/*
 * emit_standard_blob — emit the standard "call C handler" blob.
 *
 * Variable-size (ME-4): exactly 28 bytes — no NOP padding.
 *
 * 16-byte stack alignment discipline.  The System V x86-64 ABI requires
 * rsp ≡ 0 (mod 16) at the point of `call`, so the callee sees rsp ≡ 8
 * on function entry.  Control reaches each blob via `jmp` from the
 * trampoline (no rsp change) or from another blob's `jmp` — but the
 * very first transfer into SEG_CODE is sm_jit_run's `call entry()` which
 * leaves rsp ≡ 8 (mod 16).  Inside the blob, a naive `call rax` would
 * misalign rsp further (≡ 0 on callee entry from the blob's perspective,
 * which means ≡ 8 inside the callee — wrong).  Fix: `sub rsp, 8` before
 * `call rax`, restoring `rsp ≡ 0` so the callee enters with the canonical
 * `rsp ≡ 8`.  `add rsp, 8` restores after.  Without this padding, any
 * handler that itself issues a C-ABI call into a vararg or vsnprintf-
 * descended path (h_store_var → NV_SET → printf, h_arith → shared_arith)
 * faults on the first 16-byte aligned movaps in the standard library.
 */
static void emit_standard_blob(handler_fn_t fn, size_t trampoline_abs_off)
{
    /* inc dword [r12+20]  (41 ff 44 24 14)  5 bytes — pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x14);

    /* mov rax, handler_imm64  (48 b8 <imm64>)  10 bytes */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xb8);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)fn);

    /* sub rsp, 8  (48 83 ec 08)  4 bytes — 16-byte alignment for `call` */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xec); seg_byte(SEG_CODE, 0x08);

    /* call rax  (ff d0)  2 bytes */
    seg_byte(SEG_CODE, 0xff); seg_byte(SEG_CODE, 0xd0);

    /* add rsp, 8  (48 83 c4 08)  4 bytes — undo alignment padding */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xc4); seg_byte(SEG_CODE, 0x08);

    /* jmp rel32 trampoline.  rel32 is relative to the byte AFTER the
     * rel32 (i.e. the current seg head + 5).  3 bytes opcode + 5 = total 28. */
    size_t rel32_end_off = seg_offset(SEG_CODE) + 5;
    int32_t rel = (int32_t)((int64_t)trampoline_abs_off - (int64_t)rel32_end_off);
    seg_byte(SEG_CODE, 0xe9);
    seg_u32(SEG_CODE, (uint32_t)rel);
}

/*
 * emit_halt_blob — emit the SM_HALT blob: advance pc past HALT,
 * then ret (returns out of sm_jit_run via sm_jit_run's `call entry()`).
 * Variable-size: exactly 6 bytes.
 */
static void emit_halt_blob(void)
{
    /* inc dword [r12+20]  (41 ff 44 24 14)  5 bytes */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x14);

    /* ret  (c3)  1 byte — returns to sm_jit_run's `entry();` call site */
    seg_byte(SEG_CODE, 0xc3);
}

/*
 * emit_jump_blob_skeleton — emit the SM_JUMP blob with the jmp rel32
 * displacement left as 0; pass 2 patches it with the real displacement
 * to blob[target_pc].  Returns the byte offset within SEG_CODE of the
 * rel32 field (used by the patcher).  Variable-size: exactly 14 bytes.
 */
static size_t emit_jump_blob_skeleton(int32_t target_pc)
{
    /* mov dword [r12+20], target_pc  (41 c7 44 24 14 <imm32>)  9 bytes */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xc7);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x14);
    seg_u32(SEG_CODE, (uint32_t)target_pc);

    /* jmp rel32 <placeholder> (e9 <rel32>) 5 bytes — patched in pass 2 */
    seg_byte(SEG_CODE, 0xe9);
    size_t rel32_off = seg_offset(SEG_CODE);
    seg_u32(SEG_CODE, 0);

    return rel32_off;
}

/* ── ME-4 inline-native blob emitters ────────────────────────────────────
 *
 * Each emit_me4_* helper lowers one SM opcode to a self-contained native
 * x86 sequence that:
 *   - increments pc via [r12+20]
 *   - pops its operands from the value stack ([r12]-anchored)
 *   - calls the matching me4_* helper via imm64 (SEG_CODE↔scrip-text
 *     distance exceeds rel32 reach in mmap-allocated SEG_CODE; see
 *     ME-3 design notes)
 *   - pushes the result back onto the value stack
 *   - tail-jumps to the trampoline via rel32 to dispatch the next opcode
 *
 * Stack base reload after the C call is defensive — me4_arith /
 * me4_coerce_num / me4_push_null don't push and so cannot reallocate the
 * stack; me4_concat and me4_push_var may allocate strings but do not push;
 * me4_store_var calls NV_SET_fn which may grow the NV table but does not
 * touch SM_State.stack.  Even so, reloading [r12] after the call is
 * cheap (one mov) and removes a fragility class.
 */

/* Helper: 16-byte rsp alignment + call rax via imm64.  16 bytes. */
static void emit_aligned_call_imm64(void *fn)
{
    /* sub rsp, 8  (48 83 ec 08)  4 bytes */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xec); seg_byte(SEG_CODE, 0x08);
    /* mov rax, imm64  (48 b8 <imm64>)  10 bytes */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xb8);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)fn);
    /* call rax  (ff d0)  2 bytes */
    seg_byte(SEG_CODE, 0xff); seg_byte(SEG_CODE, 0xd0);
    /* add rsp, 8  (48 83 c4 08)  4 bytes */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xc4); seg_byte(SEG_CODE, 0x08);
}
/* Note: the above is 4+10+2+4 = 20 bytes (not 16 — comment was wrong). */

/* Helper: jmp rel32 to trampoline.  5 bytes. */
static void emit_jmp_trampoline(size_t trampoline_abs_off)
{
    size_t rel32_end_off = seg_offset(SEG_CODE) + 5;
    int32_t rel = (int32_t)((int64_t)trampoline_abs_off - (int64_t)rel32_end_off);
    seg_byte(SEG_CODE, 0xe9);
    seg_u32(SEG_CODE, (uint32_t)rel);
}

/* emit_me4_arith_blob — inline-native lowering of SM_ADD/SUB/MUL/DIV/MOD.
 *
 * Sequence:  pops two DESCR_t (l, r) off the value stack, calls
 *   me4_arith(l, r, op) via imm64, pushes returned DESCR_t back.
 *
 * Reg use within the blob:
 *   r12        — SM_State*, never modified
 *   r8         — stack base (loaded twice: pre-call, post-call)
 *   rax, rcx   — scratch + sp arithmetic
 *   rdi, rsi   — l (DESCR_t arg 1, low / high)
 *   rdx, rcx   — r (DESCR_t arg 2, low / high)
 *   r8d        — op (3rd integer arg; reuses r8 because base is reloaded
 *                after the call)
 *
 * Blob size: 84 bytes (pre-call setup 23 + arg loads 19 + op imm 6 +
 * aligned_call 20 + post-call store 11 + jmp_trampoline 5).
 */
static void emit_me4_arith_blob(sm_opcode_t op, size_t trampoline_abs_off)
{
    extern DESCR_t me4_arith(DESCR_t, DESCR_t, sm_opcode_t);

    /* ── pre-call: load operands, set up regs ────────────────────────── */
    /* inc dword [r12+20]                  (41 ff 44 24 14)  5 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x14);
    /* mov r8, [r12]                       (4d 8b 04 24)     4 */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);
    /* mov eax, [r12+8]                    (41 8b 44 24 08)  5 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);
    /* lea ecx, [rax - 1]                  (8d 48 ff)        3 — new sp = old sp - 1 */
    seg_byte(SEG_CODE, 0x8d); seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xff);
    /* mov [r12+8], ecx                    (41 89 4c 24 08)  5 — write new sp */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);
    /* sub eax, 2                          (83 e8 02)        3 — index of l = old sp - 2 */
    seg_byte(SEG_CODE, 0x83); seg_byte(SEG_CODE, 0xe8); seg_byte(SEG_CODE, 0x02);
    /* shl rax, 4                          (48 c1 e0 04)     4 — byte offset */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe0); seg_byte(SEG_CODE, 0x04);
    /* mov rdi, [r8 + rax]                 (49 8b 3c 00)     4 — l low */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x3c); seg_byte(SEG_CODE, 0x00);
    /* mov rsi, [r8 + rax + 8]             (49 8b 74 00 08)  5 — l high */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x00); seg_byte(SEG_CODE, 0x08);
    /* mov rdx, [r8 + rax + 16]            (49 8b 54 00 10)  5 — r low */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x00); seg_byte(SEG_CODE, 0x10);
    /* mov rcx, [r8 + rax + 24]            (49 8b 4c 00 18)  5 — r high */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x00); seg_byte(SEG_CODE, 0x18);
    /* mov r8d, <op_imm32>                 (41 b8 <imm32>)   6 — 3rd arg = op */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xb8);
    seg_u32(SEG_CODE, (uint32_t)op);

    /* ── call me4_arith ──────────────────────────────────────────────── */
    emit_aligned_call_imm64((void *)&me4_arith);   /* 20 bytes */

    /* ── post-call: store result into stack[new_sp - 1] (= l's old slot) */
    /* Save rax in r9 (we need to compute slot addr; rax is also needed) */
    /* mov r9, rax                         (49 89 c1)        3 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xc1);
    /* mov r8, [r12]                       (4d 8b 04 24)     4 — reload base (defensive) */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);
    /* mov ecx, [r12+8]                    (41 8b 4c 24 08)  5 — ecx = new sp */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);
    /* sub ecx, 1                          (83 e9 01)        3 — slot index = new sp - 1 */
    seg_byte(SEG_CODE, 0x83); seg_byte(SEG_CODE, 0xe9); seg_byte(SEG_CODE, 0x01);
    /* shl rcx, 4                          (48 c1 e1 04)     4 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe1); seg_byte(SEG_CODE, 0x04);
    /* mov [r8 + rcx], r9                  (4d 89 0c 08)     4 — result low */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x0c); seg_byte(SEG_CODE, 0x08);
    /* mov [r8 + rcx + 8], rdx             (49 89 54 08 08)  5 — result high */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x08); seg_byte(SEG_CODE, 0x08);

    /* ── tail to trampoline ──────────────────────────────────────────── */
    emit_jmp_trampoline(trampoline_abs_off);   /* 5 bytes */
}

/* emit_me4_concat_blob — inline-native lowering of SM_CONCAT.
 * Same shape as arith but no `op` argument; calls me4_concat(l, r). */
static void emit_me4_concat_blob(size_t trampoline_abs_off)
{
    extern DESCR_t me4_concat(DESCR_t, DESCR_t);

    /* Pre-call (same as arith, minus the op imm32 — see emit_me4_arith_blob): */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x14);   /* inc [r12+20] */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);                              /* mov r8, [r12] */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* mov eax, [r12+8] */
    seg_byte(SEG_CODE, 0x8d); seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xff);   /* lea ecx, [rax-1] */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* mov [r12+8], ecx */
    seg_byte(SEG_CODE, 0x83); seg_byte(SEG_CODE, 0xe8); seg_byte(SEG_CODE, 0x02);   /* sub eax, 2 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe0); seg_byte(SEG_CODE, 0x04);                              /* shl rax, 4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x3c); seg_byte(SEG_CODE, 0x00);                              /* mov rdi, [r8+rax] */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x00); seg_byte(SEG_CODE, 0x08);   /* mov rsi, [r8+rax+8] */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x00); seg_byte(SEG_CODE, 0x10);   /* mov rdx, [r8+rax+16] */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x00); seg_byte(SEG_CODE, 0x18);   /* mov rcx, [r8+rax+24] */

    emit_aligned_call_imm64((void *)&me4_concat);

    /* Post-call store: same as arith */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xc1);   /* mov r9, rax */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);                              /* mov r8, [r12] */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* mov ecx, [r12+8] */
    seg_byte(SEG_CODE, 0x83); seg_byte(SEG_CODE, 0xe9); seg_byte(SEG_CODE, 0x01);   /* sub ecx, 1 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe1); seg_byte(SEG_CODE, 0x04);                              /* shl rcx, 4 */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x0c); seg_byte(SEG_CODE, 0x08);                              /* mov [r8+rcx], r9 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x08); seg_byte(SEG_CODE, 0x08);   /* mov [r8+rcx+8], rdx */

    emit_jmp_trampoline(trampoline_abs_off);
}

/* emit_me4_coerce_num_blob — inline-native lowering of SM_COERCE_NUM.
 * Pops 1, calls me4_coerce_num(v), pushes 1.  Net stack delta: 0. */
static void emit_me4_coerce_num_blob(size_t trampoline_abs_off)
{
    extern DESCR_t me4_coerce_num(DESCR_t);

    /* pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x14);
    /* Load TOS into rdi:rsi.  Stack index = sp - 1. */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);                              /* mov r8, [r12] */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* mov eax, [r12+8] */
    seg_byte(SEG_CODE, 0x83); seg_byte(SEG_CODE, 0xe8); seg_byte(SEG_CODE, 0x01);   /* sub eax, 1 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe0); seg_byte(SEG_CODE, 0x04);                              /* shl rax, 4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x3c); seg_byte(SEG_CODE, 0x00);                              /* mov rdi, [r8+rax] */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x00); seg_byte(SEG_CODE, 0x08);   /* mov rsi, [r8+rax+8] */

    emit_aligned_call_imm64((void *)&me4_coerce_num);

    /* Post-call: write result back to the SAME slot (TOS unchanged depth).
     * Defensive base reload. */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xc1);   /* mov r9, rax */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);                              /* mov r8, [r12] */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* mov ecx, [r12+8] */
    seg_byte(SEG_CODE, 0x83); seg_byte(SEG_CODE, 0xe9); seg_byte(SEG_CODE, 0x01);   /* sub ecx, 1 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe1); seg_byte(SEG_CODE, 0x04);                              /* shl rcx, 4 */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x0c); seg_byte(SEG_CODE, 0x08);                              /* mov [r8+rcx], r9 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x08); seg_byte(SEG_CODE, 0x08);   /* mov [r8+rcx+8], rdx */

    emit_jmp_trampoline(trampoline_abs_off);
}

/* emit_me4_push_null_blob — inline-native lowering of SM_PUSH_NULL.
 * Calls me4_push_null() (no args), pushes returned NULVCL. */
static void emit_me4_push_null_blob(size_t trampoline_abs_off)
{
    extern DESCR_t me4_push_null(void);

    /* pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x14);

    emit_aligned_call_imm64((void *)&me4_push_null);

    /* Push result onto stack[sp], sp++. */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xc1);   /* mov r9, rax */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);                              /* mov r8, [r12] */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* mov ecx, [r12+8] */
    /* For push: slot is at offset (sp)*16; then sp++. */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe1); seg_byte(SEG_CODE, 0x04);                              /* shl rcx, 4 */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x0c); seg_byte(SEG_CODE, 0x08);                              /* mov [r8+rcx], r9 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x08); seg_byte(SEG_CODE, 0x08);   /* mov [r8+rcx+8], rdx */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* inc dword [r12+8] */

    emit_jmp_trampoline(trampoline_abs_off);
}

/* emit_me4_push_var_blob — inline-native lowering of SM_PUSH_VAR.
 * Calls me4_push_var(name) where name is baked from CUR_INS->a[0].s.
 * Pushes the loaded DESCR_t. */
static void emit_me4_push_var_blob(const char *name, size_t trampoline_abs_off)
{
    extern DESCR_t me4_push_var(const char *);

    /* pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x14);

    /* mov rdi, imm64(name)  (48 bf <imm64>)  10 bytes */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xbf);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)name);

    emit_aligned_call_imm64((void *)&me4_push_var);

    /* Push result onto stack[sp], sp++. */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xc1);   /* mov r9, rax */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);                              /* mov r8, [r12] */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* mov ecx, [r12+8] */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe1); seg_byte(SEG_CODE, 0x04);                              /* shl rcx, 4 */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x0c); seg_byte(SEG_CODE, 0x08);                              /* mov [r8+rcx], r9 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x08); seg_byte(SEG_CODE, 0x08);   /* mov [r8+rcx+8], rdx */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* inc dword [r12+8] */

    emit_jmp_trampoline(trampoline_abs_off);
}

/* emit_me4_store_var_blob — inline-native lowering of SM_STORE_VAR.
 * Pops val (TOS), calls me4_store_var(name, val), pushes the (un-modified)
 * val back onto the stack (mirrors h_store_var: assignment expression's
 * value is val, not NV_SET_fn's return). */
static void emit_me4_store_var_blob(const char *name, size_t trampoline_abs_off)
{
    extern DESCR_t me4_store_var(const char *, DESCR_t);

    /* pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x14);

    /* Pop val into rsi:rdx (2nd DESCR_t arg).  Stack index = sp - 1.
     * We update sp BEFORE the call (decrement by 1) since this is a pop. */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);                              /* mov r8, [r12] */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* mov eax, [r12+8] */
    seg_byte(SEG_CODE, 0x83); seg_byte(SEG_CODE, 0xe8); seg_byte(SEG_CODE, 0x01);   /* sub eax, 1 (new sp) */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* mov [r12+8], eax */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe0); seg_byte(SEG_CODE, 0x04);                              /* shl rax, 4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x34); seg_byte(SEG_CODE, 0x00);                              /* mov rsi, [r8+rax] — val low */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x00); seg_byte(SEG_CODE, 0x08);   /* mov rdx, [r8+rax+8] — val high */

    /* mov rdi, imm64(name) */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xbf);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)name);

    emit_aligned_call_imm64((void *)&me4_store_var);

    /* Push returned val (= rax:rdx) back: stack[new_sp] = result, sp++. */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xc1);   /* mov r9, rax */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);                              /* mov r8, [r12] */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* mov ecx, [r12+8] */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe1); seg_byte(SEG_CODE, 0x04);                              /* shl rcx, 4 */
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x0c); seg_byte(SEG_CODE, 0x08);                              /* mov [r8+rcx], r9 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x08); seg_byte(SEG_CODE, 0x08);   /* mov [r8+rcx+8], rdx */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);   /* inc dword [r12+8] */

    emit_jmp_trampoline(trampoline_abs_off);
}

/* ── Main codegen entry point ─────────────────────────────────────────── */

/*
 * sm_codegen — compile SM_Program into SEG_CODE per ME-3/ME-4 layout.
 *
 * Pass 1: emit trampoline, then a variable-size native blob per SM instr.
 *         Record blob absolute addresses in g_blob_addrs[i].  Note rel32
 *         patch sites for SM_JUMP instructions.
 * Pass 2: patch each SM_JUMP's `jmp rel32` displacement to point at
 *         blob[target_pc].
 *
 * Trampoline emit ordering: g_blob_addrs is calloc'd BEFORE the trampoline
 * is emitted, so the trampoline can bake `&g_blob_addrs[0]` as imm64.
 * The contents of g_blob_addrs[] are filled in by pass 1 as blobs are
 * laid down.
 *
 * Returns 0 on success, -1 on error.
 */
int sm_codegen(SM_Program *prog)
{
    init_handler_table();

    /* Free any previous side-table (e.g. from earlier sm_codegen call) */
    if (g_blob_addrs) { free(g_blob_addrs); g_blob_addrs = NULL; }
    g_blob_count = 0;
    g_trampoline_offset = 0;

    if (prog->count == 0) {
        /* Empty program: nothing to emit; sm_jit_run handles count==0 as
         * an immediate exit. */
        return 0;
    }

    g_blob_addrs = (uint8_t **)calloc((size_t)prog->count, sizeof(uint8_t *));
    if (!g_blob_addrs) {
        fprintf(stderr, "sm_codegen: g_blob_addrs allocation failed\n");
        return -1;
    }

    /* Emit trampoline — bakes &g_blob_addrs[0] as imm64 internally. */
    g_trampoline_offset = emit_trampoline(prog);

    /* Pass 1 — emit one variable-size blob per instruction; record its
     * absolute address in g_blob_addrs[i].  Remember each SM_JUMP's
     * rel32 patch offset and target_pc. */
    int *jump_indices = NULL;
    size_t *jump_rel32_offs = NULL;
    int    jump_count = 0;
    jump_indices    = (int    *)calloc((size_t)prog->count, sizeof(int));
    jump_rel32_offs = (size_t *)calloc((size_t)prog->count, sizeof(size_t));
    if (!jump_indices || !jump_rel32_offs) {
        fprintf(stderr, "sm_codegen: jump-patch buffers allocation failed\n");
        free(jump_indices); free(jump_rel32_offs);
        free(g_blob_addrs); g_blob_addrs = NULL;
        return -1;
    }

    for (int i = 0; i < prog->count; i++) {
        size_t off_before = seg_offset(SEG_CODE);
        g_blob_addrs[i] = scrip_segs[SEG_CODE].base + off_before;
        sm_opcode_t op = prog->instrs[i].op;

        if (op == SM_HALT) {
            emit_halt_blob();
        } else if (op == SM_JUMP) {
            int32_t target_pc = (int32_t)prog->instrs[i].a[0].i;
            size_t rel32_off = emit_jump_blob_skeleton(target_pc);
            jump_indices[jump_count]    = i;
            jump_rel32_offs[jump_count] = rel32_off;
            jump_count++;
        } else if (op == SM_ADD || op == SM_SUB || op == SM_MUL ||
                   op == SM_DIV || op == SM_MOD) {
            /* ME-4: inline-native arithmetic.  Args loaded from r12-stack,
             * me4_arith called with values in registers, result stored back.
             * No g_handlers[] indirection; no h_arith implicit-stack call. */
            emit_me4_arith_blob(op, g_trampoline_offset);
        } else if (op == SM_CONCAT) {
            emit_me4_concat_blob(g_trampoline_offset);
        } else if (op == SM_COERCE_NUM) {
            emit_me4_coerce_num_blob(g_trampoline_offset);
        } else if (op == SM_PUSH_NULL) {
            emit_me4_push_null_blob(g_trampoline_offset);
        } else if (op == SM_PUSH_VAR) {
            emit_me4_push_var_blob(prog->instrs[i].a[0].s, g_trampoline_offset);
        } else if (op == SM_STORE_VAR) {
            emit_me4_store_var_blob(prog->instrs[i].a[0].s, g_trampoline_offset);
        } else {
            emit_standard_blob(g_handlers[op], g_trampoline_offset);
        }
        /* Variable size — no equality check, just continue.  Each emit_*
         * helper writes its own self-consistent byte count. */
    }
    g_blob_count = prog->count;

    /* Pass 2 — patch SM_JUMP rel32 displacements.  target_blob_addr is
     * g_blob_addrs[target_pc] (absolute address).  The rel32 is relative
     * to the byte AFTER the rel32 in SEG_CODE = base + rel32_off + 4. */
    uint8_t *seg_base = scrip_segs[SEG_CODE].base;
    for (int j = 0; j < jump_count; j++) {
        int    i        = jump_indices[j];
        size_t rel32_off = jump_rel32_offs[j];
        int32_t target_pc = (int32_t)prog->instrs[i].a[0].i;
        if (target_pc < 0 || target_pc >= prog->count) {
            /* Out-of-range target — pass 2 silently leaves rel32=0; this
             * was already broken before ME-3 (the handler would set pc
             * to a bogus value).  Diagnostic message preserves the trail. */
            fprintf(stderr, "sm_codegen: SM_JUMP at pc=%d targets out-of-range pc=%d (count=%d)\n",
                    i, target_pc, prog->count);
            continue;
        }
        int64_t target_abs   = (int64_t)(uintptr_t)g_blob_addrs[target_pc];
        int64_t rel32_end_abs = (int64_t)(uintptr_t)(seg_base + rel32_off + 4);
        int32_t rel = (int32_t)(target_abs - rel32_end_abs);
        seg_patch_u32(SEG_CODE, rel32_off, (uint32_t)rel);
    }

    free(jump_indices); free(jump_rel32_offs);

    /* SEG_DISPATCH no longer used by ME-3.  Sealing an unused empty segment
     * is harmless but skipped to avoid mprotect on a zero-sized region. */
    seg_seal(SEG_CODE);
    return 0;
}

/* ── JIT execution runner ─────────────────────────────────────────────── */

/*
 * sm_jit_run — execute a codegen'd SM_Program per ME-3.
 *
 * Requires sm_codegen() to have been called first on the same prog.
 * Sets r12 = SM_State* (ME-2), then jumps into the trampoline; control
 * runs through the per-instruction blob chain until SM_HALT's `ret`
 * returns here.  No C dispatch loop, no per-opcode thunk.
 */
int sm_jit_run(SM_Program *prog, SM_State *st)
{
    g_jit_prog   = prog;
    g_jit_state  = st;
    g_jit_halted = 0;

    if (prog->count == 0) return 0;

    /* Compute trampoline entry point */
    uint8_t *tramp = scrip_segs[SEG_CODE].base + g_trampoline_offset;
    typedef void (*entry_fn_t)(void);
    entry_fn_t entry = (entry_fn_t)tramp;

    /* ME-2 + ME-3: load r12 = SM_State* before transferring control to
     * SEG_CODE.  r12 is callee-saved per System V ABI, so the surrounding
     * C frame (this function) saves/restores it for us.  Any C handler
     * called from inside a blob also preserves r12 across its return.
     * The trampoline reads st->pc as [r12+20] (offsetof SM_State.pc),
     * looks up blob[pc] by arithmetic on the fixed 22-byte stride, and
     * jmps in.  Each blob increments pc (or sets it for SM_JUMP), runs,
     * jmps back to the trampoline.  SM_HALT's blob `ret`s out — that's
     * how this function returns. */
    asm volatile ("mov %0, %%r12" : : "r"(st) : "r12");
    entry();

    return 0;
}

/* sm_jit_run_plain — debug: pure C dispatch, no SEG_CODE, proves handler correctness */
int sm_jit_run_plain(SM_Program *prog, SM_State *st)
{
    init_handler_table();
    g_jit_prog   = prog;
    g_jit_state  = st;
    g_jit_halted = 0;
    while (st->pc < prog->count && !g_jit_halted) {
        st->pc++;
        g_handlers[prog->instrs[st->pc - 1].op]();
    }
    return 0;
}

/* IM-5: sm_jit_run_steps — run at most n statements then return.
 * Sets up g_jit_step_jmp so the step-limit longjmp lands here safely. */
int sm_jit_run_steps(SM_Program *prog, SM_State *st, int n) {
    g_jit_step_limit = n;
    g_jit_steps_done = 0;
    /* SN-32a-stno: stno is carried as SM_STNO operand by sm_lower; no
     * counter to reset here. */
    int rc = 0;
    if (setjmp(g_jit_step_jmp) == 0)
        rc = sm_jit_run(prog, st);
    g_jit_step_limit = 0;
    g_jit_steps_done = 0;
    return rc;
}
