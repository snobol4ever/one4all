#ifndef SM_PROG_H
#define SM_PROG_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef enum {
    SM_LABEL = 0,
    SM_JUMP,
    SM_JUMP_S,
    SM_JUMP_F,
    SM_HALT,
    SM_STNO,
    SM_PUSH_LIT_S,
    SM_PUSH_LIT_CS,
    SM_PUSH_LIT_I,
    SM_PUSH_LIT_F,
    SM_PUSH_NULL,
    SM_PUSH_NULL_NOFLIP,
    SM_PUSH_VAR,
    SM_PUSH_EXPR,    /* push DT_E frozen expression; a[0].ptr = tree_t* */
    SM_PUSH_EXPRESSION,
    SM_CALL_EXPRESSION,
    SM_STORE_VAR,
    SM_VOID_POP,
    SM_ADD,
    SM_SUB,
    SM_MUL,
    SM_DIV,
    SM_EXP,
    SM_MOD,
    SM_CONCAT,
    SM_COERCE_NUM,
    SM_NEG,
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
    SM_PAT_ARBNO,
    SM_PAT_REM,
    SM_PAT_BAL,
    SM_PAT_FENCE0,
    SM_PAT_FENCE1,
    SM_PAT_ABORT,
    SM_PAT_FAIL,
    SM_PAT_SUCCEED,
    SM_PAT_EPS,
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
    SM_EXEC_STMT,
    SM_BB_PUMP,
    SM_BB_ONCE,
    SM_BB_EVAL,
    SM_BB_ONCE_PROC,
    SM_BB_PUMP_PROC,
    SM_BB_PUMP_CASE,
    SM_BB_PUMP_SM,
    SM_BB_PUMP_EVERY,
    SM_SUSPEND_VALUE,
    SM_CALL_FN,
    SM_RETURN,
    SM_FRETURN,
    SM_NRETURN,
    SM_RETURN_S,
    SM_RETURN_F,
    SM_FRETURN_S,
    SM_FRETURN_F,
    SM_NRETURN_S,
    SM_NRETURN_F,
    SM_DEFINE_ENTRY,
    SM_DEFINE,
    SM_INCR,
    SM_DECR,
    SM_LCOMP,
    SM_ACOMP,
    SM_SUSPEND,
    SM_LOAD_GLOCAL,
    SM_STORE_GLOCAL,
    SM_ICMP_GT,
    SM_ICMP_LT,
    SM_LOAD_FRAME,
    SM_STORE_FRAME,
    SM_EXEC_BB,
    SM_PUMP_BB,
    SM_OPCODE_COUNT
} sm_opcode_t;
typedef union {
    int64_t     i;
    double      f;
    const char *s;
    int         b;
    void       *ptr;        /* frozen pointer (tree_t* for SM_PUSH_EXPR, etc.) */
} sm_operand_t;
typedef struct {
    int entry_pc;
    int arity;
} SmExpression_t;
#define SM_INTERP_SUSPENDED  1
typedef struct GeneratorState GeneratorState;
#define SM_MAX_OPERANDS 3
typedef struct {
    sm_opcode_t   op;
    sm_operand_t  a[SM_MAX_OPERANDS];
} SM_Instr;
typedef struct {
    SM_Instr    *instrs;
    int          count;
    int          cap;
    const char **stno_labels;
    int          stno_labels_cap;
    int          stno_count;
} SM_Program;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
SM_Program *sm_prog_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void        sm_prog_free(SM_Program *p);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_emit(SM_Program *p, sm_opcode_t op);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_emit_s(SM_Program *p, sm_opcode_t op, const char *s);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_emit_i(SM_Program *p, sm_opcode_t op, int64_t i);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_emit_f(SM_Program *p, sm_opcode_t op, double f);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_emit_ptr(SM_Program *p, sm_opcode_t op, void *ptr);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_emit_si(SM_Program *p, sm_opcode_t op, const char *s, int64_t i);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_emit_sip(SM_Program *p, sm_opcode_t op, const char *s, int64_t i, void *ptr);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_emit_ii(SM_Program *p, sm_opcode_t op, int64_t i0, int64_t i1);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_label(SM_Program *p);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_label_named(SM_Program *p, const char *name);
extern SM_Program *g_current_sm_prog;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_label_pc_lookup(const SM_Program *p, const char *name);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_patch_jump(SM_Program *p, int jump_idx, int target_label);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_stno_label_record(SM_Program *p, int stno, const char *label);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_prog_print(const SM_Program *p, FILE *out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *sm_opcode_name(sm_opcode_t op);
#endif
