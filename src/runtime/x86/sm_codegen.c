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
#include "../../frontend/snobol4/scrip_cc.h"  /* tree_t, AST_FNC for SM_PAT_CAPTURE_FN */
#include "bb_broker.h"   /* SN-9b: SM_BB_PUMP / SM_BB_ONCE handlers */
#include "templates/templates.h"  /* EM-MODE4-IS-MODE3-DUMP-c: per-opcode templates */
#include "bb_emit.h"              /* EM-MODE4-IS-MODE3-DUMP-c: capture-and-flush adapter */
#include "emitter.h"              /* EM-MODE4-IS-MODE3-DUMP-c: emitter_t */

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
static void h_define_entry(void) { /* ME-6a no-op in mode-2; mode-3 blob handles jit_in_call */ }
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

/* ME-6: unified return-variant dispatcher with epilogue-pending signal.
 *
 * Replaces the nine separate h_return_* handlers from the SM-blob's point
 * of view.  A single me6_return_dispatch helper is called via an imm64 by
 * each return-variant blob; it:
 *
 *   - decodes the variant from `bits` (low nibble),
 *   - evaluates the conditional gate (cond_s / cond_f / unconditional),
 *   - if the gate holds AND there is a live SM call frame, invokes
 *     h_return_impl(is_fret, is_nret) which pops the frame and sets
 *     STATE->pc to the caller's resume PC,
 *   - sets STATE->jit_epilogue_pending = 1 iff a frame was actually popped,
 *   - returns the value of jit_epilogue_pending so the caller's blob can
 *     conditionally emit `mov rsp, rbp; pop rbp` to undo the SM_LABEL
 *     define_entry prologue.
 *
 * Bits layout (matches sm_codegen.c emit_me6_return_blob's imm32):
 *
 *   bit 0 — is_fret  (FRETURN family)
 *   bit 1 — is_nret  (NRETURN family)
 *   bit 2 — cond_s   (fire only if  STATE->last_ok)
 *   bit 3 — cond_f   (fire only if !STATE->last_ok)
 *
 * The pre-call sync-r12→sp protocol from emit_standard_blob is preserved
 * exactly (h_return_impl reads STATE->stack[STATE->sp - 1] for thunk
 * returns).  Post-call we read STATE->jit_epilogue_pending into eax so the
 * blob's `test eax, eax ; jz no_unwind` decides whether to unwind rbp.
 *
 * Top-level returns (call_depth == 0) set g_jit_halted from h_return_impl
 * and do NOT set jit_epilogue_pending — top-level can't have entered via
 * a define-entry SM_LABEL prologue, so there is nothing to unwind. */
static int me6_return_dispatch(int bits) __attribute__((unused));
static int me6_return_dispatch(int bits)
{
    /* Always clear the signal first — stale 1 from a prior return must
     * not leak past a no-op variant.  Mode-2 (sm_interp.c) never reads
     * this field, so this write is mode-3-only. */
    STATE->jit_epilogue_pending = 0;

    int is_fret = (bits >> 0) & 1;
    int is_nret = (bits >> 1) & 1;
    int cond_s  = (bits >> 2) & 1;
    int cond_f  = (bits >> 3) & 1;

    /* Gate */
    if (cond_s && !STATE->last_ok) return 0;
    if (cond_f &&  STATE->last_ok) return 0;

    /* Snapshot call_depth so we can tell whether h_return_impl actually
     * popped a frame (top-level halts without popping). */
    int depth_before = STATE->call_depth;
    h_return_impl(is_fret, is_nret);
    int popped = (STATE->call_depth < depth_before);

    /* Only signal an epilogue if a frame was popped.  Top-level returns
     * set g_jit_halted and the SM_HALT-like exit path doesn't need rbp
     * unwinding (the surrounding sm_jit_run frame's compiler-generated
     * epilogue handles its own rbp). */
    if (popped) STATE->jit_epilogue_pending = 1;
    return STATE->jit_epilogue_pending;
}

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
    /* ME-4-post: reset value-stack pointer at every statement boundary.
     * Mirrors the mode-2 contract at sm_interp.c:329 ("st->sp = 0 reset
     * value stack at each statement boundary").  Mode-3's inline ME-4
     * blobs write to state->stack[sp] without bounds checks; without
     * this reset, sp creeps monotonically across statements and
     * eventually overflows state->stack_cap, corrupting the heap.  Live
     * bug repro before this fix: multi-statement-arithmetic programs
     * aborted with `realloc(): invalid next size` during a subsequent
     * heap allocation that landed on the corruption (see ME-4 emergency
     * handoff note in GOAL-MODE3-EMIT.md).  Pre-grow of state->stack
     * (see sm_jit_run) plus per-statement sp-reset together provide the
     * mode-2-equivalent stack discipline. */
    STATE->sp = 0;
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
extern bb_node_t coro_eval(tree_t *e);
static void jit_pump_print(DESCR_t val, void *arg)
{
    (void)arg;
    char *s = VARVAL_fn(val);
    if (s) printf("%s\n", s);
}

static void h_bb_pump(void)
{
    DESCR_t expr_d = POP();
    tree_t *expr   = (tree_t *)expr_d.ptr;
    if (!expr) { STATE->last_ok = 0; return; }
    bb_node_t node = coro_eval(expr);
    int ticks = bb_broker(node, BB_PUMP, jit_pump_print, NULL);
    STATE->last_ok = (ticks > 0);
}

static void h_bb_once(void)
{
    DESCR_t expr_d = POP();
    tree_t *expr   = (tree_t *)expr_d.ptr;
    if (!expr) { STATE->last_ok = 0; return; }
    bb_node_t node = coro_eval(expr);
    int ticks = bb_broker(node, BB_ONCE, NULL, NULL);
    STATE->last_ok = (ticks > 0);
}

/* CH-17f: Prolog name-driven BB_ONCE — mirror of SM_BB_ONCE_PROC handler
 * in sm_interp.c.  No tree_t* pushed or walked at the SM layer.
 * IR fallback path until expression bodies are filled in a later rung. */
#include "../../frontend/prolog/pl_broker.h"
#include "../../runtime/interp/pl_runtime.h"
static void h_bb_once_proc(void)
{
    const char   *key   = CUR_INS->a[0].s;
    int           arity = (int)CUR_INS->a[1].i;
    tree_t       *choice = key ? pl_pred_table_lookup_global(key) : NULL;
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
        if ((tree_e)cmp_kinds[k] == AST_LEQ) {
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
    tree_t *every_ast = every_table_lookup(every_id);
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
    tree_t *ast = ast_pump_table_lookup(ast_id);
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

/* ME-9c helpers — charset pattern constructors (SM_PAT_ANY/NOTANY/SPAN/BREAK).
 * Each takes one DESCR_t arg, coerces to const char* via VARVAL_fn,
 * null-guards, then calls the pattern constructor. */
DESCR_t me9_pat_any(DESCR_t d)    { const char *cs = VARVAL_fn(d); return pat_any_cs(cs ? cs : ""); }
DESCR_t me9_pat_notany(DESCR_t d) { const char *cs = VARVAL_fn(d); return pat_notany(cs ? cs : ""); }
DESCR_t me9_pat_span(DESCR_t d)   { const char *cs = VARVAL_fn(d); return pat_span(cs ? cs : ""); }
DESCR_t me9_pat_break(DESCR_t d)  { const char *cs = VARVAL_fn(d); return pat_break_(cs ? cs : ""); }

/* ME-9d helpers — integer-arg pattern constructors
 * (SM_PAT_LEN/POS/RPOS/TAB/RTAB).  Each takes one DESCR_t arg, extracts
 * the integer if v==DT_I (else 0), then calls the pattern constructor. */
DESCR_t me9_pat_len(DESCR_t d)  { return pat_len (d.v == DT_I ? d.i : 0); }
DESCR_t me9_pat_pos(DESCR_t d)  { return pat_pos (d.v == DT_I ? d.i : 0); }
DESCR_t me9_pat_rpos(DESCR_t d) { return pat_rpos(d.v == DT_I ? d.i : 0); }
DESCR_t me9_pat_tab(DESCR_t d)  { return pat_tab (d.v == DT_I ? d.i : 0); }
DESCR_t me9_pat_rtab(DESCR_t d) { return pat_rtab(d.v == DT_I ? d.i : 0); }

/* ME-9g helper — SM_PAT_DEREF.  Mirrors h_pat_deref exactly:
 *   DT_P → pass-through (already a pattern);
 *   DT_S + non-null .s → pat_lit(.s);
 *   else → pat_ref(VARVAL_fn(v)) with null guard.
 * Keeping this discrimination in a C helper rather than x86 bytes is
 * the explicit ME-9g design choice (see GOAL-MODE3-EMIT.md Group G). */
DESCR_t me9_pat_deref(DESCR_t v) {
    if (v.v == DT_P) return v;
    if (v.v == DT_S && v.s) return pat_lit(v.s);
    const char *name = VARVAL_fn(v);
    return pat_ref(name ? name : "");
}

/* ME-10 helpers — capture / user-call pattern constructors.
 * Each mirrors the matching h_pat_* handler body exactly; only the
 * source of arguments differs (registers from the inline blob, vs
 * CUR_INS read by the standard-blob handler).  Net stack effect is
 * unchanged from the handler. */

/* SM_PAT_CAPTURE — pop child, NAME_fn(vn), 3-way dispatch on kind:
 *   kind == 1 → pat_assign_imm(child, var)         ($ capture, immediate)
 *   kind == 2 → pat_cat(child, pat_at_cursor(vn))  (. cursor capture)
 *   else      → pat_assign_cond(child, var)        (. capture, conditional)
 * Net delta: 0 (pop 1, push 1). */
DESCR_t me10_pat_capture(DESCR_t child, const char *vn, int64_t kind) {
    if (!vn) vn = "";
    DESCR_t var = NAME_fn(vn);
    if (kind == 1) return pat_assign_imm(child, var);
    if (kind == 2) return pat_cat(child, pat_at_cursor(vn));
    return pat_assign_cond(child, var);
}

/* SM_PAT_CAPTURE_FN — pop child, parse optional '\t'-separated namelist,
 * branch on is_imm.  Mirrors h_pat_capture_fn exactly.  Net delta: 0. */
DESCR_t me10_pat_capture_fn(DESCR_t child, const char *fname, int64_t is_imm, const char *namelist) {
    if (!fname) fname = "";
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
        return is_imm
            ? pat_assign_callcap_named_imm(child, fname, NULL, 0, names, nnames)
            : pat_assign_callcap_named(child, fname, NULL, 0, names, nnames);
    }
    return is_imm
        ? pat_assign_callcap_named_imm(child, fname, NULL, 0, NULL, 0)
        : pat_assign_callcap(child, fname, NULL, 0);
}

/* SM_PAT_USERCALL — bare *func() in pattern context.  No pop; pure
 * construction from fname.  Net delta: +1. */
DESCR_t me10_pat_usercall(const char *fname) {
    return pat_user_call(fname ? fname : "", NULL, 0);
}

/* ME-11 — SM_EXEC_STMT thin helper.
 * Called from native blob with r12 (TOS pointer, past top), baked sn, baked has_repl.
 * Stack layout (top-to-bottom): repl=[r12-16], subj=[r12-32], pat=[r12-48].
 * Calls exec_stmt; returns ok (1=success, 0=fail).
 * The blob pops 3 slots (sub r12, 48) and writes eax to STATE->last_ok. */
int me11_exec_stmt(DESCR_t *r12, const char *sn, int64_t has_repl)
{
    DESCR_t repl = r12[-1];    /* TOS   — replacement or INTVAL(0) */
    DESCR_t subj = r12[-2];    /* TOS-1 — subject descriptor        */
    DESCR_t pat  = r12[-3];    /* TOS-2 — pattern (DT_P)            */
    return exec_stmt(sn, &subj, pat, has_repl ? &repl : NULL, (int)has_repl);
}

/* ME-12 — SM_BB_PUMP / SM_BB_ONCE thin helpers.
 *
 * Each pops one DESCR_t (tree_t* in .ptr) and drives the Byrd-box broker.
 * Mirror of h_bb_pump / h_bb_once in this file; the only difference is
 * that the inline-blob version takes the DESCR_t by value (passed in
 * rdi:rsi by the calling blob) instead of popping it from STATE->stack.
 *
 * NO r12<->sp sync needed: bb_broker does not touch STATE->stack itself,
 * and the recursion paths it CAN reach (`_usercall_hook`, bb_eval_value,
 * bb_exec_stmt) either use a separate nested SM_State (the SM
 * dispatcher in _usercall_hook) or do not manipulate the SM value
 * stack at all (coro adapters).  This is the main correctness
 * difference vs ME-11's SM_EXEC_STMT path: exec_stmt does mutate the
 * caller's STATE->stack via PUSH(FAILDESCR) etc., so it needs sync;
 * bb_broker does not.
 */
int me12_bb_pump(DESCR_t expr_d) {
    tree_t *expr = (tree_t *)expr_d.ptr;
    if (!expr) return 0;
    bb_node_t node = coro_eval(expr);
    int ticks = bb_broker(node, BB_PUMP, jit_pump_print, NULL);
    return ticks > 0;
}

int me12_bb_once(DESCR_t expr_d) {
    tree_t *expr = (tree_t *)expr_d.ptr;
    if (!expr) return 0;
    bb_node_t node = coro_eval(expr);
    int ticks = bb_broker(node, BB_ONCE, NULL, NULL);
    return ticks > 0;
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
            STATE->jit_in_call = 1;   /* ME-6a: signal SM_DEFINE_ENTRY blob to do push rbp */
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
    g_handlers[SM_DEFINE_ENTRY] = h_define_entry;
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

    /* ME-4-post-r12-tos: read pc from [r13+20] where r13 = &SM_State.
     * Previously read [r12+20] when r12 = &SM_State; now r12 = SM value-
     * stack TOS pointer (FORTH-style) and r13 = &SM_State.
     *
     * mov eax, dword [r13+20]    (41 8b 45 14)  4 bytes — load st->pc
     */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

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
 * ME-4-post-r12-tos: r12 = SM value-stack TOS pointer (FORTH-style);
 * r13 = &SM_State.  PC bump via [r13+20].  Sync r12 ↔ st->sp around
 * the C handler call: handlers read/write stack via STATE->stack +
 * STATE->sp index, so we write sp from r12 before the call and reload
 * r12 from the new sp after.
 *
 * Sync formulae:
 *   r12 → sp:  st->sp = (r12 - st->stack) / 16
 *     mov rax, r12            48 4c 89 e0    no — that's mov rax, r12
 *     Actually: mov rax, r12; sub rax, [r13]; sar rax, 4; mov [r13+8], eax
 *   sp → r12:  r12 = st->stack + st->sp * 16
 *     mov eax, [r13+8]; shl rax, 4; add rax, [r13]; mov r12, rax
 *
 * Variable-size: 57 bytes (vs 28 bytes before — the +29 covers the
 * sync protocol).  Future optimization: emit_standard_blob_no_stack()
 * for handlers that don't touch the value stack (e.g. h_label which is
 * a no-op, h_jump_s/f which only touch pc) — they can skip both sync
 * blocks and shrink back to ~28 bytes.  Out of scope for this rung.
 *
 * 16-byte stack alignment discipline preserved from ME-3 — see the
 * original comment in git history for emit_standard_blob.
 */
static void emit_standard_blob(handler_fn_t fn, size_t trampoline_abs_off)
{
    /* inc dword [r13+20]  (41 ff 45 14)  4 bytes — pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* ── sync r12 → st->sp BEFORE the C call ────────────────────────────
     * st->sp = (r12 - st->stack) / 16
     *   mov rax, r12            (4c 89 e0)       3 bytes
     *   sub rax, [r13]          (49 2b 45 00)    4 bytes  (rax -= st->stack)
     *   sar rax, 4              (48 c1 f8 04)    4 bytes  (signed div by 16)
     *   mov [r13+8], eax        (41 89 45 08)    4 bytes  (st->sp = eax)
     */
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xe0);
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x2b);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x00);
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xf8); seg_byte(SEG_CODE, 0x04);
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x08);

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

    /* ── sync st->sp → r12 AFTER the C call ─────────────────────────────
     * r12 = st->stack + st->sp * 16
     *   mov eax, [r13+8]        (41 8b 45 08)    4 bytes  (eax = st->sp)
     *   shl rax, 4              (48 c1 e0 04)    4 bytes  (rax = sp*16)
     *   add rax, [r13]          (49 03 45 00)    4 bytes  (rax += st->stack)
     *   mov r12, rax            (49 89 c4)       3 bytes
     */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x08);
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe0); seg_byte(SEG_CODE, 0x04);
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x03);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x00);
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xc4);

    /* jmp rel32 trampoline.  rel32 is relative to the byte AFTER the
     * rel32 (i.e. the current seg head + 5). */
    size_t rel32_end_off = seg_offset(SEG_CODE) + 5;
    int32_t rel = (int32_t)((int64_t)trampoline_abs_off - (int64_t)rel32_end_off);
    seg_byte(SEG_CODE, 0xe9);
    seg_u32(SEG_CODE, (uint32_t)rel);
}

/*
 * emit_standard_blob_no_stack — emit a "call C handler" blob for handlers
 * that DO NOT read or write the SM value stack (and therefore don't need
 * the r12↔sp sync protocol).  Used for:
 *   - h_label          — no-op
 *   - h_stno           — reads pc operand, calls comm_stno, sets st->sp = 0
 *                        and IM-5 step-limit check; touches stack-pointer
 *                        but not stack contents — and the SP RESET is
 *                        exactly the convention we want anyway, so we
 *                        re-derive r12 from st->stack after.
 *   - h_jump_s/h_jump_f - obsolete: replaced by emit_cond_jump_blob below.
 *
 * Shape: PC bump + aligned call + jmp trampoline.  No sync blocks.
 *
 * IMPORTANT: when the handler resets st->sp to 0 (h_stno's job), r12 is
 * stale — it still points where it pointed before the call.  The post-
 * call reload `r12 = st->stack + st->sp * 16` re-derives the correct
 * TOS pointer (= st->stack at sp=0).  This is the SM_STNO mode-2
 * contract from REGISTER-LAYOUT.md.  For h_label which doesn't touch
 * st->sp, the reload is a no-op (r12 ends up where it started).
 *
 * Variable-size: 39 bytes (vs 57 for emit_standard_blob).  Saves 18B
 * per stack-neutral opcode on every dispatch.  In a tight loop hitting
 * SM_STNO + SM_JUMP_S every iteration, that's 36B less code + the
 * elided sync ops faster per iteration.
 *
 * The reload-only-no-pre-sync trick: handlers in this category don't
 * READ st->sp meaningfully (h_label ignores it; h_stno overrides it to
 * 0).  So we skip the pre-sync (no need to write the current sp from
 * r12), keep the post-sync (the reload) so r12 is current after.
 */
static void emit_standard_blob_no_stack(handler_fn_t fn, size_t trampoline_abs_off)
{
    /* inc dword [r13+20]  (41 ff 45 14)  4 bytes — pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

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

    /* Post-call: r12 = st->stack + st->sp * 16.  For h_label this is a
     * no-op (sp unchanged from caller); for h_stno this resets r12 to
     * st->stack (sp reset to 0).  Either way r12 is consistent after. */
    /*   mov eax, [r13+8]        (41 8b 45 08)    4 bytes */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x08);
    /*   shl rax, 4              (48 c1 e0 04)    4 bytes */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe0); seg_byte(SEG_CODE, 0x04);
    /*   add rax, [r13]          (49 03 45 00)    4 bytes */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x03);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x00);
    /*   mov r12, rax            (49 89 c4)       3 bytes */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xc4);

    /* jmp rel32 trampoline */
    size_t rel32_end_off_ns = seg_offset(SEG_CODE) + 5;
    int32_t rel_ns = (int32_t)((int64_t)trampoline_abs_off - (int64_t)rel32_end_off_ns);
    seg_byte(SEG_CODE, 0xe9);
    seg_u32(SEG_CODE, (uint32_t)rel_ns);
}

/*
 * emit_cond_jump_blob_skeleton — inline-native lowering of SM_JUMP_S /
 * SM_JUMP_F.  Reads st->last_ok via [r13+16] (no PLT call needed —
 * r13 is &SM_State and last_ok is at offset 16).  On take: sets pc to
 * target_pc and direct-rel32-jumps to blob[target_pc].  On fall-
 * through: sets pc to fallthru_pc (= this_pc + 1) and direct-rel32-
 * jumps to blob[fallthru_pc].
 *
 *   take_on_nonzero = 1 for SM_JUMP_S (jump when last_ok != 0)
 *   take_on_nonzero = 0 for SM_JUMP_F (jump when last_ok == 0)
 *
 * Bypasses the trampoline entirely — direct rel32 to both target and
 * fall-through.  Each blob has TWO rel32 patch sites; pass 2 fills
 * them in.  Returns the offset of the FIRST rel32 (target_pc); the
 * SECOND rel32 (fallthru_pc) is at first_off + 13 (deterministic
 * layout).
 *
 * Shape:
 *   cmp dword [r13+16], 0           5 bytes  (41 83 7d 10 00)
 *   jcc rel8 +12 (skip taken-side)  2 bytes  (74 0c or 75 0c)
 *   ; --- taken branch ---
 *   mov dword [r13+20], target_pc   8 bytes  (41 c7 45 14 <imm32>)
 *   jmp rel32 blob[target_pc]       5 bytes  (e9 <rel32>) [patch #1]
 *   ; --- fall-through branch ---
 *   mov dword [r13+20], fallthru    8 bytes  (41 c7 45 14 <imm32>)
 *   jmp rel32 blob[fallthru_pc]     5 bytes  (e9 <rel32>) [patch #2]
 *
 * Total: 33 bytes.  vs 57 for standard_blob → big win on jump-heavy
 * code.  Also: direct rel32 to target skips the trampoline's indirect-
 * jump pipeline stall on every branch.
 *
 * jcc encoding:
 *   take_on_nonzero=1 (SM_JUMP_S): jump-when-last_ok-nonzero.  We want
 *     to FALL THROUGH on zero, so skip-taken-on-zero.  jcc = je rel8.
 *     opcode 74.
 *   take_on_nonzero=0 (SM_JUMP_F): jump-when-last_ok-zero.  Skip-taken
 *     on nonzero.  jcc = jne rel8.  opcode 75.
 *
 * "skip taken side" means jcc over (8+5)=13 bytes if we want to land
 * on the fall-through side.  But we placed the JCC after a 5-byte cmp,
 * before the 13-byte taken side.  rel8 = 13 → 0x0d.  Hmm wait, jcc
 * rel8's reference is "byte after the jcc", so taking 13 means landing
 * 13 bytes past the jcc-end (which is byte 7 from blob start).  Fall-
 * through-side mov starts at byte 20.  byte 7 + 13 = 20.  ✓
 */
static size_t emit_cond_jump_blob_skeleton(int take_on_nonzero,
                                           int32_t target_pc,
                                           int32_t fallthru_pc)
{
    /* cmp dword [r13+16], 0  (41 83 7d 10 00)  5 bytes */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0x7d); seg_byte(SEG_CODE, 0x10); seg_byte(SEG_CODE, 0x00);

    /* jcc rel8 +13 (skip the taken-side 8+5=13 bytes) */
    if (take_on_nonzero) {
        /* SM_JUMP_S: skip taken when last_ok == 0.  je rel8. */
        seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x0d);
    } else {
        /* SM_JUMP_F: skip taken when last_ok != 0.  jne rel8. */
        seg_byte(SEG_CODE, 0x75); seg_byte(SEG_CODE, 0x0d);
    }

    /* --- taken branch --- */
    /* mov dword [r13+20], target_pc  (41 c7 45 14 <imm32>)  8 bytes */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xc7);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);
    seg_u32(SEG_CODE, (uint32_t)target_pc);
    /* jmp rel32 <placeholder>  (e9 <rel32>)  5 bytes — PATCH #1 */
    seg_byte(SEG_CODE, 0xe9);
    size_t target_rel32_off = seg_offset(SEG_CODE);
    seg_u32(SEG_CODE, 0);

    /* --- fall-through branch --- */
    /* mov dword [r13+20], fallthru_pc  (41 c7 45 14 <imm32>)  8 bytes */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xc7);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);
    seg_u32(SEG_CODE, (uint32_t)fallthru_pc);
    /* jmp rel32 <placeholder>  (e9 <rel32>)  5 bytes — PATCH #2 */
    seg_byte(SEG_CODE, 0xe9);
    /* size_t fallthru_rel32_off = seg_offset(SEG_CODE);  -- unused; deterministic */
    seg_u32(SEG_CODE, 0);

    return target_rel32_off;
}

/*
 * emit_halt_blob — emit the SM_HALT blob: advance pc past HALT,
 * then ret (returns out of sm_jit_run via sm_jit_run's `call entry()`).
 * ME-4-post-r12-tos: PC at [r13+20].  No value-stack interaction.
 * Variable-size: exactly 5 bytes.
 *
 * EM-MODE4-IS-MODE3-DUMP-c (sess 2026-05-11): kept as the rollback
 * reference but no longer called from the SM_HALT site.  Replaced by
 * `emit_halt_blob_via_template` below, which drives the per-opcode
 * template `emit_sm_halt` (templates/sm_halt.c) through a binary
 * emitter, capture-and-flush style into SEG_CODE.  The output is
 * byte-identical (41 ff 45 14 c3); the gate
 * `test_gate_em_template_byte_identity.sh` enforces this.
 *
 * If a future bug surfaces, single-line revert: change the call at
 * the SM_HALT case in the pass-1 dispatcher back to `emit_halt_blob()`.
 */
static void emit_halt_blob(void) __attribute__((unused));
static void emit_halt_blob(void)
{
    /* inc dword [r13+20]  (41 ff 45 14)  4 bytes */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* ret  (c3)  1 byte — returns to sm_jit_run's `entry();` call site */
    seg_byte(SEG_CODE, 0xc3);
}

/*
 * emit_halt_blob_via_template — EM-MODE4-IS-MODE3-DUMP-c adapter.
 *
 * Drives the SM_HALT per-opcode template (templates/sm_halt.c) through
 * a binary emitter and flushes the resulting bytes into SEG_CODE.
 *
 * Adapter rather than direct binary-emitter-into-SEG_CODE because the
 * binary emitter today writes into `bb_emit_buf` (the bb_pool current
 * buffer), not SEG_CODE.  The capture-and-flush pattern reuses the
 * proven binary backend unchanged: allocate a temporary buffer, run
 * the template into it, then `seg_byte` each captured byte into
 * SEG_CODE.  At 5 bytes per HALT, overhead is trivial.
 *
 * Byte-identity invariant: this function MUST produce exactly the
 * same byte sequence at the same SEG_CODE offset as `emit_halt_blob`
 * for every well-formed SM program.  The gate
 * `test_gate_em_template_byte_identity.sh` verifies this.
 */
static void emit_halt_blob_via_template(void)
{
    /* Temporary capture buffer — 16 bytes is generous for SM_HALT's
     * 5-byte sequence; future templates that emit more (SM_JUMP,
     * SM_CALL_FN, ...) will use the same pattern with a sized buffer. */
    uint8_t buf[16];
    bb_buf_t capture = buf;   /* bb_buf_t is `uint8_t *` (bb_pool.h:32) */

    /* Construct a binary emitter that targets the capture buffer.
     * emitter_binary_new() also sets bb_emit_mode = EMIT_BINARY and
     * calls bb_emit_begin(buf, size) which resets bb_emit_pos = 0. */
    emitter_t *e = emitter_binary_new(capture, (int)sizeof(buf));
    if (!e) {
        fprintf(stderr,
                "emit_halt_blob_via_template: emitter_binary_new failed; "
                "falling back to legacy emit_halt_blob\n");
        emit_halt_blob();
        return;
    }

    /* Run the template.  This populates buf[0..bb_emit_pos) with the
     * x86 byte sequence the binary backend renders for SM_HALT.
     * Formatting/comment calls on the emitter no-op for binary. */
    emit_sm_halt(e);

    /* Capture the byte count before freeing the emitter (which leaves
     * bb_emit_pos at its last-write value but is in any case a global
     * we must not depend on across the free). */
    int n = emitter_end(e);
    emitter_free(e);

    /* Sanity check: SM_HALT must produce exactly 5 bytes
     * (41 ff 45 14 c3).  Any deviation is a template bug; trip a
     * clean abort with a clear message rather than corrupting
     * SEG_CODE. */
    if (n != 5) {
        fprintf(stderr,
                "emit_halt_blob_via_template: template produced %d bytes; "
                "expected 5 (41 ff 45 14 c3) — template bug\n", n);
        abort();
    }

    /* Flush captured bytes into SEG_CODE at the current cursor. */
    for (int i = 0; i < n; i++) seg_byte(SEG_CODE, buf[i]);
}

/*
 * emit_jump_blob_skeleton — emit the SM_JUMP blob with the jmp rel32
 * displacement left as 0; pass 2 patches it with the real displacement
 * to blob[target_pc].  Returns the byte offset within SEG_CODE of the
 * rel32 field (used by the patcher).
 * ME-4-post-r12-tos: PC write via [r13+20].  Variable-size: 13 bytes.
 */
static size_t emit_jump_blob_skeleton(int32_t target_pc)
{
    /* mov dword [r13+20], target_pc  (41 c7 45 14 <imm32>)  8 bytes */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xc7);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);
    seg_u32(SEG_CODE, (uint32_t)target_pc);

    /* jmp rel32 <placeholder> (e9 <rel32>) 5 bytes — patched in pass 2 */
    seg_byte(SEG_CODE, 0xe9);
    size_t rel32_off = seg_offset(SEG_CODE);
    seg_u32(SEG_CODE, 0);

    return rel32_off;
}

/*
 * emit_label_blob — emit a TRULY stackless SM_LABEL blob.
 *
 * SM_LABEL is a build-time marker; its runtime handler `h_label` is a
 * literal no-op (see sm_interp.c case SM_LABEL: break;).  Mode-2 simply
 * skips it; mode-3 must too.
 *
 * BUG HISTORY (ME-14 / 2026-05-11c):
 * Prior to this rung, SM_LABEL routed through `emit_standard_blob_no_stack`
 * (shared with SM_STNO).  That blob skips the pre-sync (r12→sp) but
 * UNCONDITIONALLY runs the post-sync (sp→r12).  The post-sync was safe
 * for SM_STNO because h_stno explicitly resets sp=0 — the reload writes
 * a fresh value.  But for SM_LABEL the reload writes a STALE sp from
 * wherever the previous standard_blob ran, OVERWRITING any r12 growth
 * accumulated by inline-native blobs (SM_PUSH_VAR, SM_PAT_*, etc.) in
 * between.  Repro: Qize_driver.sno's *assign(.part, *'X') alternatives
 * — every alt's SM_PUSH_VAR + SM_PAT_DEREF + SM_JUMP -> SM_LABEL chain
 * lost the SM_PUSH_VAR's r12+=16 the moment SM_LABEL's post-sync fired,
 * leaving the per-alt pattern-build sp short by 1 slot.  Accumulated
 * across 12 SM_PUSH_EXPRESSION + 6 SM_PAT_CAPTURE_FN_ARGS + 5 SM_PAT_ALT
 * + 1 SM_PAT_CAT yielded STATE->sp = -3 at the closing SM_EXEC_STMT.
 *
 * Fix: make SM_LABEL blob a true no-op.  No C call.  No sync.  Just
 * pc++ + jmp trampoline.  9 bytes — 35 bytes smaller than the prior
 * standard_blob_no_stack shape (44 bytes) and faster on every fire.
 *
 * Shape (9 bytes):
 *   inc dword [r13+20]      (41 ff 45 14)  4 bytes — pc++
 *   jmp rel32 trampoline    (e9 <rel32>)   5 bytes — dispatch next
 */
static void emit_label_blob(size_t trampoline_abs_off)
{
    /* inc dword [r13+20]  (41 ff 45 14)  4 bytes — pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* jmp rel32 trampoline (e9 <rel32>) 5 bytes */
    size_t rel32_end_off = seg_offset(SEG_CODE) + 5;
    int32_t rel = (int32_t)((int64_t)trampoline_abs_off - (int64_t)rel32_end_off);
    seg_byte(SEG_CODE, 0xe9);
    seg_u32(SEG_CODE, (uint32_t)rel);
}

/*
 * emit_me6_define_entry_blob — emit the SM_LABEL blob for a DEFINE'd
 * function entry (ME-7 flag a[2].i == 1).  Adds a single-pair native
 * prologue (`push rbp; mov rbp, rsp`) before the standard pc-bump +
 * trampoline-jmp shape.  No C handler call needed — h_label is a no-op
 * for non-entry labels, and the entry-label case has nothing else to do
 * besides snapshot rbp.
 *
 * Shape (15 bytes):
 *   inc dword [r13+20]      (41 ff 45 14)  4 bytes — pc++
 *   push rbp                (55)           1 byte  — save caller's rbp
 *   mov rbp, rsp            (48 89 e5)     3 bytes — establish frame
 *   jmp rel32 trampoline    (e9 <rel32>)   5 bytes — dispatch next
 *
 * Why pc++ before the prologue rather than after: the standard pc-bump
 * uses the same shape (`inc dword [r13+20]`) as every other blob, so
 * keeping it first preserves the invariant "pc advances exactly once per
 * blob entry, before any side-effect that could fail".  The push/mov
 * pair is straight-line C-ABI-safe code and won't fail.
 *
 * rbp discipline: every define-entry blob does push+mov; every paired
 * SM_RETURN-variant blob does mov+pop conditionally (gated by the
 * jit_epilogue_pending signal from me6_return_dispatch).  Mismatched
 * pairs would corrupt rbp; the gate ensures we only unwind when
 * me6_return_dispatch confirmed a frame pop, which happens only when
 * a function called via h_call (which is what creates the call frame
 * that body_pc's SM_LABEL define-entry was the destination of).
 */
static void emit_me6_define_entry_blob(size_t trampoline_abs_off) __attribute__((unused));
static void emit_me6_define_entry_blob(size_t trampoline_abs_off)
{
    /* ME-6a: SM_DEFINE_ENTRY blob (+ ME-13 rsp-alignment fix).
     *
     * Shape (~30 bytes):
     *   inc dword [r13+20]       4 bytes  — pc++
     *   mov eax, [r13+28]        4 bytes  — eax = jit_in_call
     *   mov dword [r13+28], 0    7 bytes  — jit_in_call = 0 (always clear)
     *   test eax, eax            2 bytes  — was this a real call?
     *   jz skip  (rel8 +8)       2 bytes  — no: skip prologue entirely
     *   push rbp                 1 byte   — save caller's rbp
     *   mov rbp, rsp             3 bytes  — establish frame (16-aligned)
     *   sub rsp, 8               4 bytes  — ME-13: restore rsp%16==8 invariant
     *   skip:
     *   jmp rel32 trampoline     5 bytes  — dispatch next
     *
     * jit_in_call lives at SM_State offset 28 ([r13+28]).
     * Cleared unconditionally before the test so a stale 1 can never leak
     * across statements.
     *
     * The trailing `sub rsp, 8` keeps the dispatch loop's rsp%16==8
     * invariant intact for the user function body.  Without it, every
     * C-ABI call inside the body has rsp=8 mod 16 at the `call rax`
     * (instead of 0 mod 16), which breaks SSE-aligned access patterns
     * in glibc snprintf and crashes any --jit-run program that calls
     * a DT_I-formatting builtin (ARRAY, CONVERT, ...) from inside a
     * DEFINE'd function.  See the comment block at the jz site below
     * for the full reasoning.  The matching unwind in
     * emit_me6_return_blob (`mov rsp, rbp ; pop rbp`) is unchanged:
     * `mov rsp, rbp` strip-restores the subtract automatically.
     */

    /* inc dword [r13+20]  (41 ff 45 14) */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* mov eax, [r13+28]  (41 8b 45 1c) */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x1c);

    /* mov dword [r13+28], 0  (41 c7 45 1c 00 00 00 00)  7 bytes */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xc7);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x1c);
    seg_byte(SEG_CODE, 0x00); seg_byte(SEG_CODE, 0x00);
    seg_byte(SEG_CODE, 0x00); seg_byte(SEG_CODE, 0x00);

    /* test eax, eax  (85 c0) */
    seg_byte(SEG_CODE, 0x85); seg_byte(SEG_CODE, 0xc0);

    /* ME-13 (rsp-alignment fix): jz skip rel8 = +8  (74 08) — skip
     * push rbp + mov rbp,rsp + sub rsp,8 (4 + 4 = 8 bytes).
     *
     * The dispatch-loop invariant is rsp % 16 == 8 at every trampoline
     * entry (this is the post-`call`-into-JIT-entry state).  The standard
     * blob's `sub rsp, 8 ; call rax ; add rsp, 8` then puts rsp at 0 mod
     * 16 right at the `call rax`, which is the SysV C-ABI requirement
     * (callee sees rsp = 8 mod 16 immediately after the call's pushed
     * return address).
     *
     * `push rbp` alone moves rsp by -8, flipping the invariant from 8
     * mod 16 to 0 mod 16.  Inside the user-function body every standard
     * blob then makes its C-ABI call with rsp = 8 mod 16 at the `call`,
     * which violates the ABI.  C handlers that don't use SSE/AVX align
     * tolerate it silently; glibc snprintf uses `movaps -0xc0(%rbp)` and
     * crashes with SIGSEGV.  Repro: any --jit-run program that calls a
     * builtin (e.g. ARRAY, CONVERT) on a DT_I argument from inside a
     * DEFINE'd function — VARVAL_fn formats the integer via snprintf
     * and faults inside __vsnprintf_internal.
     *
     * The extra `sub rsp, 8` here restores the 8 mod 16 invariant for
     * the function body.  The matching unwind in emit_me6_return_blob
     * is unchanged: `mov rsp, rbp` already discards the extra subtract
     * (rsp ends back at the value captured by `mov rbp, rsp` above,
     * which is rsp right after `push rbp` — i.e. 0 mod 16, the pre-
     * subtract value), and `pop rbp` then restores the original
     * 8 mod 16 invariant for the caller's resume PC.
     */
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x08);

    /* push rbp  (55) */
    seg_byte(SEG_CODE, 0x55);

    /* mov rbp, rsp  (48 89 e5) */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xe5);

    /* sub rsp, 8  (48 83 ec 08)  4 bytes — ME-13 rsp-alignment fix.
     * Restores the dispatch-loop's rsp % 16 == 8 invariant after
     * push rbp.  Strip-restored by mov rsp,rbp in emit_me6_return_blob.
     */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xec); seg_byte(SEG_CODE, 0x08);

    /* skip: jmp rel32 trampoline */
    size_t rel32_end_off = seg_offset(SEG_CODE) + 5;
    int32_t rel = (int32_t)((int64_t)trampoline_abs_off - (int64_t)rel32_end_off);
    seg_byte(SEG_CODE, 0xe9);
    seg_u32(SEG_CODE, (uint32_t)rel);
}

/*
 * emit_me6_return_blob — emit a return-variant blob (SM_RETURN /
 * SM_FRETURN / SM_NRETURN and their _S / _F conditional siblings).
 *
 * Shape (variable, ~70 bytes):
 *   inc dword [r13+20]                        4 bytes — pc++
 *   ── sync r12 → st->sp ── (15 bytes; see emit_standard_blob) ──
 *   mov rax, r12                              3 bytes
 *   sub rax, [r13]                            4 bytes
 *   sar rax, 4                                4 bytes
 *   mov [r13+8], eax                          4 bytes
 *   mov edi, <bits_imm32>                     5 bytes — variant arg
 *   mov rax, &me6_return_dispatch             10 bytes — imm64
 *   sub rsp, 8                                4 bytes — 16-byte align
 *   call rax                                  2 bytes
 *   add rsp, 8                                4 bytes
 *   ── sync st->sp → r12 ── (15 bytes) ──
 *   mov ecx, [r13+8]                          4 bytes
 *   shl rcx, 4                                4 bytes  (use rcx — rax is the dispatch return)
 *   add rcx, [r13]                            4 bytes
 *   mov r12, rcx                              3 bytes
 *   test eax, eax                             2 bytes — eax = dispatch return value
 *   jz no_unwind  (rel8 +4 — skip mov/pop)    2 bytes
 *   mov rsp, rbp                              3 bytes — force-restore rsp
 *   pop rbp                                   1 byte  — restore caller's rbp
 *   no_unwind:
 *   jmp rel32 trampoline                      5 bytes
 *
 * Total: 4 + 15 + 5 + 10 + 4 + 2 + 4 + 15 + 2 + 2 + 3 + 1 + 5 = 72 bytes.
 *
 * Why we use rcx in the post-sync rather than rax: rax carries the
 * dispatch helper's return value (the jit_epilogue_pending flag), which
 * we need to test after the sync.  Using rcx for the sync arithmetic
 * preserves rax across the sync block.
 *
 * The `test eax, eax ; jz no_unwind` pattern: jz fires when ZF=1
 * (i.e. eax==0, meaning no epilogue pending), skipping the mov/pop.
 * jnz would fire when eax!=0, executing the mov/pop.  We use jz to
 * skip-to-no_unwind, which matches the "if no signal, skip the
 * unwind sequence" reading.  rel8 displacement = 4 bytes (length of
 * the mov rsp,rbp ; pop rbp sequence: 3 + 1 = 4).
 */
static void emit_me6_return_blob(int bits, size_t trampoline_abs_off) __attribute__((unused));
static void emit_me6_return_blob(int bits, size_t trampoline_abs_off)
{
    /* inc dword [r13+20]  (41 ff 45 14)  4 bytes — pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* ── sync r12 → st->sp ────────────────────────────────────────────── */
    /*   mov rax, r12            (4c 89 e0)       3 bytes */
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xe0);
    /*   sub rax, [r13]          (49 2b 45 00)    4 bytes */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x2b);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x00);
    /*   sar rax, 4              (48 c1 f8 04)    4 bytes */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xf8); seg_byte(SEG_CODE, 0x04);
    /*   mov [r13+8], eax        (41 89 45 08)    4 bytes */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x08);

    /* mov edi, bits   (bf <imm32>)  5 bytes — argument to me6_return_dispatch */
    seg_byte(SEG_CODE, 0xbf);
    seg_u32(SEG_CODE, (uint32_t)bits);

    /* mov rax, &me6_return_dispatch  (48 b8 <imm64>)  10 bytes */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xb8);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)me6_return_dispatch);

    /* sub rsp, 8  (48 83 ec 08)  4 bytes — 16-byte alignment */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xec); seg_byte(SEG_CODE, 0x08);

    /* call rax  (ff d0)  2 bytes */
    seg_byte(SEG_CODE, 0xff); seg_byte(SEG_CODE, 0xd0);

    /* add rsp, 8  (48 83 c4 08)  4 bytes */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xc4); seg_byte(SEG_CODE, 0x08);

    /* ── sync st->sp → r12 (use rcx so rax survives for the test) ─────── */
    /*   mov ecx, [r13+8]        (41 8b 4d 08)    4 bytes */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x08);
    /*   shl rcx, 4              (48 c1 e1 04)    4 bytes */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe1); seg_byte(SEG_CODE, 0x04);
    /*   add rcx, [r13]          (49 03 4d 00)    4 bytes */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x03);
    seg_byte(SEG_CODE, 0x4d); seg_byte(SEG_CODE, 0x00);
    /*   mov r12, rcx            (49 89 cc)       3 bytes */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xcc);

    /* test eax, eax  (85 c0)  2 bytes — check dispatch return value */
    seg_byte(SEG_CODE, 0x85); seg_byte(SEG_CODE, 0xc0);

    /* jz no_unwind, rel8 = +4 (skip mov rsp,rbp + pop rbp)  (74 04)  2 bytes */
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x04);

    /* mov rsp, rbp  (48 89 ec)  3 bytes — force-restore rsp to frame */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xec);

    /* pop rbp  (5d)  1 byte — restore caller's rbp */
    seg_byte(SEG_CODE, 0x5d);

    /* no_unwind: jmp rel32 trampoline */
    size_t rel32_end_off = seg_offset(SEG_CODE) + 5;
    int32_t rel = (int32_t)((int64_t)trampoline_abs_off - (int64_t)rel32_end_off);
    seg_byte(SEG_CODE, 0xe9);
    seg_u32(SEG_CODE, (uint32_t)rel);
}

/* ── ME-4-post-r12-tos inline-native blob emitters ────────────────────────
 *
 * r12 = SM value-stack TOS pointer (FORTH-style).  Top live entry is at
 *   [r12-16] (low 8 bytes = DESCR.v) and [r12-8] (high 8 bytes = DESCR.data).
 *   Entry below TOS is at [r12-32] / [r12-24].  Push: write at [r12]/[r12+8]
 *   and add r12, 16.  Pop N: sub r12, N*16.
 * r13 = &SM_State, used for [r13+20] = pc.
 *
 * Each blob:
 *   1. inc dword [r13+20]   (pc++)
 *   2. Load DESCR_t args from r12-anchored stack into SysV arg regs
 *      (16-byte struct = {rdi/rsi} = arg 1, {rdx/rcx} = arg 2, etc.)
 *   3. Call the me4_* helper via imm64 (SEG_CODE↔scrip-text distance
 *      exceeds rel32 reach in mmap-allocated SEG_CODE — see ME-3 notes)
 *   4. Store the returned DESCR_t (rax:rdx) back into the appropriate
 *      stack slot
 *   5. Adjust r12 by the net stack delta of this opcode
 *   6. jmp rel32 to trampoline for next opcode dispatch
 *
 * me4_* helpers are pure value-in / value-out — they never read or write
 * st->sp / st->stack.  So no sp-sync around the call is needed (unlike
 * the standard blob which calls handlers that DO touch the stack).  r12
 * is callee-saved per SysV so the helper preserves it across the call.
 */

/* Helper: 16-byte rsp alignment + call rax via imm64.  20 bytes. */
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

/* Helper: jmp rel32 to trampoline.  5 bytes. */
static void emit_jmp_trampoline(size_t trampoline_abs_off)
{
    size_t rel32_end_off = seg_offset(SEG_CODE) + 5;
    int32_t rel = (int32_t)((int64_t)trampoline_abs_off - (int64_t)rel32_end_off);
    seg_byte(SEG_CODE, 0xe9);
    seg_u32(SEG_CODE, (uint32_t)rel);
}

/* emit_me4_arith_blob — SM_ADD/SUB/MUL/DIV/MOD inline-native lowering.
 *
 * ME-4-post-r12-tos shape:
 *   r12 = SM value-stack TOS pointer.  TOS at [r12-16]/[r12-8]; TOS-1 at
 *   [r12-32]/[r12-24].  Pop 2, push 1 — net delta: r12 -= 16.
 *   r13 = &SM_State; pc bumped via [r13+20].
 *
 * Calls me4_arith(l=arg1, r=arg2, op=arg3).  16-byte struct args land
 * in {rdi,rsi} and {rdx,rcx} per SysV; the 3rd integer arg (sm_opcode_t)
 * goes in r8d.
 *
 * Blob size: 64 bytes.
 */
static void emit_me4_arith_blob(sm_opcode_t op, size_t trampoline_abs_off)
{
    extern DESCR_t me4_arith(DESCR_t, DESCR_t, sm_opcode_t);

    /* inc dword [r13+20]              (41 ff 45 14)         4 — pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* mov rdi, [r12-32]               (49 8b 7c 24 e0)      5 — l.lo (TOS-1.lo) */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x7c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe0);

    /* mov rsi, [r12-24]               (49 8b 74 24 e8)      5 — l.hi */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe8);

    /* mov rdx, [r12-16]               (49 8b 54 24 f0)      5 — r.lo (TOS.lo) */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0);

    /* mov rcx, [r12-8]                (49 8b 4c 24 f8)      5 — r.hi */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8);

    /* mov r8d, <op_imm32>             (41 b8 <imm32>)       6 — 3rd arg */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xb8);
    seg_u32(SEG_CODE, (uint32_t)op);

    emit_aligned_call_imm64((void *)&me4_arith);            /* 20 bytes */

    /* Store result (rax:rdx) into the TOS-1 slot ([r12-32], [r12-24]).
     * That slot becomes the new TOS after we sub r12, 16 below. */
    /* mov [r12-32], rax               (49 89 44 24 e0)      5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe0);

    /* mov [r12-24], rdx               (49 89 54 24 e8)      5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe8);

    /* sub r12, 16                     (49 83 ec 10)         4 — net pop 1 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xec); seg_byte(SEG_CODE, 0x10);

    emit_jmp_trampoline(trampoline_abs_off);                /* 5 bytes */
}

/* emit_me4_concat_blob — SM_CONCAT.  Same shape as arith but no op arg.
 * Calls me4_concat(l, r).  Blob size: 58 bytes. */
static void emit_me4_concat_blob(size_t trampoline_abs_off)
{
    extern DESCR_t me4_concat(DESCR_t, DESCR_t);

    /* pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* Load l from TOS-1, r from TOS */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x7c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe0); /* rdi = [r12-32] */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe8); /* rsi = [r12-24] */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0); /* rdx = [r12-16] */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8); /* rcx = [r12-8] */

    emit_aligned_call_imm64((void *)&me4_concat);

    /* Store result at TOS-1 slot, pop 1 net */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe0); /* [r12-32] = rax */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe8); /* [r12-24] = rdx */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xec); seg_byte(SEG_CODE, 0x10);                          /* sub r12, 16 */

    emit_jmp_trampoline(trampoline_abs_off);
}

/* emit_me4_coerce_num_blob — SM_COERCE_NUM.  Pops 1, calls
 * me4_coerce_num(v), pushes 1.  Net delta: 0 (TOS rewritten in place).
 * Blob size: 48 bytes. */
static void emit_me4_coerce_num_blob(size_t trampoline_abs_off)
{
    extern DESCR_t me4_coerce_num(DESCR_t);

    /* pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* Load TOS into rdi:rsi (the only arg) */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x7c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0); /* rdi = [r12-16] */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8); /* rsi = [r12-8] */

    emit_aligned_call_imm64((void *)&me4_coerce_num);

    /* Write result back to TOS slot (no r12 adjustment — net delta 0) */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0); /* [r12-16] = rax */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8); /* [r12-8] = rdx */

    emit_jmp_trampoline(trampoline_abs_off);
}

/* emit_me4_push_null_blob — SM_PUSH_NULL.  No args; calls me4_push_null(),
 * pushes NULVCL.  Net delta: +1.  Blob size: 42 bytes. */
static void emit_me4_push_null_blob(size_t trampoline_abs_off)
{
    extern DESCR_t me4_push_null(void);

    /* pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    emit_aligned_call_imm64((void *)&me4_push_null);

    /* Push result: write at [r12]/[r12+8], then advance r12 by 16 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);                          /* mov [r12], rax */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08); /* mov [r12+8], rdx */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xc4); seg_byte(SEG_CODE, 0x10);                          /* add r12, 16 */

    emit_jmp_trampoline(trampoline_abs_off);
}

/* emit_me4_push_var_blob — SM_PUSH_VAR.  Calls me4_push_var(name), pushes
 * result.  name baked as imm64.  Net delta: +1.  Blob size: 52 bytes. */
static void emit_me4_push_var_blob(const char *name, size_t trampoline_abs_off)
{
    extern DESCR_t me4_push_var(const char *);

    /* pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* mov rdi, imm64(name)            (48 bf <imm64>)       10 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xbf);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)name);

    emit_aligned_call_imm64((void *)&me4_push_var);

    /* Push result */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);                          /* mov [r12], rax */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08); /* mov [r12+8], rdx */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xc4); seg_byte(SEG_CODE, 0x10);                          /* add r12, 16 */

    emit_jmp_trampoline(trampoline_abs_off);
}

/* emit_me4_store_var_blob — SM_STORE_VAR.  Pops val (TOS), calls
 * me4_store_var(name, val), pushes val back (assignment-expression value).
 * Net delta: 0 (pop 1, push 1 — TOS rewritten in place).
 * Blob size: 60 bytes. */
static void emit_me4_store_var_blob(const char *name, size_t trampoline_abs_off)
{
    extern DESCR_t me4_store_var(const char *, DESCR_t);

    /* pc++ */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* Load val (TOS) into rsi:rdx (the SECOND DESCR_t arg). */
    /* mov rsi, [r12-16]               (49 8b 74 24 f0)      5 — val.lo */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0);
    /* mov rdx, [r12-8]                (49 8b 54 24 f8)      5 — val.hi */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8);

    /* mov rdi, imm64(name) — first arg */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xbf);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)name);

    emit_aligned_call_imm64((void *)&me4_store_var);

    /* Write returned val (rax:rdx) back to TOS slot (no r12 adjustment) */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0); /* [r12-16] = rax */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8); /* [r12-8] = rdx */

    emit_jmp_trampoline(trampoline_abs_off);
}

/*------------------------------------------------------------------------*/
/* ME-9a — Nullary pattern primitives.                                    */
/*                                                                        */
/* Eight SM_PAT_* opcodes have the same blob shape:                       */
/*   SM_PAT_ARB SM_PAT_REM SM_PAT_FAIL SM_PAT_SUCCEED                     */
/*   SM_PAT_EPS SM_PAT_FENCE SM_PAT_ABORT SM_PAT_BAL                      */
/*                                                                        */
/* Each calls a parameter-less runtime constructor `pat_X(void)` whose    */
/* DESCR_t return lands in rax:rdx, then pushes the result on the r12     */
/* value stack.  Net delta: +1.  No args consumed from the stack.         */
/*                                                                        */
/* Mirrors emit_me4_push_null_blob exactly — same arg-less, push-1 shape, */
/* just a different imm64 target.                                         */
/*                                                                        */
/* Layout (42 bytes):                                                     */
/*    inc  dword [r13+20]      ; pc++                          4          */
/*    sub  rsp, 8              ; align rsp to 16              \           */
/*    mov  rax, imm64(fn)      ; load function ptr             \  20      */
/*    call rax                                                 /          */
/*    add  rsp, 8              ; restore                      /           */
/*    mov  [r12], rax          ; push result.lo                4          */
/*    mov  [r12+8], rdx        ; push result.hi                5          */
/*    add  r12, 16             ; advance TOS                   4          */
/*    jmp  rel32 trampoline                                    5          */
/*                                                          = 42          */
/*                                                                        */
/* last_ok policy: untouched.  pat_X() constructors never fail and never  */
/* set last_ok in mode 2; the inline blob preserves that.                 */
/*------------------------------------------------------------------------*/
static void emit_me9_pat_nullary_blob(void *rt_fn, size_t trampoline_abs_off)
{
    /* pc++                            (41 ff 45 14)                4 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* aligned call rt_fn — DESCR_t returned in rax:rdx          20 */
    emit_aligned_call_imm64(rt_fn);

    /* push result onto r12 stack ([r12]/[r12+8]; r12 += 16)    */
    /* mov [r12], rax                  (49 89 04 24)                4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);
    /* mov [r12+8], rdx                (49 89 54 24 08)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);
    /* add r12, 16                     (49 83 c4 10)                 4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xc4); seg_byte(SEG_CODE, 0x10);

    emit_jmp_trampoline(trampoline_abs_off);                       /* 5 */
}

/*------------------------------------------------------------------------*/
/* ME-9b — SM_PAT_LIT.                                                    */
/*                                                                        */
/* Calls pat_lit(s) where s is the literal string baked as an imm64       */
/* operand (from prog->instrs[i].a[0].s).  No stack args; net delta +1.   */
/*                                                                        */
/* Mirrors emit_me4_push_var_blob exactly — same one-string-arg shape,    */
/* just a different runtime target.                                       */
/*                                                                        */
/* Layout (52 bytes):                                                     */
/*    inc  dword [r13+20]      ; pc++                          4          */
/*    mov  rdi, imm64(s)       ; literal pointer              10          */
/*    sub  rsp, 8              ; align                        \           */
/*    mov  rax, imm64(pat_lit)                                 \  20      */
/*    call rax                                                 /          */
/*    add  rsp, 8                                             /           */
/*    mov  [r12], rax          ; push result.lo                4          */
/*    mov  [r12+8], rdx        ; push result.hi                5          */
/*    add  r12, 16             ; advance TOS                   4          */
/*    jmp  rel32 trampoline                                    5          */
/*                                                          = 52          */
/*                                                                        */
/* NULL safety: pat_lit() itself null-guards s (snobol4_pattern.c:84-88), */
/* so the blob can pass a[0].s through unchecked.                         */
/*------------------------------------------------------------------------*/
static void emit_me9_pat_lit_blob(const char *lit, size_t trampoline_abs_off)
{
    /* pc++                            (41 ff 45 14)                4 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* mov rdi, imm64(lit)             (48 bf <imm64>)              10 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xbf);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)lit);

    /* aligned call pat_lit — DESCR_t in rax:rdx                  20 */
    emit_aligned_call_imm64((void *)&pat_lit);

    /* push result on r12 stack                                         */
    /* mov [r12], rax                  (49 89 04 24)                4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);
    /* mov [r12+8], rdx                (49 89 54 24 08)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);
    /* add r12, 16                     (49 83 c4 10)                 4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xc4); seg_byte(SEG_CODE, 0x10);

    emit_jmp_trampoline(trampoline_abs_off);                       /* 5 */
}

/*------------------------------------------------------------------------*/
/* ME-9g — SM_PAT_REFNAME.                                                */
/*                                                                        */
/* Calls pat_ref(name) where name is the literal string baked as an      */
/* imm64 operand (from prog->instrs[i].a[0].s).  No stack args; net      */
/* delta +1.  pat_ref() null-guards internally so a[0].s passes through. */
/*                                                                        */
/* Layout (52 bytes) — same shape as emit_me9_pat_lit_blob, only the     */
/* imm64 call target differs.                                             */
/*------------------------------------------------------------------------*/
static void emit_me9_pat_refname_blob(const char *name, size_t trampoline_abs_off)
{
    /* pc++                            (41 ff 45 14)                4 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* mov rdi, imm64(name)            (48 bf <imm64>)              10 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xbf);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)name);

    /* aligned call pat_ref — DESCR_t in rax:rdx                  20 */
    emit_aligned_call_imm64((void *)&pat_ref);

    /* push result on r12 stack                                         */
    /* mov [r12], rax                  (49 89 04 24)                4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);
    /* mov [r12+8], rdx                (49 89 54 24 08)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);
    /* add r12, 16                     (49 83 c4 10)                 4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xc4); seg_byte(SEG_CODE, 0x10);

    emit_jmp_trampoline(trampoline_abs_off);                       /* 5 */
}

/*------------------------------------------------------------------------*/
/* ME-9c — SM_PAT_ANY / SM_PAT_NOTANY / SM_PAT_SPAN / SM_PAT_BREAK.      */
/*                                                                        */
/* Each pops one DESCR_t from r12, passes it to a me9_pat_X(DESCR_t)     */
/* helper (which calls VARVAL_fn + null-guards + pat constructor), then   */
/* writes the DT_P result back in place.  Net delta: 0 (pop 1, push 1).  */
/*                                                                        */
/* Identical shape to emit_me4_coerce_num_blob: TOS loaded into rdi:rsi  */
/* (DESCR_t ABI), call via imm64, result written back to [r12-16],[r12-8]*/
/*                                                                        */
/* Layout (57 bytes):                                                     */
/*    inc  dword [r13+20]      ; pc++                          4          */
/*    mov  rdi, [r12-16]       ; arg.lo                        5          */
/*    mov  rsi, [r12-8]        ; arg.hi                        5          */
/*    sub  rsp, 8              ; align          \              */
/*    mov  rax, imm64(me9_*)   ;                 \  20         */
/*    call rax                 ;                 /             */
/*    add  rsp, 8              ; re-align       /              */
/*    mov  [r12-16], rax       ; result.lo                     5          */
/*    mov  [r12-8],  rdx       ; result.hi                     5          */
/*    jmp  rel32 trampoline                                    5          */
/*                                                          = 54          */
/*------------------------------------------------------------------------*/
static void emit_me9_pat_charset_blob(void *me9_helper, size_t trampoline_abs_off)
{
    extern DESCR_t me9_pat_any(DESCR_t);

    /* pc++                            (41 ff 45 14)                4 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* Load TOS into rdi:rsi (DESCR_t arg) */
    /* mov rdi, [r12-16]               (49 8b 7c 24 f0)             5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x7c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0);
    /* mov rsi, [r12-8]                (49 8b 74 24 f8)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8);

    /* aligned call me9_helper — DESCR_t returned in rax:rdx        20 */
    emit_aligned_call_imm64(me9_helper);

    /* Write result back to TOS slot (no r12 adjustment — net delta 0) */
    /* mov [r12-16], rax               (49 89 44 24 f0)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0);
    /* mov [r12-8], rdx                (49 89 54 24 f8)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8);

    emit_jmp_trampoline(trampoline_abs_off);                       /* 5 */
}

/*------------------------------------------------------------------------*/
/* ME-9f — SM_PAT_CAT / SM_PAT_ALT.                                       */
/*                                                                        */
/* Pops two DESCR_t values (left=TOS-1, right=TOS), calls                 */
/* fn(left, right), writes result back at TOS-1 slot, decrements r12      */
/* by 16.  Net delta: −1 (pop 2, push 1).                                 */
/*                                                                        */
/* Identical shape to emit_me4_concat_blob — same signature               */
/* DESCR_t(*)(DESCR_t,DESCR_t).  pat_cat and pat_alt are both direct      */
/* runtime entry points, so no me9_* helper indirection.                  */
/*                                                                        */
/* Layout (58 bytes):                                                     */
/*    inc  dword [r13+20]      ; pc++                          4          */
/*    mov  rdi, [r12-32]       ; left.lo                       5          */
/*    mov  rsi, [r12-24]       ; left.hi                       5          */
/*    mov  rdx, [r12-16]       ; right.lo                      5          */
/*    mov  rcx, [r12-8]        ; right.hi                      5          */
/*    sub  rsp, 8              ; align          \              */
/*    mov  rax, imm64(fn)      ;                 \  20         */
/*    call rax                 ;                 /             */
/*    add  rsp, 8              ; re-align       /              */
/*    mov  [r12-32], rax       ; result.lo at TOS-1 slot       5          */
/*    mov  [r12-24], rdx       ; result.hi                     5          */
/*    sub  r12, 16             ; net pop 1                     4          */
/*    jmp  rel32 trampoline                                    5          */
/*                                                          = 63          */
/*------------------------------------------------------------------------*/
static void emit_me9_pat_binary_blob(void *fn, size_t trampoline_abs_off)
{
    /* pc++                            (41 ff 45 14)                4 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* Load left (TOS-1) into rdi:rsi */
    /* mov rdi, [r12-32]               (49 8b 7c 24 e0)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x7c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe0);
    /* mov rsi, [r12-24]               (49 8b 74 24 e8)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe8);

    /* Load right (TOS) into rdx:rcx */
    /* mov rdx, [r12-16]               (49 8b 54 24 f0)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0);
    /* mov rcx, [r12-8]                (49 8b 4c 24 f8)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8);

    /* aligned call fn — DESCR_t returned in rax:rdx              20 */
    emit_aligned_call_imm64(fn);

    /* Store result at TOS-1 slot */
    /* mov [r12-32], rax               (49 89 44 24 e0)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe0);
    /* mov [r12-24], rdx               (49 89 54 24 e8)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xe8);

    /* sub r12, 16                     (49 83 ec 10)                 4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xec); seg_byte(SEG_CODE, 0x10);

    emit_jmp_trampoline(trampoline_abs_off);                       /* 5 */
}

/*------------------------------------------------------------------------*/
/* ME-10 — SM_PAT_USERCALL.                                               */
/*                                                                        */
/* No stack args; bakes fname as imm64, calls me10_pat_usercall(fname),  */
/* pushes DT_P result.  Net delta +1.  Same shape as                     */
/* emit_me9_pat_refname_blob (52 bytes), only the imm64 call target      */
/* differs.                                                               */
/*------------------------------------------------------------------------*/
static void emit_me10_pat_usercall_blob(const char *fname, size_t trampoline_abs_off)
{
    /* pc++                            (41 ff 45 14)                4 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* mov rdi, imm64(fname)           (48 bf <imm64>)              10 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xbf);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)fname);

    /* aligned call me10_pat_usercall — DESCR_t in rax:rdx          20 */
    emit_aligned_call_imm64((void *)&me10_pat_usercall);

    /* push result on r12 stack                                         */
    /* mov [r12], rax                  (49 89 04 24)                4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x04); seg_byte(SEG_CODE, 0x24);
    /* mov [r12+8], rdx                (49 89 54 24 08)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0x08);
    /* add r12, 16                     (49 83 c4 10)                 4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xc4); seg_byte(SEG_CODE, 0x10);

    emit_jmp_trampoline(trampoline_abs_off);                       /* 5 */
}

/*------------------------------------------------------------------------*/
/* ME-10 — SM_PAT_CAPTURE.                                                */
/*                                                                        */
/* Pop child (TOS) into rdi:rsi, bake vn as rdx imm64, bake kind as rcx  */
/* imm64, call me10_pat_capture(child, vn, kind), write result back at   */
/* TOS-1 slot.  Net delta 0 (pop 1, push 1; in-place rewrite).            */
/*                                                                        */
/* Layout (~67 bytes):                                                    */
/*    inc  dword [r13+20]      ; pc++                          4          */
/*    mov  rdi, [r12-16]       ; child.lo                      5          */
/*    mov  rsi, [r12-8]        ; child.hi                      5          */
/*    mov  rdx, imm64(vn)      ; arg2 (string ptr)            10          */
/*    mov  rcx, imm64(kind)    ; arg3 (int64)                 10          */
/*    aligned-call me10_pat_capture                           20          */
/*    mov  [r12-16], rax       ; result.lo                     5          */
/*    mov  [r12-8],  rdx       ; result.hi                     5          */
/*    jmp  rel32 trampoline                                    5          */
/*                                                          = 69          */
/*------------------------------------------------------------------------*/
static void emit_me10_pat_capture_blob(const char *vn, int64_t kind, size_t trampoline_abs_off)
{
    /* pc++                            (41 ff 45 14)                4 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* Load child (TOS) into rdi:rsi (first DESCR_t arg)               */
    /* mov rdi, [r12-16]               (49 8b 7c 24 f0)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x7c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0);
    /* mov rsi, [r12-8]                (49 8b 74 24 f8)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8);

    /* mov rdx, imm64(vn)              (48 ba <imm64>)              10 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xba);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)vn);

    /* mov rcx, imm64(kind)            (48 b9 <imm64>)              10 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xb9);
    seg_u64(SEG_CODE, (uint64_t)kind);

    /* aligned call me10_pat_capture                              20 */
    emit_aligned_call_imm64((void *)&me10_pat_capture);

    /* Write result at TOS-1 slot (in-place rewrite, net delta 0)      */
    /* mov [r12-16], rax               (49 89 44 24 f0)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0);
    /* mov [r12-8], rdx                (49 89 54 24 f8)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8);

    emit_jmp_trampoline(trampoline_abs_off);                       /* 5 */
}

/*------------------------------------------------------------------------*/
/* ME-10 — SM_PAT_CAPTURE_FN.                                             */
/*                                                                        */
/* Pop child into rdi:rsi; bake fname (rdx imm64), is_imm (rcx imm64),   */
/* namelist (r8 imm64); call me10_pat_capture_fn; write result at TOS-1. */
/* Net delta 0.                                                            */
/*                                                                        */
/* Layout (~79 bytes) — same as capture_blob plus one more imm64 arg.    */
/*------------------------------------------------------------------------*/
static void emit_me10_pat_capture_fn_blob(const char *fname, int64_t is_imm,
                                          const char *namelist,
                                          size_t trampoline_abs_off)
{
    /* pc++                            (41 ff 45 14)                4 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* Load child (TOS) into rdi:rsi                                    */
    /* mov rdi, [r12-16]               (49 8b 7c 24 f0)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x7c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0);
    /* mov rsi, [r12-8]                (49 8b 74 24 f8)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8);

    /* mov rdx, imm64(fname)           (48 ba <imm64>)              10 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xba);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)fname);

    /* mov rcx, imm64(is_imm)          (48 b9 <imm64>)              10 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xb9);
    seg_u64(SEG_CODE, (uint64_t)is_imm);

    /* mov r8, imm64(namelist)         (49 b8 <imm64>)              10 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0xb8);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)namelist);

    /* aligned call me10_pat_capture_fn                            20 */
    emit_aligned_call_imm64((void *)&me10_pat_capture_fn);

    /* Write result at TOS-1 slot                                       */
    /* mov [r12-16], rax               (49 89 44 24 f0)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x44); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0);
    /* mov [r12-8], rdx                (49 89 54 24 f8)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x54); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8);

    emit_jmp_trampoline(trampoline_abs_off);                       /* 5 */
}

/*------------------------------------------------------------------------*/
/* ME-11 — SM_EXEC_STMT.                                                  */
/*                                                                        */
/* Stack (top-to-bottom at entry): repl, subj, pat (3 DESCR_t = 48 B).  */
/* Pass r12 (TOS ptr) + baked sn + baked has_repl to me11_exec_stmt;     */
/* helper reads the 3 slots, calls exec_stmt, returns ok (int in eax).   */
/*                                                                        */
/* CRITICAL: exec_stmt can recurse into the SM interpreter via            */
/* _usercall_hook (pattern *func() calls), capture-fn callbacks, etc.    */
/* Those downstream paths read/write STATE->stack via STATE->sp.  We    */
/* MUST sync r12 → sp BEFORE the call so they see the current TOS,      */
/* and sync sp → r12 AFTER (recursion is balanced so sp_out == sp_in,   */
/* but defensively reload anyway).  The sp→r12 sync clobbers rax, so   */
/* the return value (eax) must be saved into r11 first; r11 is C-ABI    */
/* scratch and unused by sm-blob land.  Then pop 3 slots (sub r12, 48). */
/*                                                                        */
/* Layout (~93 bytes):                                                    */
/*    inc  dword [r13+20]          ; pc++                     4          */
/*    -- sync r12 → sp --                                    15          */
/*    mov  rdi, r12                ; arg1 = TOS ptr            3          */
/*    mov  rsi, imm64(sn)          ; arg2 = subject name      10          */
/*    mov  rdx, imm64(has_repl)    ; arg3 = has_repl flag     10          */
/*    aligned-call me11_exec_stmt                             20          */
/*    mov  r11d, eax               ; save return val          3          */
/*    -- sync sp → r12 --                                    15          */
/*    sub  r12, 48                 ; pop 3 slots               4          */
/*    mov  [r13+16], r11d          ; last_ok = saved result    4          */
/*    jmp  rel32 trampoline                                    5          */
/*                                                          = 93          */
/*------------------------------------------------------------------------*/
static void emit_me11_exec_stmt_blob(const char *sn, int64_t has_repl,
                                     size_t trampoline_abs_off)
{
    /* pc++                                    (41 ff 45 14)           4 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* ── sync r12 → st->sp ────────────────────────────────────────── 15
     *   mov rax, r12            (4c 89 e0)       3 bytes
     *   sub rax, [r13]          (49 2b 45 00)    4 bytes
     *   sar rax, 4              (48 c1 f8 04)    4 bytes
     *   mov [r13+8], eax        (41 89 45 08)    4 bytes
     */
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xe0);
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x2b);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x00);
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xf8); seg_byte(SEG_CODE, 0x04);
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x08);

    /* mov rdi, r12                            (4c 89 e7)              3 */
    seg_byte(SEG_CODE, 0x4c); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xe7);

    /* mov rsi, imm64(sn)                      (48 be <imm64>)        10 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xbe);
    seg_u64(SEG_CODE, (uint64_t)(uintptr_t)sn);

    /* mov rdx, imm64(has_repl)                (48 ba <imm64>)        10 */
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xba);
    seg_u64(SEG_CODE, (uint64_t)has_repl);

    /* aligned call me11_exec_stmt                                     20 */
    emit_aligned_call_imm64((void *)&me11_exec_stmt);

    /* Save return value (eax) in r11 before sp→r12 sync clobbers rax.
     * r11 is C-ABI scratch (caller-saved) — not used by sm-blob land.
     *   mov r11d, eax                          (41 89 c3)              3 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xc3);

    /* ── sync st->sp → r12 ────────────────────────────────────────── 15
     *   mov eax, [r13+8]        (41 8b 45 08)    4 bytes
     *   shl rax, 4              (48 c1 e0 04)    4 bytes
     *   add rax, [r13]          (49 03 45 00)    4 bytes
     *   mov r12, rax            (49 89 c4)       3 bytes
     *
     * Defensive reload — exec_stmt's recursion is balanced (sp_in ==
     * sp_out) so this normally re-derives the pre-call r12, but any
     * future change to balance discipline would silently corrupt r12
     * without this reload.
     */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x08);
    seg_byte(SEG_CODE, 0x48); seg_byte(SEG_CODE, 0xc1);
    seg_byte(SEG_CODE, 0xe0); seg_byte(SEG_CODE, 0x04);
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x03);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x00);
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x89); seg_byte(SEG_CODE, 0xc4);

    /* sub r12, 48   (pop 3 DESCR_t slots)     (49 83 ec 30)           4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xec); seg_byte(SEG_CODE, 0x30);

    /* mov [r13+16], r11d   (last_ok = saved result) (45 89 5d 10)     4 */
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x5d); seg_byte(SEG_CODE, 0x10);

    emit_jmp_trampoline(trampoline_abs_off);                       /* 5 */
}

/*------------------------------------------------------------------------*/
/* ME-12 — SM_BB_PUMP / SM_BB_ONCE.                                       */
/*                                                                        */
/* Both opcodes share an identical shape: pop one DESCR_t (tree_t* in     */
/* .ptr), call the runtime helper, write the int result to last_ok.      */
/* Net stack delta: -1.  Single emitter parameterized by helper fn.      */
/*                                                                        */
/* SM_SUSPEND_VALUE intentionally stays on emit_standard_blob fallback   */
/* (matching the ME-10 precedent for opcodes with conditional stack     */
/* delta): h_suspend_value's no-coroutine path calls PUSH(v) which       */
/* mutates STATE->stack, so any inline blob would need the r12<->sp    */
/* sync protocol — same code size and call shape as standard_blob.     */
/*                                                                        */
/* NO r12<->sp sync needed: bb_broker does not touch STATE->stack       */
/* itself; the _usercall_hook recursion path uses a separate nested    */
/* SM_State; coro adapters (bb_eval_value / bb_exec_stmt) don't touch  */
/* STATE->sp.  Contrast ME-11 SM_EXEC_STMT which DOES need sync         */
/* because exec_stmt mutates the caller's STATE->stack.                */
/*                                                                        */
/* Layout (47 bytes):                                                     */
/*    inc  dword [r13+20]      ; pc++                          4          */
/*    mov  rdi, [r12-16]       ; arg.lo (DESCR_t v+slen)       5          */
/*    mov  rsi, [r12-8]        ; arg.hi (DESCR_t union)        5          */
/*    aligned-call helper      ; returns int in eax           20          */
/*    sub  r12, 16             ; pop 1 DESCR_t                 4          */
/*    mov  [r13+16], eax       ; last_ok = result              4          */
/*    jmp  rel32 trampoline                                    5          */
/*                                                          = 47          */
/*------------------------------------------------------------------------*/
static void emit_me12_bb_blob(void *helper_fn, size_t trampoline_abs_off)
{
    /* pc++                            (41 ff 45 14)                4 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0xff);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x14);

    /* Load TOS DESCR_t into rdi:rsi (first 16-byte struct arg per SysV ABI) */
    /* mov rdi, [r12-16]               (49 8b 7c 24 f0)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x7c); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf0);
    /* mov rsi, [r12-8]                (49 8b 74 24 f8)              5 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x8b);
    seg_byte(SEG_CODE, 0x74); seg_byte(SEG_CODE, 0x24); seg_byte(SEG_CODE, 0xf8);

    /* aligned call helper (me12_bb_pump or me12_bb_once)            20 */
    emit_aligned_call_imm64(helper_fn);

    /* sub r12, 16   (pop 1 DESCR_t)   (49 83 ec 10)                 4 */
    seg_byte(SEG_CODE, 0x49); seg_byte(SEG_CODE, 0x83);
    seg_byte(SEG_CODE, 0xec); seg_byte(SEG_CODE, 0x10);

    /* mov [r13+16], eax   (last_ok = result)  (41 89 45 10)         4 */
    seg_byte(SEG_CODE, 0x41); seg_byte(SEG_CODE, 0x89);
    seg_byte(SEG_CODE, 0x45); seg_byte(SEG_CODE, 0x10);

    emit_jmp_trampoline(trampoline_abs_off);                       /* 5 */
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
     * rel32 patch offset and target_pc.  Buffer sized 2x prog->count
     * because ME-5 cond-jumps (SM_JUMP_S/SM_JUMP_F) push 2 entries each
     * (one for the target arm, one for the fall-through arm); SM_JUMP
     * pushes 1.  Worst case: every instruction is a cond-jump → 2x
     * entries. */
    int *jump_indices = NULL;
    size_t *jump_rel32_offs = NULL;
    int    jump_count = 0;
    jump_indices    = (int    *)calloc((size_t)prog->count * 2, sizeof(int));
    jump_rel32_offs = (size_t *)calloc((size_t)prog->count * 2, sizeof(size_t));
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
            /* EM-MODE4-IS-MODE3-DUMP-c (sess 2026-05-11): SM_HALT
             * routed through the per-opcode template
             * (templates/sm_halt.c) via the binary-emitter capture-
             * and-flush adapter.  Single-line revert if needed:
             * replace with `emit_halt_blob();`. */
            emit_halt_blob_via_template();
        } else if (op == SM_JUMP) {
            int32_t target_pc = (int32_t)prog->instrs[i].a[0].i;
            size_t rel32_off = emit_jump_blob_skeleton(target_pc);
            jump_indices[jump_count]    = i;
            jump_rel32_offs[jump_count] = rel32_off;
            jump_count++;
        } else if (op == SM_JUMP_S || op == SM_JUMP_F) {
            /* ME-5: inline-native conditional jump.  Reads st->last_ok via
             * [r13+16] (no PLT call needed since r13 = &SM_State).  Direct
             * rel32 to both target and fall-through — bypasses the
             * trampoline indirect-jump dispatch for both arms.
             *
             * Two rel32 patches per blob:
             *   - target: first rel32 (offset returned by skeleton)
             *   - fallthru: second rel32 (deterministic at first_off + 13)
             *
             * Pass 2 patches both.  We record each as a separate jump
             * entry; the patcher distinguishes by stuffing a tag in the
             * upper bits of jump_indices[] — no, cleaner: record both as
             * positive entries with i = (PC of source blob) and an
             * auxiliary array `jump_is_fallthru[j]` that says which arm.
             * Simpler still: emit two records into the existing arrays,
             * each with its own (rel32_off, target_pc).  We extend
             * jump_indices to carry the literal target_pc to use, not the
             * source-PC index. */
            int take_on_nonzero = (op == SM_JUMP_S) ? 1 : 0;
            int32_t target_pc   = (int32_t)prog->instrs[i].a[0].i;
            int32_t fallthru_pc = i + 1;
            size_t target_rel32_off  = emit_cond_jump_blob_skeleton(
                                          take_on_nonzero, target_pc, fallthru_pc);
            size_t fallthru_rel32_off = target_rel32_off + 13;

            /* Record both patch sites.  jump_indices[j] is repurposed to
             * carry the destination_pc directly (not the source-PC index)
             * for ME-5 cond-jump entries.  We tag this by storing
             * (-(target_pc + 1)) in jump_indices to distinguish from the
             * SM_JUMP entries (which store the source-PC index i >= 0).
             * Pass 2 detects the tag and reads the target from the
             * record directly. */
            jump_indices[jump_count]    = -(target_pc + 1);   /* tagged: target = -ji - 1 */
            jump_rel32_offs[jump_count] = target_rel32_off;
            jump_count++;
            jump_indices[jump_count]    = -(fallthru_pc + 1);
            jump_rel32_offs[jump_count] = fallthru_rel32_off;
            jump_count++;
        } else if (op == SM_LABEL) {
            /* ME-14: TRULY stackless no-op blob (pc++ + jmp trampoline).
             * SM_LABEL's handler h_label is a literal no-op; emitting a
             * C call + sp→r12 reload was destroying r12 growth from
             * preceding inline-native blobs.  See emit_label_blob docstring. */
            emit_label_blob(g_trampoline_offset);
        } else if (op == SM_STNO) {
            /* ME-5: stack-resetting handler — h_stno explicitly writes
             * st->sp = 0, so the post-sync reload of r12 from the fresh
             * sp value is correct and intentional. */
            emit_standard_blob_no_stack(g_handlers[op], g_trampoline_offset);
        } else if (op == SM_DEFINE_ENTRY) {
            /* ME-6a: conditional prologue blob.  Reads STATE->jit_in_call
             * ([r13+28]); does `push rbp; mov rbp, rsp` only when flag is 1
             * (entered via h_call), then clears the flag.  Goto paths that
             * land on the define-entry SM_LABEL and advance to SM_DEFINE_ENTRY
             * see jit_in_call==0 and skip the prologue entirely. */
            emit_me6_define_entry_blob(g_trampoline_offset);
        } else if (op == SM_RETURN   || op == SM_FRETURN  || op == SM_NRETURN  ||
                   op == SM_RETURN_S || op == SM_RETURN_F ||
                   op == SM_FRETURN_S || op == SM_FRETURN_F ||
                   op == SM_NRETURN_S || op == SM_NRETURN_F) {
            /* ME-6a: return-variant blobs.  Call me6_return_dispatch with
             * variant bits; conditionally pop rbp if jit_epilogue_pending. */
            int bits = 0;
            if (op == SM_FRETURN   || op == SM_FRETURN_S || op == SM_FRETURN_F) bits |= 1;
            if (op == SM_NRETURN   || op == SM_NRETURN_S || op == SM_NRETURN_F) bits |= 2;
            if (op == SM_RETURN_S  || op == SM_FRETURN_S || op == SM_NRETURN_S) bits |= 4;
            if (op == SM_RETURN_F  || op == SM_FRETURN_F || op == SM_NRETURN_F) bits |= 8;
            emit_me6_return_blob(bits, g_trampoline_offset);
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
        } else if (op == SM_PAT_LIT) {
            /* ME-9b: inline-native pat_lit.  String baked as imm64 from
             * a[0].s; pat_lit null-guards internally so no extra check. */
            emit_me9_pat_lit_blob(prog->instrs[i].a[0].s, g_trampoline_offset);
        } else if (op == SM_PAT_ANY || op == SM_PAT_NOTANY ||
                   op == SM_PAT_SPAN || op == SM_PAT_BREAK) {
            /* ME-9c: inline-native charset constructors.  Pop one DESCR_t,
             * coerce via VARVAL_fn (in me9_* helper), call pat constructor,
             * write result back.  Net delta 0. */
            void *helper = NULL;
            switch (op) {
            case SM_PAT_ANY:    helper = (void *)&me9_pat_any;    break;
            case SM_PAT_NOTANY: helper = (void *)&me9_pat_notany; break;
            case SM_PAT_SPAN:   helper = (void *)&me9_pat_span;   break;
            case SM_PAT_BREAK:  helper = (void *)&me9_pat_break;  break;
            default: break;
            }
            emit_me9_pat_charset_blob(helper, g_trampoline_offset);
        } else if (op == SM_PAT_LEN  || op == SM_PAT_POS  || op == SM_PAT_RPOS ||
                   op == SM_PAT_TAB  || op == SM_PAT_RTAB) {
            /* ME-9d: inline-native integer-arg constructors.  Pop one
             * DESCR_t, helper checks v==DT_I (else 0), calls pat
             * constructor.  Same blob shape as ME-9c (single-DESCR_t-arg,
             * net delta 0) — reuses emit_me9_pat_charset_blob. */
            void *helper = NULL;
            switch (op) {
            case SM_PAT_LEN:  helper = (void *)&me9_pat_len;  break;
            case SM_PAT_POS:  helper = (void *)&me9_pat_pos;  break;
            case SM_PAT_RPOS: helper = (void *)&me9_pat_rpos; break;
            case SM_PAT_TAB:  helper = (void *)&me9_pat_tab;  break;
            case SM_PAT_RTAB: helper = (void *)&me9_pat_rtab; break;
            default: break;
            }
            emit_me9_pat_charset_blob(helper, g_trampoline_offset);
        } else if (op == SM_PAT_ARBNO || op == SM_PAT_FENCE1) {
            /* ME-9e: inline-native unary-pattern combinators.  Pop one
             * DESCR_t (inner pat), call pat_arbno(inner) or
             * pat_fence_p(inner) directly — both have the exact
             * DESCR_t(DESCR_t) signature with no coercion or guard
             * needed.  Same blob shape as ME-9c/ME-9d (single-DESCR_t-arg,
             * net delta 0) — reuses emit_me9_pat_charset_blob.  No me9_*
             * helper indirection because the runtime entry points already
             * match the signature exactly. */
            void *fn = NULL;
            switch (op) {
            case SM_PAT_ARBNO:  fn = (void *)&pat_arbno;    break;
            case SM_PAT_FENCE1: fn = (void *)&pat_fence_p;  break;
            default: break;
            }
            emit_me9_pat_charset_blob(fn, g_trampoline_offset);
        } else if (op == SM_PAT_CAT || op == SM_PAT_ALT) {
            /* ME-9f: inline-native binary-pattern combinators.  Pop
             * right (TOS) and left (TOS-1), call pat_cat(left,right) or
             * pat_alt(left,right) directly — both have signature
             * DESCR_t(DESCR_t,DESCR_t) so no me9_* helper needed.
             * Net delta −1 (pop 2, push 1). */
            void *fn = NULL;
            switch (op) {
            case SM_PAT_CAT: fn = (void *)&pat_cat; break;
            case SM_PAT_ALT: fn = (void *)&pat_alt; break;
            default: break;
            }
            emit_me9_pat_binary_blob(fn, g_trampoline_offset);
        } else if (op == SM_PAT_DEREF) {
            /* ME-9g: variable-as-pattern.  Pop one DESCR_t, helper
             * (me9_pat_deref) does the 3-way DT_P/DT_S/else dispatch
             * (DT_P pass-through, DT_S → pat_lit, else pat_ref(VARVAL_fn)).
             * Keeping the type-discrimination in C rather than x86 is
             * the explicit Group G design choice. Net delta 0 — reuses
             * emit_me9_pat_charset_blob (single-DESCR_t-arg). */
            emit_me9_pat_charset_blob((void *)&me9_pat_deref, g_trampoline_offset);
        } else if (op == SM_PAT_REFNAME) {
            /* ME-9g: *var in pattern context — build XDSAR from the
             * literal name baked in a[0].s.  pat_ref null-guards
             * internally.  No stack args; net delta +1.  Same shape as
             * emit_me9_pat_lit_blob (different imm64 call target). */
            emit_me9_pat_refname_blob(prog->instrs[i].a[0].s, g_trampoline_offset);
        } else if (op == SM_PAT_CAPTURE) {
            /* ME-10: . / $ variable capture.  Pop child, build assign
             * pattern from baked vn + kind.  Net delta 0. */
            emit_me10_pat_capture_blob(
                prog->instrs[i].a[0].s,
                prog->instrs[i].a[1].i,
                g_trampoline_offset);
        } else if (op == SM_PAT_CAPTURE_FN) {
            /* ME-10: . *func() / $ *func() — call-cap pattern.  Pop
             * child, build XCALLCAP from baked fname/is_imm/namelist.
             * Net delta 0. */
            emit_me10_pat_capture_fn_blob(
                prog->instrs[i].a[0].s,
                prog->instrs[i].a[1].i,
                prog->instrs[i].a[2].s,
                g_trampoline_offset);
        } else if (op == SM_PAT_USERCALL) {
            /* ME-10: bare *func() in pattern context (no child wrapper).
             * Build XATP deferred-usercall.  Net delta +1.  Same blob
             * shape as emit_me9_pat_refname_blob (imm64 string arg,
             * push 1) with me10_pat_usercall as call target. */
            emit_me10_pat_usercall_blob(prog->instrs[i].a[0].s, g_trampoline_offset);
        /* SM_PAT_CAPTURE_FN_ARGS, SM_PAT_USERCALL_ARGS — variadic, runtime-
         * determined nargs.  Stay on emit_standard_blob fallback: the
         * C handlers already manipulate STATE->stack via the sync
         * protocol, and an inline blob would still have to sync r12↔sp
         * around the call.  No correctness or speed win to inlining. */
        } else if (op == SM_PAT_ARB     || op == SM_PAT_REM    ||
                   op == SM_PAT_FAIL    || op == SM_PAT_SUCCEED ||
                   op == SM_PAT_EPS     || op == SM_PAT_FENCE  ||
                   op == SM_PAT_ABORT   || op == SM_PAT_BAL) {
            /* ME-9a: inline-native pattern primitives (nullary).  Each
             * calls pat_X(void) via imm64 and pushes the DT_P result on
             * r12.  No args consumed; net delta +1.  Replaces the
             * emit_standard_blob fallback for these eight opcodes. */
            void *rt_fn = NULL;
            switch (op) {
            case SM_PAT_ARB:     rt_fn = (void *)&pat_arb;     break;
            case SM_PAT_REM:     rt_fn = (void *)&pat_rem;     break;
            case SM_PAT_FAIL:    rt_fn = (void *)&pat_fail;    break;
            case SM_PAT_SUCCEED: rt_fn = (void *)&pat_succeed; break;
            case SM_PAT_EPS:     rt_fn = (void *)&pat_epsilon; break;
            case SM_PAT_FENCE:   rt_fn = (void *)&pat_fence;   break;
            case SM_PAT_ABORT:   rt_fn = (void *)&pat_abort;   break;
            case SM_PAT_BAL:     rt_fn = (void *)&pat_bal;     break;
            default: break;
            }
            emit_me9_pat_nullary_blob(rt_fn, g_trampoline_offset);
        } else if (op == SM_EXEC_STMT) {
            /* ME-11: inline-native statement execution boundary.
             * Pop pat (TOS-2), subj (TOS-1), repl (TOS) from r12 stack;
             * bake sn (a[0].s) and has_repl (a[1].i) as imm64 args;
             * call me11_exec_stmt which forwards to exec_stmt.
             * Net delta: -3 (pop 3 DESCR_t slots). */
            emit_me11_exec_stmt_blob(
                prog->instrs[i].a[0].s,
                prog->instrs[i].a[1].i,
                g_trampoline_offset);
        } else if (op == SM_BB_PUMP || op == SM_BB_ONCE) {
            /* ME-12: inline-native Byrd-box broker drive.  Pop one DESCR_t
             * (tree_t* in .ptr), call me12_bb_pump or me12_bb_once which
             * forwards to coro_eval + bb_broker.  Net delta: -1.
             * SM_SUSPEND_VALUE stays on emit_standard_blob fallback —
             * see emit_me12_bb_blob comment for rationale. */
            void *helper = (op == SM_BB_PUMP) ? (void *)&me12_bb_pump
                                              : (void *)&me12_bb_once;
            emit_me12_bb_blob(helper, g_trampoline_offset);
        } else {
            emit_standard_blob(g_handlers[op], g_trampoline_offset);
        }
        /* Variable size — no equality check, just continue.  Each emit_*
         * helper writes its own self-consistent byte count. */
    }
    g_blob_count = prog->count;

    /* Pass 2 — patch SM_JUMP and ME-5 cond-jump rel32 displacements.
     *
     *   jump_indices[j] >= 0  → SM_JUMP entry; source pc = jump_indices[j];
     *                            target_pc read from prog->instrs[i].a[0].i.
     *   jump_indices[j] < 0   → ME-5 cond-jump entry (tagged);
     *                            target_pc = -jump_indices[j] - 1.
     *
     * target_blob_addr is g_blob_addrs[target_pc] (absolute address).  The
     * rel32 is relative to the byte AFTER the rel32 in SEG_CODE = base +
     * rel32_off + 4. */
    uint8_t *seg_base = scrip_segs[SEG_CODE].base;
    for (int j = 0; j < jump_count; j++) {
        int    ji        = jump_indices[j];
        size_t rel32_off = jump_rel32_offs[j];
        int32_t target_pc;
        int     source_pc;
        if (ji >= 0) {
            source_pc = ji;
            target_pc = (int32_t)prog->instrs[ji].a[0].i;
        } else {
            source_pc = -1;  /* tagged cond-jump record — no source-pc context */
            target_pc = -ji - 1;
        }
        if (target_pc < 0 || target_pc >= prog->count) {
            /* Out-of-range target — pass 2 silently leaves rel32=0; this
             * was already broken before ME-3 (the handler would set pc
             * to a bogus value).  Diagnostic message preserves the trail. */
            fprintf(stderr, "sm_codegen: jump at pc=%d targets out-of-range pc=%d (count=%d)\n",
                    source_pc, target_pc, prog->count);
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

/* sm_jit_unwind_call_stack — called by sm_run_with_recovery after a longjmp
 * error recovery in jit-run mode.
 *
 * Problem: sno_runtime_error longjmps out of C stack frames including
 * exec_stmt and any C function called from a JIT blob.  The C stack is
 * unwound by longjmp, but the JIT call stack (STATE->call_stack /
 * STATE->call_depth) lives in the SM_State heap struct and survives the
 * longjmp intact.  If left in place across the recovery, later
 * SM_RETURN blobs will pop stale frames and restore wrong caller_sp
 * values, leading to corrupted r12 / negative sp / SIGSEGV.
 *
 * Fix: unwind the entire JIT call stack on error recovery, restoring
 * saved NV slots so the global name-value table is consistent.  Mirrors
 * SNOBOL4 semantics: a runtime error aborts the current statement and
 * all active function invocations; execution resumes at top level.
 */
void sm_jit_unwind_call_stack(SM_State *st)
{
    while (st->call_depth > 0) {
        SmCallFrame *fr = &st->call_stack[--st->call_depth];
        for (int k = fr->nsaved - 1; k >= 0; k--)
            NV_SET_fn(fr->saved_names[k], fr->saved_vals[k]);
    }
    st->sp = 0;
}

int sm_jit_run(SM_Program *prog, SM_State *st)
{
    g_jit_prog   = prog;
    g_jit_state  = st;
    g_jit_halted = 0;

    if (prog->count == 0) return 0;

    /* ME-4-post: pre-grow value stack to a safe high-water capacity.
     * Mode-3 inline blobs write to state->stack[sp] without bounds
     * checks (mode-2's sm_push() realloc-grows on demand; mode-3
     * doesn't).  Mode-2's contract at sm_interp.c:295 resets
     * st->sp = 0 at every SM_STNO; mode-3 must replicate that (see
     * h_stno below).  Combining sp-reset-per-statement with a
     * generous initial cap means intra-statement depth is bounded by
     * the deepest single-statement expression, which 4096 entries
     * comfortably accommodates for every known corpus program.  If a
     * future program exceeds it, the right fix is per-blob bounds
     * check + slow-path, not a larger initial cap.  Cheap
     * defence-in-depth either way. */
    {
        const int ME4_STACK_PREGROW = 4096;
        if (st->stack_cap < ME4_STACK_PREGROW) {
            st->stack     = (DESCR_t *)realloc(st->stack, ME4_STACK_PREGROW * sizeof(DESCR_t));
            if (!st->stack) { fprintf(stderr, "sm_jit_run: stack pregrow OOM\n"); abort(); }
            st->stack_cap = ME4_STACK_PREGROW;
        }
        st->sp = 0;
    }

    /* Compute trampoline entry point */
    uint8_t *tramp = scrip_segs[SEG_CODE].base + g_trampoline_offset;
    typedef void (*entry_fn_t)(void);
    entry_fn_t entry = (entry_fn_t)tramp;

    /* ME-4-post-r12-tos: load BOTH callee-saved registers per the locked
     * REGISTER-LAYOUT.md convention before transferring control to SEG_CODE.
     *
     *   r12 = SM value-stack TOS pointer (FORTH-style: points to the slot
     *         where the NEXT push goes — i.e. one past the top live entry).
     *         At entry, st->sp = 0, so r12 = st->stack (the base).
     *   r13 = &SM_State (the new claim justified by ME-4-post-r12-tos —
     *         needed for cheap PC access in the trampoline and for stack
     *         base reload from [r13] in SM_STNO sync).
     *
     * Both registers are callee-saved per System V ABI, so the surrounding
     * C frame (this function) saves/restores them for us.  Any C handler
     * called from inside a blob also preserves both across its return.
     *
     * The trampoline reads st->pc as [r13+20] (offsetof SM_State.pc),
     * looks up blob[pc] by indirect jump through g_blob_addrs[], and jmps
     * in.  Each blob increments pc (or sets it for SM_JUMP), runs, jmps
     * back to the trampoline.  SM_HALT's blob `ret`s out — that's how
     * this function returns.
     *
     * Standard blobs (C handler dispatch) sync st->sp from r12 before
     * the C call (handler reads stack via STATE->stack[STATE->sp]) and
     * reload r12 from st->stack + st->sp*16 after the call returns.  This
     * keeps the FORTH discipline at the SM-blob boundary while the C
     * handlers run with the mode-2 calling convention.
     *
     * me4_* blobs (inline arithmetic / string / var ops) are pure value-
     * in / value-out — they don't read or write st->sp at all, so no
     * sync is needed; the blob just uses r12 directly for [r12-16]
     * (TOS) / [r12-32] (TOS-1) / push at [r12]+add r12,16. */
    asm volatile ("mov %0, %%r13\n\t"
                  "mov %1, %%r12"
                  :
                  : "r"(st), "r"(st->stack)
                  : "r12", "r13");
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
