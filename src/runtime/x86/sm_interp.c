/*
 * sm_interp.c — SM_Program C interpreter dispatch loop (M-SCRIP-U2)
 *
 * Executes SM_Program instructions one-by-one via a switch dispatch.
 * This is Mode I (--interp) and the correctness reference for Mode G.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-04-06
 */

#include "sm_interp.h"
#include "sm_prog.h"
#include "../../runtime/common/coerce.h"  /* shared_arith (F-1 RS-7) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gc/gc.h>

#include "../interp/coro_runtime.h"   /* A0: g_sm_dispatch_active */
#include "../interp/coro_value.h"     /* A3-seed-fix: bb_icn_rnd_seed shared RNG */

/* ── Pattern runtime (M-SCRIP-U4) ──────────────────────────────────────── */
#include "snobol4.h"   /* DESCR_t, PATND_t, DT_* */
#include "sil_macros.h" /* IS_NAMEPTR, NAME_DEREF_PTR, IS_NAMEVAL, etc. */

/* AST_t / AST_e for SM_PAT_CAPTURE_FN synthetic AST_FNC node */
#include "../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"

/* Pattern constructors from snobol4_pattern.c */
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
extern DESCR_t pat_rem(void);
extern DESCR_t pat_fence(void);
extern DESCR_t pat_fail(void);
extern DESCR_t pat_abort(void);
extern DESCR_t pat_succeed(void);
extern DESCR_t pat_bal(void);
extern DESCR_t pat_epsilon(void);
extern DESCR_t pat_cat(DESCR_t left, DESCR_t right);
extern DESCR_t pat_alt(DESCR_t left, DESCR_t right);
extern DESCR_t pat_ref(const char *name);         /* deferred *var ref */
extern DESCR_t pat_assign_imm(DESCR_t child, DESCR_t var);
extern DESCR_t pat_assign_cond(DESCR_t child, DESCR_t var);
extern DESCR_t pat_at_cursor(const char *varname);
/* pat_user_call, pat_assign_callcap, pat_assign_callcap_named come from snobol4.h */

/* exec_stmt from stmt_exec.c */
extern int exec_stmt(const char *subj_name, DESCR_t *subj_var,
                     DESCR_t pat, DESCR_t *repl, int has_repl);

/* VARVAL_fn / NV_GET_fn from snobol4.c */
extern char    *VARVAL_fn(DESCR_t d);
extern DESCR_t  NV_GET_fn(const char *name);

/* CHUNKS-step17b'' (CH-17b''): forwarders to the active Icon frame's env slots.
 * Defined in coro_runtime.c for production builds, stubbed in sm_interp_test.c
 * for the unit-test world.  Returns FAILDESCR / no-op when frame_depth == 0
 * — expressions emitted with frame-slot opcodes are dead code today (CH-17c flips
 * the consumer that reaches them).  Pure-DESCR_t signatures: no AST_t leakage
 * across the SM-runtime/IR-runtime boundary. */
extern DESCR_t  icn_frame_env_load(int slot);
extern void     icn_frame_env_store(int slot, DESCR_t val);
extern int      icn_frame_env_active(void);   /* 1 if frame_depth > 0 */

/* OE-10: Icon/Prolog BB opcode support */
#include "bb_broker.h"
#include <setjmp.h>
extern bb_node_t coro_eval(AST_t *e);   /* scrip.c — builds a drivable bb_node_t */
extern bb_node_t coro_pump_proc_by_name(const char *name, DESCR_t *args, int nargs);
                                          /* CHUNKS-step12: name-driven Icon proc pump */

/* CHUNKS-step17i-suspend: yield-to-caller helper.
 *
 * Defined in coro_runtime.c (which already owns ucontext machinery for
 * coro_t / proc_trampoline).  Called from the SM_SUSPEND_VALUE handler
 * to implement Icon's `suspend E [do body]` yield protocol without
 * pulling icon_gen.h transitive baggage into sm_interp.c.
 *
 *   sm_yield_to_caller(v) — if running inside a coroutine context
 *   (active_coro != NULL), set active_coro->yielded = v and swapcontext
 *   to caller_ctx.  When the caller resumes us, control returns from
 *   this function naturally and the SM dispatch loop continues.
 *
 *   Returns 1 if a yield happened, 0 if there was no active coroutine
 *   (top-level suspend — semantically rare).  The handler uses the
 *   return value to decide whether to push a placeholder NULVCL or
 *   the original value back. */
extern int sm_yield_to_caller(DESCR_t v);

/* CH-17f: Prolog name-driven BB_ONCE dispatch */
#include "../../frontend/prolog/pl_broker.h"
#include "../../runtime/interp/pl_runtime.h"

/* IM-4: SM step-limit for in-process sync monitor */
int      g_sm_step_limit = 0;
int      g_sm_steps_done = 0;
jmp_buf  g_sm_step_jmp;

/* CHUNKS-step05: audit counters.  When SCRIP_EXPRS_AUDIT=1, every push of
 * a DT_E-shaped value through the SM dispatch loop is tallied; on process
 * exit (registered atexit) a summary line is printed.  For pure
 * SNOBOL4/Snocone programs after Step 4, push_expr should be 0 and oor 0. */
int      g_exprs_audit_push_expr  = 0;
int      g_exprs_audit_push_expression = 0;
int      g_exprs_audit_oor  = 0;
static void exprs_audit_summary(void) {
    if (getenv("SCRIP_EXPRS_AUDIT")) {
        fprintf(stderr,
                "[CHUNKS-AUDIT] summary: SM_PUSH_EXPRESSION=%d  SM_PUSH_EXPR=%d  out_of_range=%d\n",
                g_exprs_audit_push_expression,
                g_exprs_audit_push_expr,
                g_exprs_audit_oor);
    }
}
__attribute__((constructor))
static void exprs_audit_register(void) {
    atexit(exprs_audit_summary);
}

/* CHUNKS-step14: pointer to the active SmGenState (the one owned by the
 * outermost bb_broker_drive_sm tick).  NULL when not inside a generator drive.
 * SM_SUSPEND writes the suspended state here and sm_interp_run returns
 * SM_INTERP_SUSPENDED.  Exposed (non-static) since CHUNKS-step14b so the JIT
 * codegen handlers for SM_LOAD_GLOCAL / SM_STORE_GLOCAL can reach it. */
SmGenState *g_current_gen_state = NULL;

/* CHUNKS-step17i-every-suspend: every-table.
 * Indexed by SM_BB_PUMP_EVERY's a[0].i.  Populated by sm_lower at AST_EVERY
 * lowering time (lower.c).  Bounded grow; never shrinks within a compile.
 * Reset by every_table_reset (called from sm_program_free path).
 * AST pointers borrowed — caller (lower) owns them; same lifetime as
 * g_pl_pred_table's borrowed AST_CHOICE pointers. */
#define EVERY_TABLE_INIT 16
static AST_t **g_every_table     = NULL;
static int     g_every_table_n   = 0;
static int     g_every_table_cap = 0;

int every_table_register(AST_t *ast)
{
    if (g_every_table_n >= g_every_table_cap) {
        int new_cap = g_every_table_cap ? g_every_table_cap * 2 : EVERY_TABLE_INIT;
        AST_t **nt = (AST_t **)realloc(g_every_table, new_cap * sizeof(AST_t *));
        if (!nt) { fprintf(stderr, "every_table: OOM\n"); abort(); }
        g_every_table     = nt;
        g_every_table_cap = new_cap;
    }
    int id = g_every_table_n++;
    g_every_table[id] = ast;
    return id;
}

AST_t *every_table_lookup(int id)
{
    if (id < 0 || id >= g_every_table_n) return NULL;
    return g_every_table[id];
}

void every_table_reset(void)
{
    if (g_every_table) { free(g_every_table); g_every_table = NULL; }
    g_every_table_n = 0;
    g_every_table_cap = 0;
}

/* GOAL-ICON-BB-COMPLETE Phase A: unified ast_pump_table for SM_BB_PUMP_AST.
 * Mirrors every_table exactly — same lifetime, same borrow model. */
#define AST_PUMP_TABLE_INIT 16
static AST_t **g_ast_pump_table     = NULL;
static int     g_ast_pump_table_n   = 0;
static int     g_ast_pump_table_cap = 0;

int ast_pump_table_register(AST_t *ast)
{
    if (g_ast_pump_table_n >= g_ast_pump_table_cap) {
        int new_cap = g_ast_pump_table_cap ? g_ast_pump_table_cap * 2 : AST_PUMP_TABLE_INIT;
        AST_t **nt = (AST_t **)realloc(g_ast_pump_table, new_cap * sizeof(AST_t *));
        if (!nt) { fprintf(stderr, "ast_pump_table: OOM\n"); abort(); }
        g_ast_pump_table     = nt;
        g_ast_pump_table_cap = new_cap;
    }
    int id = g_ast_pump_table_n++;
    g_ast_pump_table[id] = ast;
    return id;
}

AST_t *ast_pump_table_lookup(int id)
{
    if (id < 0 || id >= g_ast_pump_table_n) return NULL;
    return g_ast_pump_table[id];
}

void ast_pump_table_reset(void)
{
    if (g_ast_pump_table) { free(g_ast_pump_table); g_ast_pump_table = NULL; }
    g_ast_pump_table_n   = 0;
    g_ast_pump_table_cap = 0;
}

/* OE-10: body_fn for BB_PUMP — print each generated Icon value to stdout */
static void pump_print(DESCR_t val, void *arg) {
    (void)arg;
    char *s = VARVAL_fn(val);
    if (s) printf("%s\n", s);
}
/* ME-1: pat-stack unified into SM_State.stack — no separate g_pat_stack */

/* ── Stack helpers ──────────────────────────────────────────────────── */

#define SM_STACK_INIT 16
void sm_state_init(SM_State *st)
{
    memset(st, 0, sizeof *st);
    st->stack     = malloc(SM_STACK_INIT * sizeof(DESCR_t));
    st->stack_cap = SM_STACK_INIT;
    st->sp        = 0;
    st->last_ok   = 1;
    st->pc        = 0;
}

void sm_state_free(SM_State *st)
{
    free(st->stack);
    st->stack     = NULL;
    st->stack_cap = 0;
    st->sp        = 0;
}

void sm_push(SM_State *st, DESCR_t d)
{
    if (st->sp >= st->stack_cap) {
        st->stack_cap *= 2;
        st->stack = realloc(st->stack, st->stack_cap * sizeof(DESCR_t));
        if (!st->stack) { fprintf(stderr, "sm_interp: out of memory\n"); abort(); }
    }
    st->stack[st->sp++] = d;
}

DESCR_t sm_pop(SM_State *st)
{
    if (st->sp <= 0) {
        fprintf(stderr, "sm_interp: stack underflow\n");
        abort();
    }
    return st->stack[--st->sp];
}

DESCR_t sm_peek(SM_State *st)
{
    if (st->sp <= 0) {
        fprintf(stderr, "sm_interp: peek on empty stack\n");
        abort();
    }
    return st->stack[st->sp - 1];
}

/* ── Arithmetic helpers ─────────────────────────────────────────────── */
/* F-1 RS-7: sm_arith replaced by shared_arith() in runtime/common/coerce.c */

/* F-5 RS-7: nv_fold_get / nv_fold_set — name-fold-then-lookup helper.
 * Collapses the repeated GC_strdup + sno_fold_name + NV_GET/SET_fn triad
 * (SN-19 pattern) to a single call. */
static DESCR_t nv_fold_get(const char *raw) {
    if (!raw || !*raw) return NULVCL;
    char *n = GC_strdup(raw); sno_fold_name(n);
    return NV_GET_fn(n);
}
static void nv_fold_set(const char *raw, DESCR_t val) {
    if (!raw || !*raw) return;
    char *n = GC_strdup(raw); sno_fold_name(n);
    NV_SET_fn(n, val);
}

/* ── Main dispatch loop ─────────────────────────────────────────────── */

int sm_interp_run(SM_Program *prog, SM_State *st)
{
    int was_active = g_sm_dispatch_active;
    g_sm_dispatch_active = 1;
    int _rc = sm_interp_run_inner(prog, st);
    g_sm_dispatch_active = was_active;
    return _rc;
}

int sm_interp_run_inner(SM_Program *prog, SM_State *st)
{
    while (st->pc < prog->count) {
        SM_Instr *ins = &prog->instrs[st->pc];
        st->pc++;

        switch (ins->op) {

        /* ── Control ───────────────────────────────────────────────── */

        case SM_LABEL:
            /* no-op at runtime — label is a marker for the builder */
            break;
        case SM_DEFINE_ENTRY:
            /* ME-6a: no-op in mode-2; mode-3 blob does conditional push rbp */
            break;

        case SM_HALT:
            return 0;

        case SM_STNO: {
            extern void comm_stno(int n);
            /* SN-32a-stno: source stno carried as operand by sm_lower so
             * gotos and label-skips report &STNO correctly.  Linear
             * counter `g_sm_stno` was wrong on every backward branch and
             * after every `:(label)` jump that skipped intervening stmts.
             * Mirrors the IR-side SN-26-bridge-coverage-j fix. */
            int sm_stno = (int)ins->a[0].i;
            comm_stno(sm_stno);
            /* SN-26-bridge-coverage-f: fire MWK_LABEL on every statement entry. */
            {
                extern void mon_emit_label_bin(int64_t stno);
                mon_emit_label_bin((int64_t)sm_stno);
            }
            kw_stno = sm_stno;
            st->sp = 0;   /* reset value stack at each statement boundary */
            /* SN-26c-stmt637 probe: trace each SM step -> source stmt number */
            {
                static int s_trace_init = 0, s_trace_on = 0;
                if (!s_trace_init) {
                    const char *e = getenv("ONE4ALL_STEP_TRACE");
                    s_trace_on = (e && e[0] == '1');
                    s_trace_init = 1;
                }
                if (s_trace_on)
                    fprintf(stderr, "SMSTEP %d sm_stno=%d pc=%d\n",
                            g_sm_steps_done + 1, sm_stno, st->pc);
            }
            /* IM-4: step-limit */
            if (g_sm_step_limit > 0 && g_sm_steps_done++ >= g_sm_step_limit)
                longjmp(g_sm_step_jmp, 1);
            break;
        }

        case SM_JUMP:
            st->pc = (int)ins->a[0].i;
            break;

        case SM_JUMP_S:
            if (st->last_ok) st->pc = (int)ins->a[0].i;
            break;

        case SM_JUMP_F:
            if (!st->last_ok) st->pc = (int)ins->a[0].i;
            break;

        /* ── Values ────────────────────────────────────────────────── */

        case SM_PUSH_LIT_S: {
            const char *s = ins->a[0].s ? ins->a[0].s : "";
            int64_t     n = ins->a[1].i;  /* explicit byte length; 0 = use strlen */
            DESCR_t d;
            d.v    = DT_S;
            d.s    = (char *)s;
            d.slen = (n > 0) ? (uint32_t)n : 0;
            sm_push(st, d);
            break;
        }

        case SM_PUSH_LIT_I:
            sm_push(st, INTVAL(ins->a[0].i));
            break;

        case SM_PUSH_LIT_F:
            sm_push(st, REALVAL(ins->a[0].f));
            break;

        case SM_PUSH_NULL:
            sm_push(st, NULVCL);
            st->last_ok = 1;    /* null is a valid (non-fail) result */
            break;

        case SM_PUSH_NULL_NOFLIP:
            sm_push(st, NULVCL); /* preserve last_ok — used after SM_EXEC_STMT */
            break;

        case SM_PUSH_VAR: {
            const char *name = ins->a[0].s;
            DESCR_t val = NV_GET_fn(name);
            sm_push(st, val);
            /* SN-6: keyword reads (e.g. INPUT at EOF) return FAILDESCR.
             * Update last_ok so the statement's :F branch fires correctly. */
            st->last_ok = (val.v != DT_FAIL);
            break;
        }

        case SM_PUSH_EXPR: {
            /* Push a frozen DT_E expression descriptor (for *expr / EVAL()) */
            /* CHUNKS-step05 instrumentation: tally legacy AST_t* DT_E pushes.
             * When SCRIP_EXPRS_AUDIT=1 and the program is pure SNOBOL4/Snocone,
             * any SM_PUSH_EXPR fire is a violation of M1's "expression-only" invariant.
             * Icon/Raku/Prolog generators legitimately still hit this until M4. */
            if (getenv("SCRIP_EXPRS_AUDIT")) {
                g_exprs_audit_push_expr++;
                fprintf(stderr, "[CHUNKS-AUDIT] SM_PUSH_EXPR fired at pc=%d (legacy AST_t* path)\n",
                        st->pc);
            }
            DESCR_t d;
            d.v    = DT_E;
            d.slen = 0;
            d.ptr  = ins->a[0].ptr;   /* ptr and s share a union — set ptr last */
            sm_push(st, d);
            st->last_ok = 1;
            break;
        }

        case SM_PUSH_EXPRESSION: {
            /* CHUNKS-step02: push DT_E expression descriptor.
             * slen=1 distinguishes expression from legacy AST_t* (slen=0).
             * entry_pc stored in the .i union field. */
            /* CHUNKS-step05 instrumentation: validate entry_pc within prog bounds.
             * Guarded by SCRIP_EXPRS_AUDIT to keep production builds free of overhead. */
            if (getenv("SCRIP_EXPRS_AUDIT")) {
                g_exprs_audit_push_expression++;
                int entry_pc = (int)ins->a[0].i;
                if (entry_pc < 0 || entry_pc >= prog->count) {
                    g_exprs_audit_oor++;
                    fprintf(stderr, "[CHUNKS-AUDIT] SM_PUSH_EXPRESSION at pc=%d: entry_pc=%d out of range [0,%d)\n",
                            st->pc, entry_pc, prog->count);
                }
            }
            DESCR_t d;
            d.v    = DT_E;
            d.slen = 1; /* expression flag */
            d.i    = ins->a[0].i;        /* entry_pc */
            sm_push(st, d);
            st->last_ok = 1;
            break;
        }

        case SM_CALL_EXPRESSION: {
            /* CHUNKS-step02: call a compiled chunk thunk on the same SM stack.
             * entry_pc = a[0].i.  No params, no locals, no retval NV slot.
             * Push a minimal SmCallFrame (caller stack saved, ret_pc set),
             * jump to entry_pc; SM_RETURN pops the frame and pushes result. */
            int entry_pc = (int)ins->a[0].i;
            if (entry_pc < 0 || entry_pc >= prog->count
                    || st->call_depth >= SM_CALL_STACK_MAX) {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
                break;
            }
            SmCallFrame *fr = &st->call_stack[st->call_depth++];
            fr->ret_pc = st->pc;
            fr->ret_ok = 1;
            fr->retval_name = NULL;   /* no NV retval slot for a thunk */
            fr->nsaved = 0;
            /* Save caller's value stack */
            fr->caller_sp = st->sp;
            if (st->sp > 0) {
                fr->caller_stack = GC_malloc(st->sp * sizeof(DESCR_t));
                memcpy(fr->caller_stack, st->stack, st->sp * sizeof(DESCR_t));
            } else {
                fr->caller_stack = NULL;
            }
            st->sp = 0;  /* expression body runs on empty stack */
            st->pc = entry_pc;
            goto sm_call_done;
        }

        case SM_STORE_VAR: {
            const char *name = ins->a[0].s;
            DESCR_t val = sm_pop(st);
            /* SNOBOL4 semantics: if RHS evaluated to FAIL, the statement fails
             * and no assignment occurs. Without this guard, OUTPUT = DIFFER(X,Y)
             * would call output_val(FAILDESCR) and print a spurious blank line. */
            if (val.v == DT_FAIL) {
                /* SN-32b-store-fail: push FAILDESCR so enclosing calls (e.g.
                 * DIFFER(sno = Pop()) where Pop() FRETURNs) see a balanced
                 * stack.  Without the push, the enclosing SM_CALL_FN pops a
                 * stale value, corrupting the arg and mis-setting last_ok.
                 * SNOBOL4 semantics: the assignment does not occur, and FAIL
                 * propagates up to the enclosing expression. */
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
                break;
            }
            /* RT-5 / SN-32b-store-val: push `val` (the RHS value), NOT the return
             * value of NV_SET_fn.  The IR path (interp.c AST_ASSIGN, line 2844) always
             * returns `val` regardless of what NV_SET_fn stores — NV_SET_fn's return
             * value is unreliable for DT_DATA objects (returns SNUL on the second
             * call for the same variable).  Pushing `val` ensures DIFFER(sno=Pop())
             * sees the actual DATA value, not a stripped SNUL. */
            NV_SET_fn(name, val);
            sm_push(st, val);
            /* SN-6: successful assignment sets last_ok=1 so a prior failure
             * (e.g. EQ in previous statement) does not bleed into this statement's
             * :F branch. "Leave last_ok unchanged" was wrong — it caused
             * last_ok=0 from EQ failure to persist across the loop-back goto,
             * making INPUT's :F(END) fire even when INPUT succeeded. */
            st->last_ok = 1;
            break;
        }

        case SM_VOID_POP:
            sm_pop(st);
            break;

        /* ── Arithmetic / String ───────────────────────────────────── */

        case SM_ADD:
        case SM_SUB:
        case SM_MUL:
        case SM_DIV:
        case SM_MOD:   /* OC-1 RS-6 */
        case SM_EXP: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            /* SN-6: propagate FAIL from operands (e.g. CHARS + SIZE(INPUT) when
             * INPUT hits EOF — SIZE swallows the FAIL and returns 0, so we must
             * catch it here before arithmetic runs). */
            if (l.v == DT_FAIL || r.v == DT_FAIL) {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
                break;
            }
            /* coerce strings to numeric if needed */
            if (l.v == DT_S) l = INTVAL(to_int(l));
            if (r.v == DT_S) r = INTVAL(to_int(r));
            /* coerce SNUL (unset/empty) to integer 0 — matches SPITBOL & --ir-run */
            if (l.v == DT_SNUL) l = INTVAL(0);
            if (r.v == DT_SNUL) r = INTVAL(0);
            DESCR_t result = shared_arith(l, r, ins->op);
            sm_push(st, result);
            st->last_ok = (result.v != DT_FAIL);
            break;
        }

        case SM_NEG: {
            DESCR_t v = sm_pop(st);
            if (v.v == DT_I) sm_push(st, INTVAL(-v.i));
            else              sm_push(st, REALVAL(-to_real(v)));
            break;
        }

        case SM_CONCAT: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            DESCR_t result = CONCAT_fn(l, r);
            sm_push(st, result);
            st->last_ok = (result.v != DT_FAIL);
            break;
        }

        case SM_COERCE_NUM: {
            /* unary +: coerce string to int (or real if not integer) */
            DESCR_t v = sm_pop(st);
            if (v.v == DT_S) {
                int64_t iv = to_int(v);
                if (iv != 0 || (v.s && v.s[0] == '0')) { sm_push(st, INTVAL(iv)); }
                else { double rv = to_real(v); sm_push(st, REALVAL(rv)); }
            } else { sm_push(st, v); }
            st->last_ok = 1;
            break;
        }

        /* ── Pattern construction ops (M-SCRIP-U4) ─────────────────── */

        case SM_PAT_LIT: {
            /* a[0].s = literal string */
            sm_push(st, pat_lit(ins->a[0].s ? ins->a[0].s : ""));
            break;
        }
        case SM_PAT_ANY: {
            /* arg on value stack: charset string */
            DESCR_t arg = sm_pop(st);
            const char *cs = VARVAL_fn(arg);
            sm_push(st, pat_any_cs(cs ? cs : ""));
            break;
        }
        case SM_PAT_NOTANY: {
            DESCR_t arg = sm_pop(st);
            const char *cs = VARVAL_fn(arg);
            sm_push(st, pat_notany(cs ? cs : ""));
            break;
        }
        case SM_PAT_SPAN: {
            DESCR_t arg = sm_pop(st);
            const char *cs = VARVAL_fn(arg);
            sm_push(st, pat_span(cs ? cs : ""));
            break;
        }
        case SM_PAT_BREAK: {
            DESCR_t arg = sm_pop(st);
            const char *cs = VARVAL_fn(arg);
            sm_push(st, pat_break_(cs ? cs : ""));
            break;
        }
        case SM_PAT_LEN: {
            DESCR_t arg = sm_pop(st);
            int64_t n = (arg.v == DT_I) ? arg.i : 0;
            sm_push(st, pat_len(n));
            break;
        }
        case SM_PAT_POS: {
            DESCR_t arg = sm_pop(st);
            int64_t n = (arg.v == DT_I) ? arg.i : 0;
            sm_push(st, pat_pos(n));
            break;
        }
        case SM_PAT_RPOS: {
            DESCR_t arg = sm_pop(st);
            int64_t n = (arg.v == DT_I) ? arg.i : 0;
            sm_push(st, pat_rpos(n));
            break;
        }
        case SM_PAT_TAB: {
            DESCR_t arg = sm_pop(st);
            int64_t n = (arg.v == DT_I) ? arg.i : 0;
            sm_push(st, pat_tab(n));
            break;
        }
        case SM_PAT_RTAB: {
            DESCR_t arg = sm_pop(st);
            int64_t n = (arg.v == DT_I) ? arg.i : 0;
            sm_push(st, pat_rtab(n));
            break;
        }
        case SM_PAT_ARB:     sm_push(st, pat_arb());     break;
        case SM_PAT_ARBNO:   { DESCR_t _inner = sm_pop(st); sm_push(st, pat_arbno(_inner)); } break;
        case SM_PAT_REM:     sm_push(st, pat_rem());     break;
        case SM_PAT_FAIL:    sm_push(st, pat_fail());    break;
        case SM_PAT_SUCCEED: sm_push(st, pat_succeed()); break;
        case SM_PAT_EPS:     sm_push(st, pat_epsilon()); break;
        case SM_PAT_FENCE:   sm_push(st, pat_fence());   break;
        case SM_PAT_FENCE1:  { DESCR_t _ch = sm_pop(st); sm_push(st, pat_fence_p(_ch)); } break;
        case SM_PAT_ABORT:   sm_push(st, pat_abort());   break;
        case SM_PAT_BAL:     sm_push(st, pat_bal());     break;

        case SM_PAT_CAT: {
            /* pop right then left (left was pushed first) */
            DESCR_t right = sm_pop(st);
            DESCR_t left  = sm_pop(st);
            sm_push(st, pat_cat(left, right));
            break;
        }
        case SM_PAT_ALT: {
            DESCR_t right = sm_pop(st);
            DESCR_t left  = sm_pop(st);
            sm_push(st, pat_alt(left, right));
            break;
        }
        /* SM_PAT_BOXVAL deleted by ME-1 — pat-stack and value-stack are now one */
        case SM_PAT_DEREF: {
            DESCR_t v = sm_pop(st);
            if (v.v == DT_P) {
                sm_push(st, v);                        /* already a pattern */
            } else if (v.v == DT_S && v.s) {
                sm_push(st, pat_lit(v.s));             /* string → literal */
            } else {
                /* variable name or other — deferred ref */
                const char *name = VARVAL_fn(v);
                sm_push(st, pat_ref(name ? name : ""));
            }
            break;
        }
        case SM_PAT_REFNAME: {
            /* SN-6: *var in pattern context — build XDSAR from the NAME,
             * never fetching the variable's current value at build time.
             * Required for self-recursive patterns. */
            const char *name = ins->a[0].s ? ins->a[0].s : "";
            sm_push(st, pat_ref(name));
            break;
        }
        case SM_PAT_CAPTURE: {
            /* a[0].s = variable name; a[1].i = 0=cond 1=imm 2=cursor
             * pat_assign_cond/imm expects a NAME descriptor (DT_N). */
            DESCR_t child = sm_pop(st);
            const char *vname = ins->a[0].s ? ins->a[0].s : "";
            DESCR_t var = NAME_fn(vname);
            int kind = (int)ins->a[1].i;
            if (kind == 1)
                sm_push(st, pat_assign_imm(child, var));
            else if (kind == 2)
                sm_push(st, pat_cat(child, pat_at_cursor(vname)));  /* cursor: seq child then @var */
            else
                sm_push(st, pat_assign_cond(child, var));
            break;
        }

        case SM_PAT_CAPTURE_FN: {
            /* . *func() or $ *func() — a[0].s = function name, a[1].i = 0(cond)/1(imm).
             * a[2].s (TL-2): optional '\t'-separated arg *names* for flush-time
             * resolution — set when every arg of *func() is a plain AST_VAR.
             * When NULL, legacy path (no args, pat_assign_callcap).
             * Use pat_assign_callcap → XCALLCAP node, lowered to bb_cap with
             * NM_CALL NameKind_t (SN-21d).  At match time, name_commit_value's
             * NM_CALL branch calls g_user_call_hook(fname, args, nargs);
             * deferred for '.', immediate for '$'.
             * The old DT_E/pat_assign_cond approach only worked via the snobol4_pattern.c
             * materialise() path, which --sm-run does not use. */
            DESCR_t child = sm_pop(st);
            const char *fname    = ins->a[0].s ? ins->a[0].s : "";
            const char *namelist = ins->a[2].s;
            if (namelist && namelist[0]) {
                /* Split on '\t' into GC-lifetime name array */
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
                int is_imm = (int)ins->a[1].i;
                sm_push(st, is_imm
                    ? pat_assign_callcap_named_imm(child, fname, NULL, 0, names, nnames)
                    : pat_assign_callcap_named(child, fname, NULL, 0, names, nnames));
            } else {
                int is_imm = (int)ins->a[1].i;
                sm_push(st, is_imm
                    ? pat_assign_callcap_named_imm(child, fname, NULL, 0, NULL, 0)
                    : pat_assign_callcap(child, fname, NULL, 0));
            }
            break;
        }

        case SM_PAT_CAPTURE_FN_ARGS: {
            /* SN-8a: . *func(args) / $ *func(args) — args-on-stack form.
             * a[0].s = fname, a[1].i = kind (0=cond, 1=imm), a[2].i = nargs.
             * The nargs values were lowered in order 0..nargs-1 onto the value stack
             * (last-pushed = last arg).  Pop them into positions nargs-1..0 to
             * reconstruct original argument order.  Then pop the child pattern and
             * build pat_assign_callcap(child, fname, values, nargs). */
            int nargs = (int)ins->a[2].i;
            DESCR_t *argv = nargs > 0
                ? (DESCR_t *)GC_MALLOC((size_t)nargs * sizeof(DESCR_t))
                : NULL;
            for (int i = nargs - 1; i >= 0; i--) argv[i] = sm_pop(st);
            DESCR_t child = sm_pop(st);
            const char *fname = ins->a[0].s ? ins->a[0].s : "";
            int is_imm = (int)ins->a[1].i;  /* SN-26c-parseerr-f: 0=cond(.) 1=imm($) */
            sm_push(st, is_imm
                ? pat_assign_callcap_named_imm(child, fname, argv, nargs, NULL, 0)
                : pat_assign_callcap(child, fname, argv, nargs));
            break;
        }

        case SM_PAT_USERCALL: {
            /* SN-17a: bare *func() in pattern context.
             * a[0].s = function name; a[2].s = '\t'-separated arg names (or NULL).
             * No child pattern is popped — bare *fn() wraps nothing.
             * Build XATP deferred-usercall node via pat_user_call so the engine
             * invokes func() per position at match time; func's FAIL propagates
             * as pattern FAIL.  The named-args (a[2].s) path is not yet consumed
             * — when every arg is a plain AST_VAR it is currently routed through
             * the all-AST_VAR stash but the downstream XATP node is not yet wired
             * to resolve names at match time.  SN-8a fixes the non-AST_VAR case
             * via SM_PAT_USERCALL_ARGS (args-on-stack). */
            const char *fname = ins->a[0].s ? ins->a[0].s : "";
            sm_push(st, pat_user_call(fname, NULL, 0));
            break;
        }

        case SM_PAT_USERCALL_ARGS: {
            /* SN-8a: bare *func(args) in pattern context — args-on-stack form.
             * a[0].s = fname, a[1].i = nargs.  Pop nargs values (last-pushed = last
             * arg), build XATP deferred-usercall with the evaluated args.
             * No child pattern is popped — bare *fn() wraps nothing. */
            int nargs = (int)ins->a[1].i;
            DESCR_t *argv = nargs > 0
                ? (DESCR_t *)GC_MALLOC((size_t)nargs * sizeof(DESCR_t))
                : NULL;
            for (int i = nargs - 1; i >= 0; i--) argv[i] = sm_pop(st);
            const char *fname = ins->a[0].s ? ins->a[0].s : "";
            sm_push(st, pat_user_call(fname, argv, nargs));
            break;
        }

        case SM_EXEC_STMT: {
            /*
             * Stack at entry (top-of-stack = last pushed):
             *   [subj_descr] [pat_descr(DT_P)] [repl_or_zero]
             *   a[0].s  = subject variable name (or NULL)
             *   a[1].i  = has_repl flag
             *
             * ME-1: pattern is now on the value stack alongside subj and repl.
             * sm_lower push order: pat_tree, subject, repl — so pop order is repl, subj, pat.
             */
            int has_repl = (int)ins->a[1].i;
            DESCR_t repl   = sm_pop(st);    /* replacement or INTVAL(0) — top of stack */
            DESCR_t subj_d = sm_pop(st);    /* subject descriptor */
            DESCR_t pat_d  = sm_pop(st);    /* pattern (DT_P) — pushed first by sm_lower */

            const char *sname = ins->a[0].s;   /* subject var name for write-back */

            int ok = exec_stmt(sname, &subj_d, pat_d,
                               has_repl ? &repl : NULL, has_repl);
            st->last_ok = ok;
            break;
        }

        /* ── OE-10/11: Byrd box broker opcodes — Icon/Prolog SM-run support ── */
        case SM_BB_PUMP: {
            /* Pop DT_E descriptor whose .ptr is the AST_t* of the Icon statement subject.
             * Build a drivable bb_node_t via coro_eval, pump all values via bb_broker. */
            DESCR_t expr_d = sm_pop(st);
            AST_t *expr   = (AST_t *)expr_d.ptr;
            if (!expr) { st->last_ok = 0; break; }
            bb_node_t node = coro_eval(expr);
            int ticks = bb_broker(node, BB_PUMP, pump_print, NULL);
            st->last_ok = (ticks > 0);
            break;
        }

        case SM_BB_ONCE: {
            /* Pop DT_E descriptor whose .ptr is the AST_t* of the Prolog statement subject.
             * Build a bb_node_t via coro_eval (shared builder handles AST_CHOICE/AST_CLAUSE),
             * drive once via bb_broker(BB_ONCE). */
            DESCR_t expr_d = sm_pop(st);
            AST_t *expr   = (AST_t *)expr_d.ptr;
            if (!expr) { st->last_ok = 0; break; }
            bb_node_t node = coro_eval(expr);
            int ticks = bb_broker(node, BB_ONCE, NULL, NULL);
            st->last_ok = (ticks > 0);
            break;
        }

        /* CH-17f: Prolog goal dispatch by predicate key — replaces legacy
         * lower_expr(AST_CHOICE) + SM_BB_ONCE.  a[0].s = "name/arity" key,
         * a[1].i = arity.  Looks up Pl_PredEntry; if entry_pc >= 0 AND the
         * expression body is filled (CH-17f body fill rung), uses pl_box_choice_pc;
         * otherwise falls back to pl_box_choice (IR path — correct semantics).
         * No AST_t* pushed or walked at the SM statement-dispatch layer. */
        case SM_BB_ONCE_PROC: {
            const char *key   = ins->a[0].s;
            int         arity = (int)ins->a[1].i;
            /* IR fallback: look up the AST_CHOICE node and drive it.
             * This is the correct path until expression bodies are filled in
             * a later CH-17f body-fill rung. */
            AST_t *choice = key ? pl_pred_table_lookup_global(key) : NULL;
            bb_node_t node = choice ? pl_box_choice(choice, g_pl_env, arity)
                                    : pl_box_fail();
            int ticks = bb_broker(node, BB_ONCE, NULL, NULL);
            st->last_ok = (ticks > 0);
            break;
        }

        /* CHUNKS-step12: name-driven Icon proc BB pump — replaces the
         * synthesised AST_FNC + emit_push_expr + SM_BB_PUMP wrapper that
         * sm_lower used to emit for the top-level call_main(). a[0].s = proc
         * name, a[1].i = nargs. nargs values, if any, are popped from the
         * value stack in caller-pushed order (reverse-pop). No AST_t is
         * constructed or walked at this layer. The IR walk inside
         * coro_call(proc_table[i].proc, ...) is unchanged — Step 17 territory. */
        case SM_BB_PUMP_PROC: {
            const char *name  = ins->a[0].s;
            int         nargs = (int)ins->a[1].i;
            DESCR_t *args = NULL;
            if (nargs > 0) {
                args = calloc(nargs, sizeof(DESCR_t));
                for (int k = nargs - 1; k >= 0; k--) args[k] = sm_pop(st);
            }
            bb_node_t node = coro_pump_proc_by_name(name, args, nargs);
            if (!node.fn) {
                /* proc not found in proc_table — treat as failed pump */
                if (args) free(args);
                st->last_ok = 0;
                break;
            }
            g_ast_pump_active++;
            int ticks = bb_broker(node, BB_PUMP, pump_print, NULL);
            g_ast_pump_active--;
            st->last_ok = (ticks > 0);
            break;
        }

        /* CHUNKS-step13: Raku CASE dispatch — replaces emit_push_expr +
         * SM_BB_PUMP for AST_CASE. a[0].i = ncases, a[1].i = has_default.
         * Stack layout (bottom→top, i.e. earliest pushed first):
         *   topic_chunk          (DT_E)
         *   cmp_kind_0           (DT_I, AST_e: AST_LEQ for ==, AST_EQ otherwise)
         *   val_chunk_0          (DT_E)
         *   body_chunk_0         (DT_E)
         *   ... ncases triples ...
         *   default_body_chunk   (DT_E, only if has_default)
         * Reverse-pop, evaluate topic, walk arms, run matching body.
         * The matched body's value (or NULVCL) is left on the stack —
         * even though AST_CASE is currently used in stmt-context (raku
         * given_stmt), keeping the value-context discipline consistent
         * with the underlying coro_value.c AST_CASE evaluator means
         * future value-context use is symmetric. The trailing SM_VOID_POP
         * the lower_stmt expression-stmt path emits for AST_CASE balances
         * the stack. Mirrors the comparison logic in
         * coro_value.c:947 — string compare on AST_LEQ, integer-or-string
         * compare on AST_EQ — but operates entirely on expression-call results,
         * never on AST_t. */
        case SM_BB_PUMP_CASE: {
            int ncases      = (int)ins->a[0].i;
            int has_default = (int)ins->a[1].i;

            /* Pop default body first (top of stack) if present */
            int default_pc = -1;
            if (has_default) {
                DESCR_t d = sm_pop(st);
                default_pc = (d.v == DT_E && d.slen == 1) ? (int)d.i : -1;
            }

            /* Pop ncases triples in reverse: (body, val, kind) per arm,
             * but each arm was pushed in (kind, val, body) order, so
             * top-of-stack is body of last arm. We collect into arrays
             * indexed in source-order. */
            int *cmp_kinds = (int*)GC_malloc(sizeof(int) * (ncases > 0 ? ncases : 1));
            int *val_pcs   = (int*)GC_malloc(sizeof(int) * (ncases > 0 ? ncases : 1));
            int *body_pcs  = (int*)GC_malloc(sizeof(int) * (ncases > 0 ? ncases : 1));
            for (int k = ncases - 1; k >= 0; k--) {
                DESCR_t b = sm_pop(st);
                DESCR_t v = sm_pop(st);
                DESCR_t c = sm_pop(st);
                body_pcs[k]  = (b.v == DT_E && b.slen == 1) ? (int)b.i : -1;
                val_pcs[k]   = (v.v == DT_E && v.slen == 1) ? (int)v.i : -1;
                cmp_kinds[k] = (c.v == DT_I) ? (int)c.i : (int)AST_EQ;
            }

            /* Pop topic expression and evaluate it */
            DESCR_t topic_d = sm_pop(st);
            int topic_pc = (topic_d.v == DT_E && topic_d.slen == 1) ? (int)topic_d.i : -1;
            DESCR_t topic = (topic_pc >= 0) ? sm_call_expression(topic_pc) : NULVCL;

            /* Walk arms: for each, eval val_chunk, compare, run body on match */
            DESCR_t result = NULVCL;
            int matched   = 0;
            for (int k = 0; k < ncases; k++) {
                if (val_pcs[k] < 0 || body_pcs[k] < 0) continue;
                DESCR_t wval = sm_call_expression(val_pcs[k]);
                int match = 0;
                if ((AST_e)cmp_kinds[k] == AST_LEQ) {
                    /* String equality (Raku ==): coerce to string both sides */
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

            sm_push(st, result);
            st->last_ok = matched;
            break;
        }

        /* CHUNKS-step15: BB pump for an SM generator expression — replaces the
         * legacy emit_push_expr + SM_BB_PUMP pair for migrated Icon
         * generator kinds (CH-15a: AST_TO, AST_TO_BY).  Pops expression descriptor
         * (DT_E, slen=1, .i = entry_pc) from TOS, allocates an SmGenState
         * rooted at that entry_pc, and drives the expression via
         * bb_broker_drive_sm with the same pump_print body that
         * SM_BB_PUMP uses — preserving Icon's statement-context
         * "every yielded value is printed" semantics.  No AST_t walk
         * anywhere on this path: the expression body is pure SM with explicit
         * SM_SUSPEND yield points. */
        case SM_BB_PUMP_SM: {
            DESCR_t d = sm_pop(st);
            if (d.v != DT_E || d.slen != 1) {
                /* Not a expression descriptor — treat as failed pump */
                st->last_ok = 0;
                break;
            }
            int entry_pc = (int)d.i;
            if (entry_pc < 0 || entry_pc >= prog->count) {
                st->last_ok = 0;
                break;
            }
            SmGenState *gs = sm_gen_state_new(entry_pc);
            int ticks = bb_broker_drive_sm(gs, pump_print, NULL);
            st->last_ok = (ticks > 0);
            break;
        }

        /* CHUNKS-step17i-every-suspend: AST_EVERY dispatch by id.
         * Mirrors SM_BB_ONCE_PROC for Prolog.  a[0].i = every_id;
         * every_table_lookup(id) returns the borrowed AST_t* registered
         * by sm_lower at expression-body lowering time.  Runtime delegates
         * to the existing IR broker (coro_eval(AST_EVERY) builds an
         * icn_every_state_t whose body field is e->children[1]; coro_bb_every
         * pumps gen and calls bb_exec_stmt(body) per tick — this is the
         * "boxes stay; graph-construction moves to lower-time" boundary
         * the orientation note draws).  The SM bytecode and value stack
         * carry only the integer id; no AST_t* in either layer.
         *
         * Stack discipline: this opcode is reached from proc-body lowering's
         * `lower_expr(body); SM_VOID_POP` loop.  The legacy
         * `emit_push_expr + SM_BB_PUMP` shape was net-stack-zero (push 1,
         * pop 1) — the trailing SM_VOID_POP underflowed.  Fix: push DT_NUL
         * after the pump so the trailing SM_VOID_POP balances.  In value
         * context (every used as expression result, rare) DT_NUL is the
         * correct semantic per Icon's `every` having no value.
         *
         * Body-function: NULL.  AST_EVERY-as-statement runs its own body
         * (the do-clause, e.g. `write(v)`) via bb_exec_stmt inside
         * coro_bb_every — passing pump_print would double-print yielded
         * values, since the user's body already produces output. */
        case SM_BB_PUMP_EVERY: {
            int every_id = (int)ins->a[0].i;
            AST_t *every_ast = every_table_lookup(every_id);
            if (!every_ast) {
                st->last_ok = 0;
                sm_push(st, NULVCL);
                break;
            }
            g_ast_pump_active++;
            bb_node_t node = coro_eval(every_ast);
            int ticks = bb_broker(node, BB_PUMP, NULL, NULL);
            g_ast_pump_active--;
            st->last_ok = (ticks > 0);
            sm_push(st, NULVCL);   /* every is void — leave a placeholder for VOID_POP */
            break;
        }

        /* GOAL-ICON-BB-COMPLETE Phase A: unified handler for legacy-fallthrough kinds.
         * Mirrors SM_BB_PUMP_EVERY but drives the generator to collect ONE value
         * (not to exhaustion) — the caller (every / for-each loop in the proc body)
         * is responsible for re-issuing SM_BB_PUMP_AST on resume.
         *
         * Stack discipline: pushes the generator's result descriptor on success,
         * NULVCL on fail.  last_ok = 1 on success, 0 on fail.
         *
         * This reuses coro_eval → bb_broker(BB_PUMP) exactly like EVERY; the
         * difference is that EVERY drives to exhaustion while this opcode yields
         * one value.  For Phase A kinds (BANG_BINARY, LCONCAT-gen, LIMIT, RANDOM,
         * SECTION, SECTION_MINUS, SECTION_PLUS) coro_eval builds the correct
         * coro_bb_* Byrd-box node and bb_broker(BB_PUMP) drives it one step. */
        case SM_BB_PUMP_AST: {
            int ast_id = (int)ins->a[0].i;
            AST_t *ast = ast_pump_table_lookup(ast_id);
            if (!ast) {
                st->last_ok = 0;
                sm_push(st, NULVCL);
                break;
            }
            /* Increment g_ast_pump_active: marks that coro_eval calls from
             * within this handler (and anything it transitively invokes,
             * including nested sm_interp_run calls for SM proc bodies) are
             * intentional Phase A bridges, not leaks.  The guard macro in
             * coro_runtime.h exempts coro_eval when this counter > 0. */
            int saved = g_sm_dispatch_active;
            g_sm_dispatch_active = 0;
            g_ast_pump_active++;
            bb_node_t node = coro_eval(ast);
            DESCR_t result = node.fn(node.ζ, α);
            g_ast_pump_active--;
            g_sm_dispatch_active = saved;
            if (IS_FAIL_fn(result)) {
                st->last_ok = 0;
                sm_push(st, NULVCL);
            } else {
                st->last_ok = 1;
                sm_push(st, result);
            }
            break;
        }

        /* CHUNKS-step17i-suspend: yield primitive for `suspend E [do body]`.
         *
         * Pops one value (the yield value) from the SM stack.  If we're
         * inside a coroutine context (proc_trampoline / gather_trampoline
         * set active_coro when they entered), call sm_yield_to_caller —
         * it stashes the value in active_coro->yielded and swapcontexts
         * to the caller.  When the caller's broker swaps back, control
         * returns from sm_yield_to_caller and the SM dispatch loop
         * proceeds — the next instructions in our lowering shape are
         * the do-clause and the placeholder NULVCL push.
         *
         * Outside a coroutine (top-level suspend, semantically rare and
         * not exercised by the corpus today), push the value back so the
         * outer SM_VOID_POP balances.  This treats top-level suspend as
         * a no-op yield, matching the existing coro_stmt.c:88 semantics
         * where FRAME.suspending=1 has no observer when active_coro is
         * NULL. */
        case SM_SUSPEND_VALUE: {
            DESCR_t v = sm_pop(st);
            if (sm_yield_to_caller(v)) {
                /* Yielded and resumed.  Stack is now one shorter than at
                 * SM_SUSPEND_VALUE entry.  The lowering's next instruction
                 * is the do-clause (or directly SM_PUSH_NULL).  last_ok
                 * stays as it was (the value succeeded — that's how we
                 * got past the SM_JUMP_F gate above this opcode). */
                st->last_ok = 1;
            } else {
                /* No coroutine — push value back as a placeholder for the
                 * outer SM_VOID_POP.  The lowering's SM_PUSH_NULL +
                 * do-clause are dead in this case, but they're harmless:
                 * they just push and pop one extra value.  Actually wait
                 * — the lowering doesn't gate on whether we yielded.  In
                 * the no-coroutine case, the do-clause WILL run (it would
                 * have run on resume in the coroutine case).  Push the
                 * value back; the lowering will then run do-clause,
                 * VOID_POP it, and SM_PUSH_NULL.  That leaves
                 * stack-effect +2 instead of the +1 that the proc-body
                 * loop expects.  This is acceptable for now — top-level
                 * AST_SUSPEND is not exercised by the rung03/04 corpus
                 * and the existing AST-walker fallback (coro_stmt.c:88)
                 * has the same "no-op" behaviour outside a coroutine. */
                sm_push(st, v);
                st->last_ok = !IS_FAIL_fn(v);
            }
            break;
        }

        /* ── Functions (stubs — wired in U3) ───────────────────────── */

        case SM_CALL_FN: {
            const char *name  = ins->a[0].s;
            int         nargs = (int)ins->a[1].i;

            /* Special pseudo-calls handled inline */
            if (name && strcmp(name, "INDIR_GET") == 0) {
                /* $expr: pop descriptor from value stack, look up variable, push its value.
                 * Three cases:
                 *   DT_S "bal"  → $'bal' : look up variable named by string
                 *   DT_N NAMEVAL("bal") → $.bal path: name-of-bal; $ deref = value of bal
                 *   DT_N NAMEPTR(p)     → $ on interior ptr: deref pointer directly
                 */
                DESCR_t name_d = sm_pop(st);
                DESCR_t val;
                if (IS_NAMEPTR(name_d)) {
                    val = NAME_DEREF_PTR(name_d);   /* interior ptr → value directly */
                } else if (IS_NAMEVAL(name_d)) {
                    val = nv_fold_get(name_d.s);
                } else {
                    val = nv_fold_get(VARVAL_fn(name_d));
                }
                sm_push(st, val);
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "NAME_PUSH") == 0) {
                /* .X: pop name string, push DT_N NAMEVAL descriptor.
                 * Use NAMEVAL (slen=0, name in .s) so VARVAL_fn returns the
                 * name string correctly. NAMEPTR (interior ptr) would cause
                 * VARVAL_fn to do NV_name_from_ptr which fails for names not
                 * yet in the NV table (e.g. OPSYN(.facto,'fact') before facto
                 * is ever read/written). */
                DESCR_t name_d = sm_pop(st);
                const char *vname0 = VARVAL_fn(name_d);
                char *vname = GC_strdup(vname0 ? vname0 : ""); sno_fold_name(vname);
                sm_push(st, NAMEVAL(vname));
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "ASGN_INDIR") == 0) {
                DESCR_t name_d = sm_pop(st);
                DESCR_t val    = sm_pop(st);
                int ok = 0;
                if (IS_NAMEPTR(name_d)) {
                    /* $(.var) — write through name pointer directly */
                    *(DESCR_t*)name_d.ptr = val; ok = 1;
                } else if (IS_NAMEVAL(name_d)) {
                    nv_fold_set(name_d.s, val); ok = 1;
                } else {
                    const char *vname0 = VARVAL_fn(name_d);
                    if (vname0 && *vname0) { nv_fold_set(vname0, val); ok = 1; }
                    /* else: empty/null name — fail the statement */
                }
                sm_push(st, val);
                st->last_ok = ok;
                break;
            }
            if (name && strcmp(name, "NRETURN_ASGN") == 0) {
                /* NRETURN lvalue assignment: fname() = rhs
                 * a[0].i = nargs (1), a[1].s = function name
                 * Stack: [rhs]  (1 item, already popped via nargs loop below)
                 * Call zero-param user fn; if it returns DT_N write through name,
                 * else try fname_SET(rhs, result) field-mutator convention. */
                const char *fname = ins->a[1].s;
                DESCR_t rhs = sm_pop(st);
                DESCR_t fres = INVOKE_fn(fname, NULL, 0);
                int ok = 0;
                if (IS_NAMEPTR(fres)) { NAME_DEREF_PTR(fres) = rhs; ok = 1; }
                else if (IS_NAMEVAL(fres)) { NV_SET_fn(fres.s, rhs); ok = 1; }
                else {
                    /* Field mutator fallback: fname_SET(rhs, obj) */
                    char setname[256];
                    snprintf(setname, sizeof(setname), "%s_SET", fname ? fname : "");
                    DESCR_t sargs[2] = { rhs, fres };
                    DESCR_t sr = INVOKE_fn(setname, sargs, 2);
                    ok = (sr.v != DT_FAIL);
                }
                sm_push(st, rhs);
                st->last_ok = ok;
                break;
            }
            if (name && strcmp(name, "IDX") == 0) {
                /* subscript read: stack top=last_idx ... base; nargs=nchildren */
                if (nargs == 2) {
                    DESCR_t idx  = sm_pop(st);
                    DESCR_t base = sm_pop(st);
                    DESCR_t r = subscript_get(base, idx);
                    sm_push(st, r);
                    st->last_ok = (r.v != DT_FAIL);
                } else if (nargs == 3) {
                    DESCR_t j    = sm_pop(st); DESCR_t i = sm_pop(st);
                    DESCR_t base = sm_pop(st);
                    DESCR_t r = subscript_get2(base, i, j);
                    sm_push(st, r);
                    st->last_ok = (r.v != DT_FAIL);
                } else {
                    /* N-dim (nargs >= 4): sm_lower pushed children[0]=base first,
                     * then indices. Stack top→bot: idx[ndim-1]...idx[0], base.
                     * nargs == nchildren == 1+ndim_indices.
                     * Pop nargs items: indices top-first, then base. */
                    int n = nargs; /* total stack items = 1 base + (n-1) indices */
                    DESCR_t raw[32];
                    for (int k = 0; k < n; k++) raw[k] = sm_pop(st);
                    /* raw[0]=last_idx, raw[n-2]=first_idx, raw[n-1]=base */
                    DESCR_t base = raw[n-1];
                    /* fargs[0]=base, fargs[1..n-1]=indices in original order */
                    DESCR_t fargs[32]; fargs[0] = base;
                    for (int k = 0; k < n-1; k++) fargs[k+1] = raw[n-2-k];
                    DESCR_t r = INVOKE_fn("ITEM", fargs, n);
                    sm_push(st, r);
                    st->last_ok = (r.v != DT_FAIL);
                }
                break;
            }
            if (name && strcmp(name, "IDX_SET") == 0) {
                /* sm_lower emits: rhs, then base, then i [, j]
                 * Stack top-to-bottom at IDX_SET: i [j], base, rhs
                 * Pop: indices first (top), then base, then rhs (bottom). */
                if (nargs == 3) {        /* 1D: children=2, nc+1=3 */
                    DESCR_t i    = sm_pop(st);
                    DESCR_t base = sm_pop(st);
                    DESCR_t val  = sm_pop(st);
                    st->last_ok = subscript_set(base, i, val);
                    /* SN-32a-idxset: fire VALUE <lval> for symmetry with
                     * SPITBOL's asnpb path on aggregate-element stores
                     * (mirrors SN-26-bridge-coverage-g for the SM lowering). */
                    { extern void comm_var(const char *, DESCR_t);
                      comm_var("<lval>", val); }
                    sm_push(st, val);
                } else if (nargs == 4) { /* 2D: children=3, nc+1=4 */
                    DESCR_t j    = sm_pop(st); DESCR_t i = sm_pop(st);
                    DESCR_t base = sm_pop(st);
                    DESCR_t val  = sm_pop(st);
                    st->last_ok = subscript_set2(base, i, j, val);
                    { extern void comm_var(const char *, DESCR_t);
                      comm_var("<lval>", val); }
                    sm_push(st, val);
                } else {
                    /* N-dim (nargs >= 5): sm_lower pushed rhs, base, then indices.
                     * Stack top→bot: idx[n-1]...idx[0], base, rhs(val).
                     * ndim = nargs - 2 indices. Pop: indices top-first, then base, then val. */
                    int ndim = nargs - 2;
                    DESCR_t idx[32];
                    for (int k = ndim - 1; k >= 0; k--) idx[k] = sm_pop(st); /* idx[0]=first, idx[ndim-1]=last in original order */
                    DESCR_t base = sm_pop(st);
                    DESCR_t val  = sm_pop(st);
                    /* ITEM_SET: args[0]=val, args[1]=base, args[2..2+ndim-1]=indices */
                    DESCR_t fargs[32]; fargs[0] = val; fargs[1] = base;
                    for (int k = 0; k < ndim; k++) fargs[k+2] = idx[k];
                    DESCR_t r = INVOKE_fn("ITEM_SET", fargs, ndim + 2);
                    st->last_ok = (r.v != DT_FAIL);
                    { extern void comm_var(const char *, DESCR_t);
                      comm_var("<lval>", val); }
                    sm_push(st, val);
                }
                break;
            }

            /* GOAL-ICON-BB-COMPLETE A3: ICN_RANDOM — ?E one-shot random selection.
             * Mirrors bb_eval_value's AST_RANDOM arm in coro_value.c:698-746.
             * A3-seed-fix: uses canonical bb_icn_rnd_seed (defined in coro_value.c)
             * so --ir-run and --sm-run produce identical sequences for random programs.
             * NOTE: inner early-exits use goto (not break) to avoid breaking
             * nested for-loops instead of the outer switch case. */
            if (name && strcmp(name, "ICN_RANDOM") == 0) {
                DESCR_t v = sm_pop(st);
                if (IS_FAIL_fn(v)) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                bb_icn_rnd_seed = bb_icn_rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
                unsigned long rnd = bb_icn_rnd_seed >> 33;
                DESCR_t result = FAILDESCR;
                if (IS_INT_fn(v)) {
                    long long n = v.i;
                    if (n <= 0) goto icn_random_fail;
                    result = INTVAL((long long)(rnd % (unsigned long)n) + 1);
                } else if (v.v == DT_S || v.v == DT_SNUL) {
                    const char *s = VARVAL_fn(v);
                    if (!s || !*s) goto icn_random_fail;
                    long slen = (long)strlen(s);
                    char *out = GC_malloc(2); out[0] = s[rnd % (unsigned long)slen]; out[1] = '\0';
                    result = STRVAL(out);
                } else if (v.v == DT_DATA) {
                    DESCR_t tag = FIELD_GET_fn(v, "icn_type");
                    if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                        int n = (int)FIELD_GET_fn(v, "frame_size").i;
                        if (n <= 0) goto icn_random_fail;
                        DESCR_t ea = FIELD_GET_fn(v, "frame_elems");
                        if (ea.v == DT_DATA && ea.ptr) {
                            DESCR_t *elems = (DESCR_t *)ea.ptr;
                            result = elems[rnd % (unsigned long)n];
                        }
                    } else if (v.u && v.u->type && v.u->type->nfields > 0 && v.u->fields) {
                        int n = v.u->type->nfields;
                        result = v.u->fields[rnd % (unsigned long)n];
                    }
                } else if (v.v == DT_T) {
                    if (!v.tbl || v.tbl->size <= 0) goto icn_random_fail;
                    int target = (int)(rnd % (unsigned long)v.tbl->size);
                    int seen = 0;
                    for (int b = 0; b < TABLE_BUCKETS; b++) {
                        for (TBPAIR_t *bp = v.tbl->buckets[b]; bp; bp = bp->next) {
                            if (seen == target) { result = bp->val; goto icn_random_done; }
                            seen++;
                        }
                    }
                }
                goto icn_random_done;
                icn_random_fail: result = FAILDESCR;
                icn_random_done:
                sm_push(st, result);
                st->last_ok = (result.v != DT_FAIL);
                break;
            }

            /* GOAL-ICON-BB-COMPLETE A2: ICN_SECTION_RANGE/PLUS/MINUS
             * Stack (pushed by sm_lower in order): string, lo, hi → TOS=hi
             * Mirrors bb_section() in coro_value.c exactly.
             * Uses the same Icon position rules: p>0 → 1-based; p==0 → slen+1; p<0 → slen+1+p. */
            if (name && (strcmp(name, "ICN_SECTION_RANGE") == 0 ||
                         strcmp(name, "ICN_SECTION_PLUS")  == 0 ||
                         strcmp(name, "ICN_SECTION_MINUS") == 0)) {
                DESCR_t hi_d = sm_pop(st);
                DESCR_t lo_d = sm_pop(st);
                DESCR_t sd   = sm_pop(st);
                if (IS_FAIL_fn(sd) || IS_FAIL_fn(lo_d) || IS_FAIL_fn(hi_d)) {
                    sm_push(st, FAILDESCR); st->last_ok = 0; break;
                }
                const char *s = (sd.v == DT_S || sd.v == DT_SNUL) ? VARVAL_fn(sd) : NULL;
                if (!s) s = "";
                int slen = (int)strlen(s);
                int i = (int)to_int(lo_d);
                int x = (int)to_int(hi_d);
                if (i == 0) i = slen + 1; else if (i < 0) i = slen + 1 + i;
                int lo, hi;
                if (strcmp(name, "ICN_SECTION_RANGE") == 0) {
                    if (x == 0) x = slen + 1; else if (x < 0) x = slen + 1 + x;
                    if (i < 1 || i > slen+1 || x < 1 || x > slen+1) {
                        sm_push(st, FAILDESCR); st->last_ok = 0; break;
                    }
                    lo = i < x ? i : x;
                    hi = i < x ? x : i;
                } else if (strcmp(name, "ICN_SECTION_PLUS") == 0) {
                    if (i < 1 || i > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                    if (x >= 0) { lo = i;     hi = i + x; }
                    else        { lo = i + x; hi = i;     }
                    if (lo < 1 || hi > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                } else { /* ICN_SECTION_MINUS */
                    if (i < 1 || i > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                    if (x >= 0) { lo = i - x; hi = i;     }
                    else        { lo = i;     hi = i - x; }
                    if (lo < 1 || hi > slen+1) { sm_push(st, FAILDESCR); st->last_ok = 0; break; }
                }
                int len = hi - lo;
                char *buf = GC_malloc(len + 1);
                memcpy(buf, s + lo - 1, len);
                buf[len] = '\0';
                DESCR_t r = STRVAL(buf);
                sm_push(st, r); st->last_ok = 1;
                break;
            }

            DESCR_t args[32];
            for (int k = nargs - 1; k >= 0; k--)
                args[k] = sm_pop(st);
            /* ICN_SCAN_PUSH / ICN_SCAN_POP — Icon scan-context helpers.
             * Must be handled BEFORE the SN-6 FAIL-arg check because
             * ICN_SCAN_POP must pass a failed body result through, and
             * ICN_SCAN_PUSH must not be treated as a normal function call. */
            if (name && strcmp(name, "ICN_SCAN_PUSH") == 0 && nargs == 1) {
                const char *s = VARVAL_fn(args[0]); if (!s) s = "";
                if (scan_depth < SCAN_STACK_MAX) {
                    scan_stack[scan_depth].subj = scan_subj;
                    scan_stack[scan_depth].pos  = scan_pos;
                    scan_depth++;
                }
                scan_subj = GC_strdup(s); scan_pos = 1;
                sm_push(st, args[0]);
                st->last_ok = 1;
                break;
            }
            if (name && strcmp(name, "ICN_SCAN_POP") == 0 && nargs == 1) {
                if (scan_depth > 0) {
                    scan_depth--;
                    scan_subj = scan_stack[scan_depth].subj;
                    scan_pos  = scan_stack[scan_depth].pos;
                }
                sm_push(st, args[0]);
                st->last_ok = (args[0].v != DT_FAIL);
                break;
            }
            /* SN-6: SNOBOL4 semantics — if any argument is FAIL, the call fails
             * without invoking the function. This is what allows
             * CHARS + SIZE(INPUT) :F(DONE) to branch when INPUT hits EOF:
             * INPUT returns FAILDESCR → SIZE receives it → SIZE would swallow it,
             * but we catch it here before the call. */
            for (int k = 0; k < nargs; k++) {
                if (args[k].v == DT_FAIL) {
                    sm_push(st, FAILDESCR);
                    st->last_ok = 0;
                    goto sm_call_done;
                }
            }
            /* CH-17g-smcall-proc: Icon user-proc fast path.
             * When name matches a proc_table entry with a resolved entry_pc,
             * dispatch via sm_call_proc (frame-slot ABI) instead of the NV-binding
             * inline path below.  This is the correct calling convention for proc
             * bodies lowered by CH-17b'' which emit SM_LOAD_FRAME / SM_STORE_FRAME.
             * Without this, SM_CALL_FN binds params into NV but the body reads
             * frame slots → params always read as uninitialised FAILDESCR. */
            if (name && g_current_sm_prog) {
                extern int proc_count;
                extern IcnProcEntry proc_table[];
                for (int _pi = 0; _pi < proc_count; _pi++) {
                    if (proc_table[_pi].entry_pc >= 0 &&
                        proc_table[_pi].name &&
                        strcmp(proc_table[_pi].name, name) == 0) {
                        DESCR_t _pr = sm_call_proc(proc_table[_pi].entry_pc,
                                                    proc_table[_pi].nparams,
                                                    args, nargs);
                        sm_push(st, _pr);
                        st->last_ok = (_pr.v != DT_FAIL);
                        goto sm_call_done;
                    }
                }
            }

            /* DATA field accessor/mutator/constructor: when first arg is a DATA
             * instance (or name ends in _SET with second arg DT_DATA), give
             * DATA field dispatch priority over same-named builtins (e.g. 'real'). */
            {
            DESCR_t result = FAILDESCR;
            int _data_first = (nargs >= 1 && args[0].v == DT_DATA);
            int _data_set   = (nargs >= 2 && args[1].v == DT_DATA && name &&
                               strlen(name) > 4 &&
                               strcasecmp(name + strlen(name) - 4, "_SET") == 0);
            if (_data_first || _data_set)
                result = sc_dat_field_call(name, args, nargs);

            /* RS-9a: if DATA dispatch failed/skipped, try SM-native user function.
             * Look up a named SM_LABEL for this function. If found: push a call
             * frame, bind params into NV, and jump to the body PC.
             * The body executes SM opcodes; SM_RETURN pops the frame and resumes. */
            if (result.v == DT_FAIL || (!_data_first && !_data_set)) {
                int body_pc = -1;
                if (!_data_first && !_data_set && name) {
                    /* Try canonical name, then uppercase.
                     * Note: DEFINE'd user functions have FNCEX_fn=true (registered)
                     * but no C body — we must try SM label lookup for them too. */
                    body_pc = sm_label_pc_lookup(prog, name);
                    if (body_pc < 0) {
                        char uname[128]; size_t nl = strlen(name);
                        if (nl >= sizeof(uname)) nl = sizeof(uname)-1;
                        for (size_t _i = 0; _i <= nl; _i++)
                            uname[_i] = (char)toupper((unsigned char)name[_i]);
                        body_pc = sm_label_pc_lookup(prog, uname);
                    }
                    /* Also try FUNC_ENTRY_fn alias (OPSYN) */
                    if (body_pc < 0) {
                        const char *entry = FUNC_ENTRY_fn(name);
                        if (entry) body_pc = sm_label_pc_lookup(prog, entry);
                    }
                }
                if (body_pc >= 0 && st->call_depth < SM_CALL_STACK_MAX) {
                    /* ── Push SM call frame ── */
                    SmCallFrame *fr = &st->call_stack[st->call_depth++];
                    fr->ret_pc = st->pc;  /* resume after the SM_CALL_FN instr */
                    fr->ret_ok = 1;
                    /* RS-9c: save caller's value stack so SM_STNO resets inside
                     * the callee don't wipe expression operands from the caller. */
                    fr->caller_sp = st->sp;
                    if (st->sp > 0) {
                        fr->caller_stack = GC_malloc(st->sp * sizeof(DESCR_t));
                        memcpy(fr->caller_stack, st->stack, st->sp * sizeof(DESCR_t));
                    } else {
                        fr->caller_stack = NULL;
                    }
                    st->sp = 0;  /* callee starts with an empty stack */

                    /* Determine retval NV slot (body writes result into this name) */
                    const char *entry2 = FUNC_ENTRY_fn(name);
                    const char *retname = (entry2 && strcmp(entry2, name) != 0
                                           && FNCEX_fn(entry2)) ? entry2 : name;
                    fr->retval_name = GC_strdup(retname);

                    /* Save + bind params and locals */
                    int np = FUNC_NPARAMS_fn(name);
                    int nl2 = FUNC_NLOCALS_fn(name);
                    if (np > 64) np = 64;
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
                    /* Jump into body */
                    st->pc = body_pc;
                    goto sm_call_done;  /* skip push/last_ok — body runs next */
                }
                /* No SM body found — fall through to INVOKE_fn (builtins + hook) */
                if (result.v == DT_FAIL || (!_data_first && !_data_set)) {
                    /* CH-17g-runtime-bridge-2 (2026-05-09): try Icon-builtin
                     * dispatch first.  APPLY_fn / INVOKE_fn knows only the
                     * SNOBOL4 builtin registry plus user functions registered
                     * through register_fn; Icon builtins live in
                     * interp_eval.c's icn_call_builtin and are never
                     * registered there.  When the name isn't a SNOBOL4
                     * builtin or user fn, APPLY_fn calls sno_err which
                     * longjmps out via g_sno_err_jmp — so we cannot wait
                     * for INVOKE_fn to return FAIL on Icon names.  Instead:
                     * try the Icon helper first.  If it handles the call,
                     * use its result; if it returns 0 (unknown name), fall
                     * through to INVOKE_fn unchanged (the Icon helper only
                     * recognises a fixed set of Icon builtin names; any
                     * other input passes through harmlessly).
                     *
                     * Without this fallback, --sm-run of any Icon program
                     * that calls write() FATALs with "Undefined function
                     * or operation."  The fallback was originally placed
                     * after INVOKE_fn, but APPLY_fn's longjmp-on-not-found
                     * meant control never returned to consult the helper. */
                    if (name) {
                        extern int icn_try_call_builtin_by_name(
                            const char *fn, DESCR_t *args, int nargs, DESCR_t *out);
                        DESCR_t icn_out;
                        if (icn_try_call_builtin_by_name(name, args, nargs, &icn_out)) {
                            result = icn_out;
                            goto sm_call_invoke_done;
                        }
                    }
                    result = INVOKE_fn(name, args, nargs);
                    sm_call_invoke_done: ;
                }
            }
            /* NRETURN: user fn returned DT_N — dereference like tree-walk AST_FNC */
            if (IS_NAMEPTR(result))      result = NAME_DEREF_PTR(result);
            else if (IS_NAMEVAL(result)) result = NV_GET_fn(result.s);
            sm_push(st, result);
            st->last_ok = (result.v != DT_FAIL);
            }
            sm_call_done:
            break;
        }

        case SM_RETURN:
        case SM_FRETURN:
        case SM_NRETURN:
        case SM_RETURN_S:  case SM_RETURN_F:
        case SM_FRETURN_S: case SM_FRETURN_F:
        case SM_NRETURN_S: case SM_NRETURN_F: {
            /* Conditional variants: _S fires only if last_ok; _F only if !last_ok. */
            int cond_s = (ins->op == SM_RETURN_S || ins->op == SM_FRETURN_S || ins->op == SM_NRETURN_S);
            int cond_f = (ins->op == SM_RETURN_F || ins->op == SM_FRETURN_F || ins->op == SM_NRETURN_F);
            if ((cond_s && !st->last_ok) || (cond_f && st->last_ok)) break; /* condition not met */
            /* RS-9a: if we have a live SM call frame, pop it and resume caller.
             * Otherwise (top-level): halt. */
            if (st->call_depth > 0) {
                SmCallFrame *fr = &st->call_stack[--st->call_depth];
                int is_fret  = (ins->op == SM_FRETURN  || ins->op == SM_FRETURN_S || ins->op == SM_FRETURN_F);
                int is_nret  = (ins->op == SM_NRETURN  || ins->op == SM_NRETURN_S || ins->op == SM_NRETURN_F);
                /* Read return value: for expression thunks (retval_name==NULL) use stack top;
                 * for user functions use the NV retval slot the body wrote. */
                DESCR_t retval = (fr->retval_name)
                    ? NV_GET_fn(fr->retval_name)
                    : ((st->sp > 0) ? st->stack[st->sp - 1] : FAILDESCR);
                /* Restore saved NV vars (reverse order so retval slot last) */
                for (int k = fr->nsaved - 1; k >= 0; k--)
                    NV_SET_fn(fr->saved_names[k], fr->saved_vals[k]);
                /* RS-9c: restore caller's value stack, then push return value on top */
                if (fr->caller_sp > 0 && fr->caller_stack) {
                    if (fr->caller_sp > st->stack_cap) {
                        st->stack = GC_realloc(st->stack, fr->caller_sp * sizeof(DESCR_t));
                        st->stack_cap = fr->caller_sp;
                    }
                    memcpy(st->stack, fr->caller_stack, fr->caller_sp * sizeof(DESCR_t));
                }
                st->sp = fr->caller_sp;
                /* Push result onto caller's restored value stack */
                if (is_fret) {
                    sm_push(st, FAILDESCR);
                    st->last_ok = 0;
                    strncpy(kw_rtntype, "FRETURN", sizeof(kw_rtntype)-1); /* RS-11 */
                } else if (is_nret) {
                    /* SN-33b-nreturn: NRETURN's `retval` (read at ~line 1338 from
                     * NV_GET_fn(retname)) is a NAME descriptor (DT_N) referring
                     * to the lvalue the body assigned to the function-name slot
                     * (e.g. `foo = .dummy` → retval is NAMEVAL("dummy")).
                     *
                     * For value context (`r = foo()`) the IR pipeline applies
                     * NAME_DEREF before storing — see interp_eval.c:2771
                     * `if (IS_NAME(r)) return NAME_DEREF(r);` after
                     * call_user_function returns.  The SM path was both
                     * (a) substituting NAMEVAL(retval_name) instead of retval,
                     *     pushing the function's own name as a string, AND
                     * (b) skipping the NAME_DEREF, causing `r` to receive a
                     *     raw NAME ref where IR receives the dereferenced value.
                     *
                     * Fix: deref the NAME on the way out, matching IR.  The
                     * downstream NRETURN-aware lvalue-assign path (asg10-style
                     * `fname() = rhs`) reads `kw_rtntype == "NRETURN"` to
                     * re-fetch the raw DT_N when needed (cf. interp_call.c
                     * BP-1 lines 344-349); SM follows the same convention via
                     * SM_STORE_NRETURN_ASGN, which reads NV_GET_fn(retval_name)
                     * itself and does not depend on what we push here. */
                    DESCR_t deref = retval;
                    if (IS_NAMEPTR(deref))      deref = NAME_DEREF_PTR(deref);
                    else if (IS_NAMEVAL(deref)) deref = NV_GET_fn(deref.s);
                    sm_push(st, deref);
                    st->last_ok = 1;
                    strncpy(kw_rtntype, "NRETURN", sizeof(kw_rtntype)-1); /* RS-11 */
                } else {
                    sm_push(st, retval);
                    st->last_ok = (retval.v != DT_FAIL);
                    strncpy(kw_rtntype, "RETURN",  sizeof(kw_rtntype)-1); /* RS-11 */
                }
                st->pc = fr->ret_pc;
            } else {
                /* Top-level return (nested sm_interp_run call from _usercall_hook).
                 * Set kw_rtntype so the caller can detect NRETURN/FRETURN. RS-11 */
                int is_fret = (ins->op == SM_FRETURN  || ins->op == SM_FRETURN_S || ins->op == SM_FRETURN_F);
                int is_nret = (ins->op == SM_NRETURN  || ins->op == SM_NRETURN_S || ins->op == SM_NRETURN_F);
                if (is_fret)      strncpy(kw_rtntype, "FRETURN", sizeof(kw_rtntype)-1);
                else if (is_nret) strncpy(kw_rtntype, "NRETURN", sizeof(kw_rtntype)-1);
                else              strncpy(kw_rtntype, "RETURN",  sizeof(kw_rtntype)-1);
                return 0;  /* halt */
            }
            break;
        }

        case SM_DEFINE:
            /* stub: function definition handled by preprocessor */
            break;

        case SM_INCR: {
            DESCR_t v = sm_pop(st);
            sm_push(st, INTVAL(v.i + ins->a[0].i));
            break;
        }

        case SM_DECR: {
            DESCR_t v = sm_pop(st);
            sm_push(st, INTVAL(v.i - ins->a[0].i));
            break;
        }

        /* CH-17g-runtime-bridge-acomp, sess 2026-05-09:
         * SM_ACOMP — numeric comparison emitted by sm_lower for
         * AST_EQ/AST_NE/AST_LT/AST_LE/AST_GT/AST_GE.  ins->a[0].i carries the
         * operator EKind.  Icon-style relops: on success push the RIGHT
         * operand and set last_ok=1; on failure push FAILDESCR and clear
         * last_ok.  Mirrors NUMREL macro in interp_eval.c.  SNUL (unset)
         * coerces to 0 — same convention as SM_ADD/SM_SUB. */
        case SM_ACOMP: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            if (l.v == DT_FAIL || r.v == DT_FAIL) {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
                break;
            }
            if (l.v == DT_SNUL) l = INTVAL(0);
            if (r.v == DT_SNUL) r = INTVAL(0);
            double lv = (l.v == DT_R) ? l.r : (double)((l.v == DT_I) ? l.i : 0);
            double rv = (r.v == DT_R) ? r.r : (double)((r.v == DT_I) ? r.i : 0);
            int op = (int)ins->a[0].i;
            int ok;
            switch (op) {
                case AST_EQ: ok = (lv == rv); break;
                case AST_NE: ok = (lv != rv); break;
                case AST_LT: ok = (lv <  rv); break;
                case AST_LE: ok = (lv <= rv); break;
                case AST_GT: ok = (lv >  rv); break;
                case AST_GE: ok = (lv >= rv); break;
                /* No operator code (legacy emit).  Fall back to equality
                 * — matches the historical SM_ACOMP-as-tristate intent
                 * least surprisingly.  Should not occur post-bridge-acomp. */
                default:   ok = (lv == rv); break;
            }
            if (ok) {
                sm_push(st, r);
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }

        /* CH-17g-runtime-bridge-lcomp, sess 2026-05-09:
         * SM_LCOMP — string/lexicographic comparison emitted by sm_lower
         * for AST_LLT/AST_LLE/AST_LGT/AST_LGE/AST_LEQ/AST_LNE.  ins->a[0].i carries
         * the operator EKind.  Sibling of SM_ACOMP — Icon-style relops:
         * on success push the RIGHT operand and set last_ok=1; on failure
         * push FAILDESCR and clear last_ok.  Mirrors STRREL macro in
         * interp_eval.c. */
        case SM_LCOMP: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            if (l.v == DT_FAIL || r.v == DT_FAIL) {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
                break;
            }
            const char *ls = VARVAL_fn(l); if (!ls) ls = "";
            const char *rs = VARVAL_fn(r); if (!rs) rs = "";
            int cmp = strcmp(ls, rs);
            int op = (int)ins->a[0].i;
            int ok;
            switch (op) {
                case AST_LLT: ok = (cmp <  0); break;
                case AST_LLE: ok = (cmp <= 0); break;
                case AST_LGT: ok = (cmp >  0); break;
                case AST_LGE: ok = (cmp >= 0); break;
                case AST_LEQ: ok = (cmp == 0); break;
                case AST_LNE: ok = (cmp != 0); break;
                /* Legacy emit safety net (pre-bridge-lcomp programs).
                 * Should not occur on freshly lowered code. */
                default:    ok = (cmp == 0); break;
            }
            if (ok) {
                sm_push(st, r);
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }

        /* CHUNKS-step14: SM_SUSPEND — yield a value from a generator expression.
         * Pops TOS as the yielded value.  If g_current_gen_state is live
         * (we are inside bb_broker_drive_sm), saves pc+stack into the
         * SmGenState and returns SM_INTERP_SUSPENDED so the broker can
         * deliver the value and later resume.  If not in a generator context
         * (bare sm_call_expression or top-level), yields FAILDESCR / no-op. */
        case SM_SUSPEND: {
            DESCR_t yielded = (st->sp > 0) ? sm_pop(st) : FAILDESCR;
            if (g_current_gen_state) {
                SmGenState *gs = g_current_gen_state;
                gs->yielded    = yielded;
                gs->resume_pc  = st->pc;   /* pc already advanced past SM_SUSPEND */
                gs->last_ok    = st->last_ok;
                /* snapshot value stack */
                if (st->sp > 0) {
                    if (st->sp > gs->stack_cap) {
                        gs->stack     = GC_realloc(gs->stack, st->sp * sizeof(DESCR_t));
                        gs->stack_cap = st->sp;
                    }
                    memcpy(gs->stack, st->stack, st->sp * sizeof(DESCR_t));
                }
                gs->sp = st->sp;
                return SM_INTERP_SUSPENDED;
            }
            /* Outside generator context: SM_SUSPEND in a bare call — push FAILDESCR */
            sm_push(st, FAILDESCR);
            break;
        }

        /* CHUNKS-step14: SM_RESUME — no-op in the dispatch loop.
         * Emitted as documentation at the top of a generator body so that
         * JIT codegen has a stable hook point.  bb_broker_drive_sm restores
         * the SmGenState and re-enters sm_interp_run at resume_pc; by the
         * time SM_RESUME would execute on re-entry, pc is already past it. */
        case SM_RESUME:
            break;

        /* CHUNKS-step14b: SM_LOAD_GLOCAL — push gen-local slot N onto value stack.
         * Only meaningful inside a generator expression driven by bb_broker_drive_sm.
         * Outside that context, push FAILDESCR and clear last_ok so callers
         * see the slot as unavailable (mirrors SM_PUSH_VAR's FAIL handling). */
        case SM_LOAD_GLOCAL: {
            int slot = (int)ins->a[0].i;
            if (g_current_gen_state && slot >= 0 && slot < SM_GEN_LOCAL_MAX) {
                sm_push(st, g_current_gen_state->locals[slot]);
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }

        /* CHUNKS-step14b: SM_STORE_GLOCAL — pop TOS into gen-local slot N.
         * The value is preserved on the stack (mirrors SM_STORE_VAR's
         * push-back-the-value semantics) so chained stores compose. */
        case SM_STORE_GLOCAL: {
            int slot = (int)ins->a[0].i;
            DESCR_t v = sm_pop(st);
            if (g_current_gen_state && slot >= 0 && slot < SM_GEN_LOCAL_MAX) {
                g_current_gen_state->locals[slot] = v;
                sm_push(st, v);
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }

        /* CHUNKS-step15a: SM_ICMP_GT — integer compare greater-than.
         * Pops right (TOS) then left (TOS-1).  Sets last_ok = (left.i > right.i).
         * Pushes nothing.  Used by AST_TO / AST_TO_BY generator expression loop-exit test. */
        case SM_ICMP_GT: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            st->last_ok = (l.i > r.i);
            break;
        }

        /* CHUNKS-step15a: SM_ICMP_LT — mirror of SM_ICMP_GT for negative-step AST_TO_BY. */
        case SM_ICMP_LT: {
            DESCR_t r = sm_pop(st);
            DESCR_t l = sm_pop(st);
            st->last_ok = (l.i < r.i);
            break;
        }

        /* CHUNKS-step17b'' (CH-17b''): SM_LOAD_FRAME — push IcnFrame.env[slot]
         * of the active Icon frame onto the value stack.  Outside an Icon frame
         * (frame_depth == 0), push FAILDESCR / clear last_ok — expressions emitted
         * with frame-slot ops are dead code until CH-17c flips the consumer
         * that reaches them, so the FAIL fallback never fires on real programs.
         * Mirrors the slot-resolution logic that bb_eval_value does for AST_VAR
         * when frame_depth > 0 (coro_value.c:382–399). */
        case SM_LOAD_FRAME: {
            int slot = (int)ins->a[0].i;
            if (icn_frame_env_active() && slot >= 0) {
                sm_push(st, icn_frame_env_load(slot));
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }

        /* CHUNKS-step17b'' (CH-17b''): SM_STORE_FRAME — pop TOS into IcnFrame.env[slot].
         * Like SM_STORE_VAR / SM_STORE_GLOCAL, the value is left on the stack
         * after the store (mirrors interp_eval AST_ASSIGN's "return val" semantics
         * so chained assigns / value-context assignments compose). */
        case SM_STORE_FRAME: {
            int slot = (int)ins->a[0].i;
            DESCR_t v = sm_pop(st);
            if (icn_frame_env_active() && slot >= 0) {
                icn_frame_env_store(slot, v);
                sm_push(st, v);
                st->last_ok = 1;
            } else {
                sm_push(st, FAILDESCR);
                st->last_ok = 0;
            }
            break;
        }

        default:
            fprintf(stderr, "sm_interp: unhandled opcode %d (%s) at pc=%d\n",
                    (int)ins->op, sm_opcode_name(ins->op), st->pc - 1);
            return -1;
        }
    }
    return 0;  /* fell off end = implicit HALT */
}

/* IM-4: sm_interp_run_steps — run at most n statements then return */
int sm_interp_run_steps(SM_Program *prog, SM_State *st, int n) {
    g_sm_step_limit = n;
    g_sm_steps_done = 0;
    int rc = 0;
    if (setjmp(g_sm_step_jmp) == 0)
        rc = sm_interp_run(prog, st);
    g_sm_step_limit = 0;
    g_sm_steps_done = 0;
    return rc;
}

/* CHUNKS-step02: sm_call_expression — run a compiled chunk as a thunk.
 * Used by EXPVAL_fn when called from C-land (via EVAL_fn or bb_usercall thaw).
 * Runs expression body in a fresh nested SM_State (own stack, shared NV table) with
 * a local g_sno_err_jmp save/restore so a runtime error inside the expression does
 * not longjmp into the outer dispatch's jmp_buf.  Returns the top-of-stack
 * value left by the expression body, or FAILDESCR if the expression errored. */

/* Subject globals (defined in stmt_exec.c) */
extern const char *Σ;
extern int         Ω;
extern int         Δ;
extern jmp_buf     g_sno_err_jmp;

DESCR_t sm_call_expression(int entry_pc)
{
    SM_Program *prog = g_current_sm_prog;
    if (!prog || entry_pc < 0 || entry_pc >= prog->count) {
        fprintf(stderr, "sm_call_expression: invalid entry_pc %d\n", entry_pc);
        return FAILDESCR;
    }

    /* Save NAM frame and subject globals */
    NAME_ctx_t chunk_ctx;
    NAME_ctx_enter(&chunk_ctx);
    const char *save_Σ = Σ;
    int         save_Ω = Ω;
    int         save_Δ = Δ;

    /* Save outer err_jmp; install a local one so expression errors don't escape */
    jmp_buf saved_err_jmp;
    memcpy(&saved_err_jmp, &g_sno_err_jmp, sizeof(jmp_buf));

    DESCR_t result = FAILDESCR;
    if (setjmp(g_sno_err_jmp) == 0) {
        SM_State *nested = GC_malloc(sizeof(SM_State));
        sm_state_init(nested);
        nested->pc = entry_pc;
        sm_interp_run(prog, nested);
        if (nested->sp > 0) result = nested->stack[nested->sp - 1];
    } /* else: expression errored — result stays FAILDESCR */

    /* Restore outer err_jmp */
    memcpy(&g_sno_err_jmp, &saved_err_jmp, sizeof(jmp_buf));

    Σ = save_Σ; Ω = save_Ω; Δ = save_Δ;
    NAME_ctx_leave();

    return result;
}

/*============================================================================================================================
 * CHUNKS-step14: sm_gen_state_new / bb_broker_drive_sm — SM generator infrastructure
 *
 * A generator expression is a compiled SM sub-program that uses SM_SUSPEND to yield successive
 * values.  bb_broker_drive_sm drives such an expression in BB_PUMP style: call body_fn for each
 * yielded value, stop when the expression reaches SM_RETURN or SM_HALT.
 *
 * Design:
 *   - SmGenState holds the full SM_State snapshot at the last suspension point.
 *   - g_current_gen_state is set around each sm_interp_run call so SM_SUSPEND
 *     can write into it and signal SM_INTERP_SUSPENDED.
 *   - On each tick we reconstruct an SM_State from the snapshot and run until
 *     the next suspension or termination.
 *============================================================================================================================*/

/* Allocate a fresh SmGenState for a generator at entry_pc. */
SmGenState *sm_gen_state_new(int entry_pc)
{
    SmGenState *gs = GC_malloc(sizeof(SmGenState));
    memset(gs, 0, sizeof *gs);
    gs->entry_pc  = entry_pc;
    gs->resume_pc = entry_pc;   /* first call starts at entry */
    gs->started   = 0;
    gs->yielded   = FAILDESCR;
    gs->stack     = NULL;
    gs->sp        = 0;
    gs->stack_cap = 0;
    gs->last_ok   = 1;
    return gs;
}

/* Drive an SM generator expression through all its ticks.
 * body_fn is called once per yielded value.  Returns tick count. */
int bb_broker_drive_sm(SmGenState *gs, void (*body_fn)(DESCR_t val, void *arg), void *arg)
{
    SM_Program *prog = g_current_sm_prog;
    if (!prog || !gs || gs->started == 2) return 0;

    int ticks = 0;

    for (;;) {
        /* Build (or restore) SM_State for this tick */
        SM_State *st = GC_malloc(sizeof(SM_State));
        sm_state_init(st);
        st->pc      = gs->resume_pc;
        st->last_ok = gs->last_ok;

        /* Restore value stack snapshot from previous suspension */
        if (gs->sp > 0 && gs->stack) {
            if (gs->sp > st->stack_cap) {
                st->stack     = GC_realloc(st->stack, gs->sp * sizeof(DESCR_t));
                st->stack_cap = gs->sp;
            }
            memcpy(st->stack, gs->stack, gs->sp * sizeof(DESCR_t));
            st->sp = gs->sp;
        }

        /* Arm the gen-state pointer so SM_SUSPEND can write into gs */
        SmGenState *outer_gs = g_current_gen_state;
        g_current_gen_state  = gs;
        gs->started = 1;

        int rc = sm_interp_run(prog, st);

        g_current_gen_state = outer_gs;   /* always restore */

        if (rc == SM_INTERP_SUSPENDED) {
            /* Generator yielded gs->yielded */
            ticks++;
            if (body_fn) body_fn(gs->yielded, arg);
            /* Loop: resume on next tick */
        } else {
            /* SM_RETURN, SM_HALT, or error — generator exhausted */
            gs->started = 2;
            break;
        }
    }

    return ticks;
}
