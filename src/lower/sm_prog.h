/*
 * sm_prog.h — SM_Instr / SM_Program types (M-SCRIP-U2)
 *
 * The flat instruction array that SM-LOWER produces from IR.
 * Both the interpreter dispatch loop and the in-memory code generator
 * walk this same array — one dispatches in C, the other emits x86 blobs.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-04-06
 */

#ifndef SM_PROG_H
#define SM_PROG_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ── Opcode enumeration ─────────────────────────────────────────────── */

typedef enum {
    /* Control */
    SM_LABEL = 0,
    SM_JUMP,
    SM_JUMP_S,
    SM_JUMP_F,
    SM_HALT,
    SM_STNO,       /* increment &STCOUNT/&STNO at each statement boundary */

    /* Values */
    SM_PUSH_LIT_S,
    SM_PUSH_LIT_CS,    /* IJ-15: push a cset literal — CSETVAL(canonical) */
    SM_PUSH_LIT_I,
    SM_PUSH_LIT_F,
    SM_PUSH_NULL,
    SM_PUSH_NULL_NOFLIP, /* push null but do NOT clobber last_ok — for TT_SCAN value-balance */
    SM_PUSH_VAR,
    SM_PUSH_EXPR,    /* push DT_E frozen expression; a[0].ptr = tree_t* */
    SM_PUSH_EXPRESSION,   /* push DT_E expression descriptor; a[0].i=entry_pc, a[1].i=arity */
    SM_CALL_EXPRESSION,   /* pop expression descriptor, push return frame, jump to entry_pc */
    SM_STORE_VAR,
    SM_VOID_POP,

    /* Arithmetic / String */
    SM_ADD,
    SM_SUB,
    SM_MUL,
    SM_DIV,
    SM_EXP,
    SM_MOD,   /* OC-1 RS-6: integer/real modulo — all languages use this */
    SM_CONCAT,
    SM_COERCE_NUM, /* unary +: coerce top of stack to int or real */
    SM_NEG,

    /* Pattern construction */
    SM_PAT_LIT,
    SM_PAT_ANY,
    SM_PAT_NOTANY,
    SM_PAT_SPAN,
    SM_PAT_BREAK,
    SM_PAT_LEN,
    SM_PAT_POS,
    SM_PAT_RPOS,
    SM_PAT_TAB,
    SM_PAT_RTAB,
    SM_PAT_ARB,
    SM_PAT_ARBNO,    /* pop inner pat, push ARBNO(inner) */
    SM_PAT_REM,
    SM_PAT_BAL,
    SM_PAT_FENCE0,
    SM_PAT_FENCE1,  /* pop child pat, push pat_fence_p(child) — FENCE(P) */
    SM_PAT_ABORT,
    SM_PAT_FAIL,
    SM_PAT_SUCCEED,
    SM_PAT_EPS,         /* push epsilon pattern onto pat-stack */
    SM_PAT_ALT,
    SM_PAT_CAT,
    SM_PAT_DEREF,
    SM_PAT_REFNAME,     /* *var in pattern context — a[0].s = var name; push pat_ref(name)
                         * onto pat-stack.  Unlike SM_PUSH_VAR + SM_PAT_DEREF which fetches
                         * the variable's CURRENT value at pattern-build time (wrong for
                         * self-recursive patterns like primary = ... | '(' *primary ')'),
                         * this opcode preserves the name and defers lookup to match time
                         * via XDSAR / bb_deferred_var.  Mirrors the --ir-run pat_ref(name)
                         * path in interp_eval_pat's TT_DEFER(TT_VAR) case.  SN-6 fix. */
    SM_PAT_CAPTURE,
    SM_PAT_CAPTURE_FN,  /* . *func() — a[0].s=funcname; calls func(matched_text) at match time */
    SM_PAT_CAPTURE_FN_ARGS, /* . *func(args) / $ *func(args) — args-are-values form.
                         * a[0].s = funcname, a[1].i = kind (0=cond, 1=imm), a[2].i = nargs.
                         * The nargs values were pushed onto the value stack by preceding
                         * lower_expr calls; handler pops them (last-pushed = last arg) and
                         * calls pat_assign_callcap(child, fname, values, nargs).  SN-8a.
                         * Emitted when any arg is not a plain TT_VAR (TT_QLIT literal, nested
                         * expression, etc.) — the TL-2 name-stash path handles the all-TT_VAR
                         * case in SM_PAT_CAPTURE_FN. */
    SM_PAT_USERCALL,    /* bare *func() — a[0].s=funcname; a[2].s = '\t'-separated arg names (or NULL)
                         * Builds XATP deferred-usercall pattern via pat_user_call; at match time
                         * the engine invokes func() per position and the call's FAIL propagates
                         * as pattern FAIL.  SN-17a. */
    SM_PAT_USERCALL_ARGS, /* bare *func(args) — args-are-values form.
                         * a[0].s = funcname, a[1].i = nargs.  The nargs values were pushed
                         * onto the value stack; handler pops them and calls
                         * pat_user_call(fname, values, nargs).  SN-8a.
                         * Emitted when any arg is not a plain TT_VAR. */

    /* Statement execution */
    SM_EXEC_STMT,

    /* Byrd box broker modes — U-16
     * SM_BB_PUMP: pops bb_node_t* from value stack; calls bb_broker(root,BB_PUMP,body_fn,arg);
     *             pushes tick count as DT_I.  For Icon 'every' / generator loops.
     * SM_BB_ONCE: pops bb_node_t* from value stack; calls bb_broker(root,BB_ONCE,NULL,NULL);
     *             sets st->last_ok.  For Prolog goal dispatch.
     * SM_BB_EVAL: a[0].i = every_table id (tree_t* registered at lower-time, no GC clone).
     *             Calls bb_eval_value(expr); pushes DESCR_t result; sets last_ok.
     *             For Icon A|B (TT_ALTERNATE) in value context.
     * Note: BB_SCAN is already wired via SM_EXEC_STMT → exec_stmt → bb_broker(BB_SCAN). */
    SM_BB_PUMP,
    SM_BB_ONCE,
    SM_BB_EVAL,
    /* CH-17f: Prolog goal dispatch identified by predicate key + arity.
     * Replaces the legacy lower_expr(TT_CHOICE) + SM_BB_ONCE wrapper that
     * pushed a raw tree_t* and called icn_bb_build(TT_CHOICE) at runtime.
     * a[0].s = predicate key ("name/arity"), a[1].i = arity.
     * Handler: pl_pred_entry_lookup(key) → if entry_pc >= 0 use
     * pl_box_choice_pc; else fall back to pl_box_choice(IR).
     * Drive via bb_broker(BB_ONCE); sets st->last_ok.
     * No tree_t* is pushed or walked by this opcode at the SM layer. */
    SM_BB_ONCE_PROC,
    /* CHUNKS-step12: BB pump for an Icon user-proc identified by name + nargs.
     * Replaces the synthesised TT_FNC + emit_push_expr + SM_BB_PUMP wrapper
     * that sm_lower used to emit for the top-level call_main(). a[0].s = proc
     * name, a[1].i = nargs. Args (when nargs>0) are popped from the value
     * stack in caller order and passed straight to the coroutine staging
     * machinery — no tree_t* is constructed at lowering time and none is
     * walked by this opcode. The IR walk that remains lives entirely inside
     * coro_call(proc_table[i].proc, ...) and is Step 17's territory. */
    SM_BB_PUMP_PROC,
    /* CHUNKS-step13: Raku CASE dispatch — replaces the synthesised
     * emit_push_expr + SM_BB_PUMP wrapper that sm_lower used to emit for
     * TT_CASE.  a[0].i = ncases (number of when-arms), a[1].i = has_default
     * (0 or 1).  Stack layout at entry (deepest first):
     *   topic_expression           (DT_E, evaluates the case topic)
     *   cmp_kind_0   (DT_I, tree_e value: TT_LEQ for string ==, else TT_EQ)
     *   val_expression_0           (DT_E, evaluates arm 0's "when" value)
     *   body_expression_0          (DT_E, evaluates arm 0's body)
     *   ... ncases triples ...
     *   default_body_expression    (DT_E, only if has_default)
     * Handler pops in reverse, evaluates topic, walks arms doing string
     * or integer compare per cmp_kind, runs the matching body's expression,
     * or the default if present, or pushes NULVCL.  No tree_t is
     * constructed at lowering time and none is walked by this opcode. */
    SM_BB_PUMP_CASE,
    /* CHUNKS-step15: BB pump for a generator expression — replaces the legacy
     * emit_push_expr + SM_BB_PUMP pair for migrated Icon generator kinds
     * (TT_TO, TT_TO_BY in CH-15a; later kinds in subsequent rungs).  Pops
     * an expression descriptor (DT_E, slen=1, i=entry_pc) from TOS, allocates
     * an SmGenState rooted at entry_pc, and drives the expression with
     * bb_broker_drive_sm(gs, pump_print, NULL) — same statement-context
     * print semantics as SM_BB_PUMP for un-migrated kinds.  No tree_t
     * is constructed at lowering time and none is walked by this opcode;
     * the expression body lowers to pure SM with explicit SM_SUSPEND points. */
    SM_BB_PUMP_SM,

    /* CHUNKS-step17i-every-suspend: BB pump for TT_EVERY by id —
     * mirrors CH-17f's SM_BB_ONCE_PROC pattern (Prolog) for Icon `every`.
     * a[0].i = every_id  (index into g_every_table; populated by sm_lower
     *                     when TT_EVERY is encountered in expression-body
     *                     lowering; NEVER an tree_t* in SM bytecode).
     * Runtime handler does: g_every_table[id] → icn_bb_build → bb_broker(BB_PUMP).
     * Net stack delta: pushes one DT_NUL (every is void in stmt context;
     * the trailing SM_VOID_POP from the proc-body loop discards it).
     *
     * The legacy `emit_push_expr + SM_BB_PUMP` shape (which pushed a raw
     * tree_t* on the SM stack and called icn_bb_build at runtime) underflowed
     * the stack when reached via sm_call_proc (CH-17g) — net push 0,
     * trailing SM_VOID_POP fires, underflow.  This opcode pushes a single
     * DT_NUL at end so the trailing VOID_POP is balanced. */
    SM_BB_PUMP_EVERY,

    /* CHUNKS-step17i-suspend: TT_SUSPEND `suspend E [do body]` — yield-to-caller.
     *
     * Stack discipline: pops one value (the yield value).  Pushes nothing.
     * The lowering shape (lower.c TT_SUSPEND case) wraps it as:
     *
     *   [lower expr]                ; push v, last_ok = !IS_FAIL(v)
     *   SM_JUMP_F  L_end            ; v failed → leave it on stack, fall to L_end
     *   SM_SUSPEND_VALUE            ; pop v, yield (swapcontext to caller)
     *                               ; on resume, control returns here
     *   [lower do-clause if any]    ; push d
     *   SM_VOID_POP                 ; discard d (do-clause is stmt-context)
     *   SM_PUSH_NULL                ; placeholder for outer SM_VOID_POP
     *   SM_JUMP    L_finally
     *   L_end:                      ; failed-v path: stack already has v
     *   L_finally:                  ; outer proc-body SM_VOID_POP fires here
     *
     * Runtime handler: if active_coro != NULL (running inside proc_trampoline /
     * gather_trampoline), set active_coro->yielded = v and swapcontext to
     * caller_ctx — exactly the same yield protocol as icn_bb_suspend
     * (icon_gen.c:211–240).  When the caller resumes us, control returns
     * naturally to the next SM instruction.  If active_coro is NULL
     * (top-level suspend, semantically rare and not in current corpus),
     * push v back as a fallback — the outer SM_VOID_POP will discard it.
     *
     * Why this shape rather than CH-17i-every's g_table+icn_bb_build+broker
     * pattern: TT_SUSPEND-as-statement's existing semantics
     * (icn_stmt.c:88) are entirely in-frame state mutation
     * (FRAME.suspending=1 + suspend_val) followed by an outer-loop
     * swapcontext.  No bb_node_t is constructed, no broker is driven.
     * The SM-side equivalent is a direct yield primitive.  This matches
     * JCON's ir_Succeed (irgen.icn:962, 970) — a yield primitive that
     * saves resume PC and returns to caller. */
    SM_SUSPEND_VALUE,

    /* Functions */
    SM_CALL_FN,
    SM_RETURN,
    SM_FRETURN,
    SM_NRETURN,
    SM_RETURN_S,    /* :S(RETURN)  — return normal value only if last_ok */
    SM_RETURN_F,    /* :F(RETURN)  — return normal value only if !last_ok */
    SM_FRETURN_S,   /* :S(FRETURN) — return FAIL only if last_ok */
    SM_FRETURN_F,   /* :F(FRETURN) — return FAIL only if !last_ok */
    SM_NRETURN_S,   /* :S(NRETURN) — return NAME only if last_ok */
    SM_NRETURN_F,   /* :F(NRETURN) — return NAME only if !last_ok */
    /* ME-6a: emitted by sm_lower immediately after the define-entry SM_LABEL.
     * In mode-3 the blob does `push rbp ; mov rbp, rsp` only when
     * STATE->jit_in_call == 1 (i.e. entered via SM_CALL_FN dispatch, not a
     * plain goto back to the function's own label).  Blob always clears
     * jit_in_call.  In mode-2 and mode-1 this is a no-op. */
    SM_DEFINE_ENTRY,
    SM_DEFINE,

    /* Integer arithmetic */
    SM_INCR,
    SM_DECR,
    SM_LCOMP,
    SM_ACOMP,


    SM_SUSPEND,

    /* CHUNKS-step14b: gen-local slot access — read/write SmGenState->locals[N].
     * a[0].i = slot index (0..SM_GEN_LOCAL_MAX-1).  Only meaningful inside a
     * generator expression being driven by bb_broker_drive_sm; outside that
     * context (g_current_gen_state == NULL) SM_LOAD_GLOCAL pushes FAILDESCR
     * and SM_STORE_GLOCAL is a no-op (with last_ok cleared).  These slots
     * survive SUSPEND/RESUME because the SmGenState is the persistent
     * envelope — they are the per-invocation equivalent of the closure-state
     * struct that icn_bb_to et al. allocate fresh per icn_bb_build call. */
    SM_LOAD_GLOCAL,
    SM_STORE_GLOCAL,

    /* CHUNKS-step15a: SM_ICMP_GT — integer compare greater-than.
     * Pops two DT_I values (right = TOS, left = TOS-1).
     * Sets last_ok = (left.i > right.i).  Pushes nothing.
     * Used by TT_TO / TT_TO_BY generator expressions for the loop-exit test. */
    SM_ICMP_GT,
    /* CHUNKS-step15a: SM_ICMP_LT — integer compare less-than (mirror of GT).
     * Used by TT_TO_BY expressions for the negative-step loop-exit test. */
    SM_ICMP_LT,

    /* CHUNKS-step17b'' (CH-17b''): frame-slot read/write — read/write
     * IcnFrame.env[slot] of the active Icon frame.
     * a[0].i = slot index (0..FRAME_SLOT_MAX-1).
     *
     * Semantics inside an Icon frame (frame_depth > 0):
     *   SM_LOAD_FRAME  : push frame_stack[frame_depth-1].env[slot]; last_ok=1.
     *   SM_STORE_FRAME : pop value, write to that slot, push value back; last_ok=1.
     *
     * Outside an Icon frame (frame_depth == 0): push FAILDESCR / clear last_ok
     * (mirrors SM_LOAD_GLOCAL's behaviour outside a generator drive).  This is
     * safe because expressions emitted with frame-slot ops are only reachable via
     * the expression-shaped consumer dispatch (CH-17c+); until then they are dead
     * code, forward-jumped over by their enclosing SM_JUMP.
     *
     * The slot index is baked at lower-time by sm_lower's per-proc scope
     * construction (mirrors icn_runtime.c's icn_scope_patch but without
     * mutating tree_t.v.ival in place).  See lower.c expression-body emission. */
    SM_LOAD_FRAME,
    SM_STORE_FRAME,

    /* LR-3: SM_EXEC_BB — drive a compile-time wired IR_block_t* once.
     * Replaces SM_BB_EVAL(tree_t*) for the new DCG pipeline (GOAL-LOWER-REDESIGN).
     * a[0].ptr = IR_block_t* (compile-time wired generator DCG, GC-pinned).
     * Calls IR_exec_once(cfg) → DESCR_t; pushes result; sets st->last_ok.
     * Nothing emits this opcode yet — added here so the enum is stable and
     * the interp handler slot exists.  Emitters follow in LR-S1 (SNOBOL4 pats),
     * LR-6 (Icon), LR-10 (Prolog), LR-12 (Snocone). */
    SM_EXEC_BB,
    /* LR-3: SM_PUMP_BB — drive a compile-time wired IR_block_t* to exhaustion.
     * For generative contexts (every, while, gather): replaces SM_BB_PUMP+table-id.
     * a[0].ptr = IR_block_t* (same BB pointer as SM_EXEC_BB).
     * a[1].i  = body entry_pc in SM_Program (int — body block executed per value).
     * Calls IR_exec_pump(cfg, body_fn) until graph exhausted; pushes tick count
     * as DT_I.  Nothing emits this opcode yet — stub handler only. */
    SM_PUMP_BB,

    SM_OPCODE_COUNT
} sm_opcode_t;

/* ── Instruction operand union ──────────────────────────────────────── */

typedef union {
    int64_t     i;          /* integer literal / label index / nargs */
    double      f;          /* float literal */
    const char *s;          /* string literal / variable name / label name */
    int         b;          /* boolean (has_repl etc.) */
    void       *ptr;        /* frozen pointer (tree_t* for SM_PUSH_EXPR, etc.) */
} sm_operand_t;

/* ── Compiled expression descriptor ──────────────────────────────────────── */
/* Replaces raw tree_t* in DT_E descriptors once a site is migrated away
 * from SM_PUSH_EXPR.  entry_pc indexes SM_Program::instrs[]; arity is the
 * number of args expected on the SM value stack at entry (0 for a thunk).
 * SM_PUSH_EXPRESSION encodes these as a[0].i and a[1].i respectively. */
typedef struct {
    int entry_pc;   /* index into SM_Program::instrs[] where expression starts */
    int arity;      /* args on SM value stack at entry; 0 = thunk */
} SmExpression_t;

/* CHUNKS-step14: return code from sm_interp_run when SM_SUSPEND fires.
 * Normal halt = 0; error = -1; suspended = SM_INTERP_SUSPENDED. */
#define SM_INTERP_SUSPENDED  1

/* CHUNKS-step14: SmGenState is defined in sm_interp.h (requires DESCR_t from snobol4.h).
 * Forward-declare here so sm_prog.h users can hold SmGenState* pointers. */
typedef struct SmGenState SmGenState;

#define SM_MAX_OPERANDS 3

typedef struct {
    sm_opcode_t   op;
    sm_operand_t  a[SM_MAX_OPERANDS];   /* a[0], a[1], a[2] */
} SM_Instr;

/* ── SM_Program (flat array of SM instructions produced by lower()) ── */

typedef struct {
    SM_Instr    *instrs;
    int          count;
    int          cap;
    /* IM-9: per-statement source label (1-based; stno_labels[0] unused).
     * stno_labels[n] = source label of statement n, or NULL if unlabelled.
     * Populated by lower(); strings are interned (not owned). */
    const char **stno_labels;
    int          stno_labels_cap;   /* allocated slots (indices 0..cap-1) */
    int          stno_count;        /* number of statements lowered */
} SM_Program;

/* ── Builder helpers ────────────────────────────────────────────────── */

SM_Program *sm_prog_new(void);
void        sm_prog_free(SM_Program *p);

/* Append one instruction; returns its index */
int sm_emit(SM_Program *p, sm_opcode_t op);
int sm_emit_s(SM_Program *p, sm_opcode_t op, const char *s);
int sm_emit_i(SM_Program *p, sm_opcode_t op, int64_t i);
int sm_emit_f(SM_Program *p, sm_opcode_t op, double f);
int sm_emit_ptr(SM_Program *p, sm_opcode_t op, void *ptr);
int sm_emit_si(SM_Program *p, sm_opcode_t op, const char *s, int64_t i);
int sm_emit_sip(SM_Program *p, sm_opcode_t op, const char *s, int64_t i, void *ptr);
int sm_emit_ii(SM_Program *p, sm_opcode_t op, int64_t i0, int64_t i1);

/* Label: emit SM_LABEL with index=next_instr; return the label index */
int sm_label(SM_Program *p);

/* Label with name: emit SM_LABEL and store label name in a[0].s for runtime lookup.
 * Returns the label index (same as sm_label). RS-9a: needed for SM call frames. */
int sm_label_named(SM_Program *p, const char *name);

/* RS-9b: global pointer to the active SM_Program, set by scrip.c after lower().
 * Allows _usercall_hook to detect SM-bodied functions without the IR tree. */
extern SM_Program *g_current_sm_prog;

/* Look up the PC (SM_LABEL instr index) for a named label. Returns -1 if not found.
 * RS-9a: used by SM_CALL_FN to jump to user-defined function bodies. */
int sm_label_pc_lookup(const SM_Program *p, const char *name);

/* Patch a jump target: set a[0].i of instr at `jump_idx` to `target_label` */
void sm_patch_jump(SM_Program *p, int jump_idx, int target_label);

/* IM-9: record source label string for statement stno (1-based); NULL = unlabelled */
void sm_stno_label_record(SM_Program *p, int stno, const char *label);
void sm_prog_print(const SM_Program *p, FILE *out);

/* Opcode name for diagnostics */
const char *sm_opcode_name(sm_opcode_t op);

#endif /* SM_PROG_H */
