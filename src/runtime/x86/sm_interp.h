/*
 * sm_interp.h — SM_Program C interpreter dispatch loop (M-SCRIP-U2)
 *
 * Interprets a SM_Program directly in C — no x86 emission.
 * This is the Mode I execution engine and the correctness reference
 * for Mode G (in-memory codegen).
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-04-06
 */

#ifndef SM_INTERP_H
#define SM_INTERP_H

#include <stdlib.h>
#include <setjmp.h>
#include "sm_prog.h"
#include "snobol4.h"

/* ── RS-9a: SM call frame ────────────────────────────────────────────────
 * One frame per active user-defined function call in --sm-run mode.
 * Mirrors CallFrame in interp_call.c but references SM PCs not IR nodes. */
#define SM_CALL_STACK_MAX 256
#define SM_SAVED_NV_MAX    64

typedef struct {
    int         ret_pc;               /* PC to resume after RETURN */
    int         ret_ok;               /* 1=S branch on return, 0=F branch (FRETURN) */
    char       *retval_name;          /* NV slot the body writes its return value into */
    int         nsaved;               /* number of NV vars saved */
    char       *saved_names[SM_SAVED_NV_MAX];
    DESCR_t     saved_vals [SM_SAVED_NV_MAX];
    int         ret_jump_s_pc;        /* caller's :S label PC (-1 = fall through) */
    int         ret_jump_f_pc;        /* caller's :F label PC (-1 = fall through) */
    /* RS-9c: caller value-stack snapshot (survives SM_STNO resets inside callee) */
    int         caller_sp;            /* sp before nargs were popped */
    DESCR_t    *caller_stack;         /* GC'd copy of caller's stack[0..caller_sp-1] */
} SmCallFrame;

/* CHUNKS-step14: SmGenState — persistent state for a suspended generator expression.
 * bb_broker_drive_sm allocates one of these per generator invocation.
 * sm_interp_run fills it in when SM_SUSPEND fires; bb_broker_drive_sm
 * restores it on each resume call.
 * (Forward-declared as typedef struct SmGenState in sm_prog.h.) */
#define SM_GEN_LOCAL_MAX 8   /* CHUNKS-step14b: gen-local slots per chunk invocation;
                                covers every Icon generator kind in CHUNKS-step15
                                (TT_TO 3 slots, TT_TO_BY 4, TT_LIMIT 2, etc.) */
struct SmGenState {
    int      entry_pc;   /* initial entry pc (for first call) */
    int      resume_pc;  /* pc to resume from (instruction after last SM_SUSPEND) */
    int      started;    /* 0 = not yet entered, 1 = first call done, 2 = exhausted */
    DESCR_t  yielded;    /* value produced by last SM_SUSPEND */
    /* value stack snapshot at suspension point */
    DESCR_t *stack;      /* GC-owned copy of stack[0..sp-1] at suspend time */
    int      sp;         /* saved sp */
    int      stack_cap;  /* allocated capacity of stack[] */
    int      last_ok;    /* saved last_ok at suspend time */
    /* CHUNKS-step14b: per-invocation gen-local slots — accessed via
     * SM_LOAD_GLOCAL / SM_STORE_GLOCAL.  Survive SUSPEND/RESUME the same
     * way `stack` does (they are part of the SmGenState, which is the
     * persistent envelope).  Each Icon generator kind allocates a fixed
     * static count of these at expression start; the count itself is encoded
     * in the lowering, never stored. */
    DESCR_t  locals[SM_GEN_LOCAL_MAX];
};

/* Interpreter state */
typedef struct {
    DESCR_t  *stack;       /* dynamic value stack (realloc-grown) */
    int       sp;          /* stack pointer: stack[0..sp-1] are live */
    int       stack_cap;   /* current allocated capacity */
    int       last_ok;     /* 1 = last operation succeeded, 0 = failed */
    int       pc;          /* program counter: index into SM_Program */
    /* ME-6 (mode-3 only): epilogue signal from C return-handler back to the
     * SM-blob.  Set to 1 by me6_return_dispatch when a SmCallFrame is popped
     * AND the SM_LABEL emitting the function entry had a push-rbp prologue;
     * the SM_RETURN-variant blob reads it post-call and, if non-zero, emits
     * `mov rsp, rbp ; pop rbp` to undo the prologue before jumping back to
     * the trampoline.  Cleared by me6_return_dispatch on every call so a
     * stale 1 from a prior return can't leak.  Lives at offset 24 from r13
     * (verified via offsetof in build_scrip.sh wiring; see ME-6 in
     * GOAL-MODE3-EMIT.md).  Mode-2 (sm_interp.c) never reads or writes this. */
    int       jit_epilogue_pending;
    /* ME-6a (mode-3 only): set to 1 by h_call just before STATE->pc = body_pc
     * so SM_DEFINE_ENTRY blob knows the entry is a real function call (vs a
     * goto back to the function's own label).  The SM_DEFINE_ENTRY blob reads
     * this flag, does push rbp / mov rbp, rsp only if it is 1, then clears it.
     * Lives at offset 28 from r13.  Mode-2 never reads or writes this. */
    int       jit_in_call;
    jmp_buf   err_jmp;     /* per-statement error recovery (SM_STNO arms it) */
    int       err_fail_pc; /* pc to jump to on runtime error (-1 = halt) */
    int       err_armed;   /* 1 if err_jmp is live */
    /* RS-9a: SM-native call stack */
    SmCallFrame call_stack[SM_CALL_STACK_MAX];
    int         call_depth;
} SM_State;

/*
 * Execute prog from instruction 0 until SM_HALT or end.
 * Returns 0 on normal halt, -1 on error.
 * Uses the live SNOBOL4 runtime (NV_GET_fn / NV_SET_fn etc.) —
 * call only after SNO_INIT_fn() has run.
 */
int sm_interp_run(SM_Program *prog, SM_State *st);
int sm_interp_run_inner(SM_Program *prog, SM_State *st); /* A0: inner body; sm_interp_run wraps with dispatch flag */
int sm_interp_run_steps(SM_Program *prog, SM_State *st, int n);  /* IM-4 */

/* IM-4: SM step-limit globals */
extern int     g_sm_step_limit;
extern int     g_sm_steps_done;
extern jmp_buf g_sm_step_jmp;

/* Initialise a fresh SM_State (stack empty, pc=0, last_ok=1) */
void sm_state_init(SM_State *st);

/* Push / pop helpers (used by sm_interp_run and tests) */
void sm_push(SM_State *st, DESCR_t d);
DESCR_t sm_pop(SM_State *st);
DESCR_t sm_peek(SM_State *st);

/* CHUNKS-step02: run a compiled chunk thunk, return its result */
DESCR_t sm_call_expression(int entry_pc);

/* CHUNKS-step14: generator infrastructure.
 *
 * sm_gen_state_new — allocate and initialise a SmGenState for entry_pc.
 *   Must be called before the first bb_broker_drive_sm tick.
 *
 * bb_broker_drive_sm — drive an SM generator expression across all its ticks.
 *   Analogous to bb_broker(root, BB_PUMP, body_fn, arg) but for SM expressions
 *   rather than tree_t-based bb_node_t generators.
 *
 *   On each tick the interpreter runs until SM_SUSPEND (yield a value) or
 *   SM_RETURN / SM_HALT (generator exhausted).  body_fn is called once per
 *   yielded value.  Returns total tick count (0 = generator produced nothing).
 */
SmGenState *sm_gen_state_new(int entry_pc);
int bb_broker_drive_sm(SmGenState *gs, void (*body_fn)(DESCR_t val, void *arg), void *arg);
/* GOAL-ICON-BB-COMPLETE rung13: drive one tick; push result into *out; return 1 on success, 0 exhausted. */
int bb_broker_drive_sm_one(SmGenState *gs, DESCR_t *out);

/* CHUNKS-step14b: the active SmGenState (set by bb_broker_drive_sm around
 * each sm_interp_run call).  Used by SM_SUSPEND, SM_LOAD_GLOCAL,
 * SM_STORE_GLOCAL handlers in both sm_interp.c and sm_codegen.c. */
extern SmGenState *g_current_gen_state;

/* CHUNKS-step17i-every-suspend: every-table — id → tree_t* side table populated
 * at lower-time when TT_EVERY is encountered in expression-body lowering.
 * Mirrors CH-17f's g_pl_pred_table pattern (Prolog) for Icon `every`.
 * The SM bytecode contains only the integer id; no tree_t* in any
 * SM_Program::instrs[] payload.  The runtime handler (SM_BB_PUMP_EVERY)
 * looks up by id and constructs the box via icn_bb_build — same boundary
 * CH-17f draws for SM_BB_ONCE_PROC (broker constructs the box from IR;
 * the SM dispatch layer never sees a raw IR pointer).
 *
 * every_table_register(ast) — append AST to table, return id.
 *                              ast is borrowed (lifetime: the IR's lifetime,
 *                              same as g_pl_pred_table's IR refs); table
 *                              is reset by every_table_reset on each compile.
 * every_table_lookup(id)    — return tree_t* for id, or NULL if out of range.
 * every_table_reset()       — clear table; called by sm_program_free path. */
struct tree_t;
int   every_table_register(struct tree_t *ast);
struct tree_t *every_table_lookup(int id);
void  every_table_reset(void);

#endif /* SM_INTERP_H */
