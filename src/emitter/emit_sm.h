#ifndef EMIT_SM_H
#define EMIT_SM_H
#include "emit_core.h"
#include "SM.h"
#include "emit_bb.h"
#include "BB.h"
#include <stdio.h>
/*---- SM opcode walker (mode-4 text codegen) ------------------------------*/
void emit_sm_selftest(void);
int  emit_walk_codegen(SM_sequence_t * prog, FILE * out, const char * src_path);
/* EC-BB-UNIFY-2: phase-2 simulator returns an BB_t* pattern root built into the caller-supplied cfg arena (NULL on empty window). */
BB_t * emit_walk_phase2(const SM_sequence_t * prog, int phase2_start, int phase2_end, BB_graph_t * cfg, int * out_variant);
/*---- flat-glob eligibility -----------------------------------------------*/
int  emit_flat_eligible (const BB_t * nd);
int  emit_flat_invariant(const BB_t * nd);
extern int g_emit_inline;
/*---- SM opcode template dispatch (called by walker) ----------------------*/
void emit_sm_op_label        (int pc);
void emit_sm_op_stno         (int stno, int lineno, const char * src);
void emit_sm_op_halt         (void);         void emit_sm_op_return      (void);
void emit_sm_op_add          (void);         void emit_sm_op_sub         (void);
void emit_sm_op_mul          (void);         void emit_sm_op_div         (void);
void emit_sm_op_mod          (void);         void emit_sm_op_neg         (void);
void emit_sm_op_exp          (void);         void emit_sm_op_coerce_num  (void);
void emit_sm_op_acomp        (int op);       void emit_sm_op_lcomp       (int op);
void emit_sm_op_incr         (int n);        void emit_sm_op_decr        (int n);
void emit_sm_op_jump         (int pc);       void emit_sm_op_jump_s      (int pc);
void emit_sm_op_jump_f       (int pc);
void emit_sm_op_push_lit_i   (int64_t v);    void emit_sm_op_push_lit_f  (double v);
void emit_sm_op_push_lit_s   (const char * sym, int len);
void emit_sm_op_push_var     (const char * sym);
void emit_sm_op_store_var    (const char * sym);
void emit_sm_op_push_expr    (void * ptr);
void emit_sm_op_push_expression(int entry_pc, int arity);
void emit_sm_op_call_expression(int target_pc);
void emit_sm_op_call_fn      (const char * name, int nargs);
void emit_sm_op_exec_stmt    (const char * subj, int has_repl);
void emit_sm_op_define       (void);         void emit_sm_op_define_entry(void);
void emit_sm_op_pat_eps      (void);         void emit_sm_op_pat_arb     (void);
void emit_sm_op_pat_rem      (void);         void emit_sm_op_pat_fail    (void);
void emit_sm_op_pat_succeed  (void);         void emit_sm_op_pat_abort   (void);
void emit_sm_op_pat_bal      (void);         void emit_sm_op_pat_fence   (void);
void emit_sm_op_pat_fence1   (void);         void emit_sm_op_pat_span    (void);
void emit_sm_op_pat_break    (void);         void emit_sm_op_pat_any     (void);
void emit_sm_op_pat_notany   (void);         void emit_sm_op_pat_len     (void);
void emit_sm_op_pat_pos      (void);         void emit_sm_op_pat_rpos    (void);
void emit_sm_op_pat_tab      (void);         void emit_sm_op_pat_rtab    (void);
void emit_sm_op_pat_arbno    (void);         void emit_sm_op_pat_cat     (void);
void emit_sm_op_pat_alt      (void);         void emit_sm_op_pat_deref   (void);
void emit_sm_op_pat_lit      (const char * sym);
void emit_sm_op_pat_refname  (const char * sym);
void emit_sm_op_pat_usercall (const char * sym);
void emit_sm_op_pat_capture  (const char * name, int kind);
void emit_sm_op_pat_usercall_args  (const char * name, int nargs);
void emit_sm_op_pat_capture_fn     (const char * fname);
void emit_sm_op_pat_capture_fn_args(const char * fname, int nargs);
void emit_sm_op_resume       (void);         void emit_sm_op_suspend     (void);
void emit_sm_op_suspend_value(void);         void emit_sm_op_gen_tick    (void);
void emit_sm_op_load_glocal  (void);         void emit_sm_op_store_glocal(void);
void emit_sm_op_load_frame   (void);         void emit_sm_op_store_frame (void);
void emit_sm_op_icmp_gt      (void);         void emit_sm_op_icmp_lt     (void);
void emit_sm_op_bb_once      (void);         void emit_sm_op_bb_once_proc(void);
void emit_sm_op_bb_pump      (void);         void emit_sm_op_bb_pump_case(void);
void emit_sm_op_bb_pump_every(void);         void emit_sm_op_bb_pump_proc(void);
void emit_sm_op_bb_pump_sm   (void);         void emit_sm_op_bb_pump_ast (void);
void emit_sm_op_freturn      (int pc);       void emit_sm_op_nreturn     (int pc);
void emit_sm_op_return_s     (void);         void emit_sm_op_return_f    (void);
void emit_sm_op_freturn_s    (void);         void emit_sm_op_freturn_f   (void);
void emit_sm_op_nreturn_s    (void);         void emit_sm_op_nreturn_f   (void);
void emit_sm_op_unhandled    (int opc);
/* EC-UNI-1: x86 GAS text dispatchers — exposed so SM_template fns can call them from IS_X86 arms. */
int emit_push_lit_i_line       (FILE *out, const SM_t *ins, int pc);
int emit_sm_push_lit_s_dispatch(FILE *out, const SM_t *ins, int pc);
int emit_sm_push_lit_f_dispatch(FILE *out, const SM_t *ins, int pc);
int emit_sm_push_var_dispatch  (FILE *out, const SM_t *ins, int pc);
int emit_sm_store_var_dispatch (FILE *out, const SM_t *ins, int pc);
int emit_sm_push_null_dispatch (FILE *out, int pc);
int emit_sm_push_null_noflip_dispatch(FILE *out, int pc);
int emit_sm_pop                (FILE *out, int pc);
/* EC-UNI-2: arith family x86 dispatchers */
int edp4_sm_arith              (FILE *out, const SM_t *ins, int pc);  /* SM_ADD/SUB/MUL/DIV/MOD */
int emit_sm_concat_dispatch    (FILE *out, int pc);
int emit_sm_neg_dispatch       (FILE *out, int pc);
int emit_sm_coerce_num_dispatch(FILE *out, int pc);
int emit_sm_exp_dispatch       (FILE *out, int pc);
/* EC-UNI-2: compare family x86 dispatchers */
int emit_sm_stno_template      (FILE *out, const SM_t *ins);     /* shim — passes NULL SrcLines */
int emit_sm_acomp_dispatch     (FILE *out, const SM_t *ins, int pc);
int emit_sm_lcomp_dispatch     (FILE *out, const SM_t *ins, int pc);
/* EC-UNI-2: control family x86 dispatchers (jump/halt) */
int emit_halt_line             (FILE *out, int pc);
int emit_sm_jump_line          (FILE *out, const SM_t *ins, int pc);
int emit_sm_jump_s_line        (FILE *out, const SM_t *ins, int pc);
int emit_sm_jump_f_line        (FILE *out, const SM_t *ins, int pc);
int emit_sm_label_dispatch     (FILE *out, const SM_t *ins, int pc);
/* EC-UNI-14(c)(4): un-staticed for use from SM_templates/sm_expr_incr.c arms.  Each is a thin
 * trampoline into emit_sm_* that already exists in emit_sm.c; the templates use them in IS_X86 arms. */
int emit_sm_push_expr_dispatch       (FILE *out, const SM_t *ins, int pc);
int emit_sm_push_expression_dispatch (FILE *out, const SM_t *ins, int pc);
int emit_sm_call_expression_dispatch (FILE *out, const SM_t *ins, int pc);
int emit_sm_incr_dispatch            (FILE *out, const SM_t *ins, int pc);
int emit_sm_decr_dispatch            (FILE *out, const SM_t *ins, int pc);
/* EC-UNI-2: return family x86 dispatchers + shim */
int emit_sm_return_dispatch    (FILE *out, int pc);
int emit_sm_return_variant_dispatch(FILE *out, SM_op_t op, int pc, const SM_sequence_t *prog);
int emit_sm_return_template    (FILE *out, const SM_t *ins);   /* shim — passes NULL SM_sequence_t for NRETURN comment annotation */
/* EC-UNI-2d: pat family x86 dispatchers — uniform `(FILE *, int pc)` dispatchers un-staticed for direct template use. */
int emit_sm_pat_span_dispatch    (FILE *out, int pc);
int emit_sm_pat_break_dispatch   (FILE *out, int pc);
int emit_sm_pat_any_dispatch     (FILE *out, int pc);
int emit_sm_pat_notany_dispatch  (FILE *out, int pc);
int emit_sm_pat_len_dispatch     (FILE *out, int pc);
int emit_sm_pat_pos_dispatch     (FILE *out, int pc);
int emit_sm_pat_rpos_dispatch    (FILE *out, int pc);
int emit_sm_pat_tab_dispatch     (FILE *out, int pc);
int emit_sm_pat_rtab_dispatch    (FILE *out, int pc);
int emit_sm_pat_arb_dispatch     (FILE *out, int pc);
int emit_sm_pat_arbno_dispatch   (FILE *out, int pc);
int emit_sm_pat_rem_dispatch     (FILE *out, int pc);
int emit_sm_pat_fence0_dispatch  (FILE *out, int pc);
int emit_sm_pat_fence1_dispatch  (FILE *out, int pc);
int emit_sm_pat_fail_dispatch    (FILE *out, int pc);
int emit_sm_pat_abort_dispatch   (FILE *out, int pc);
int emit_sm_pat_succeed_dispatch (FILE *out, int pc);
int emit_sm_pat_bal_dispatch     (FILE *out, int pc);
int emit_sm_pat_eps_dispatch     (FILE *out, int pc);
int emit_sm_pat_cat_dispatch     (FILE *out, int pc);
int emit_sm_pat_alt_dispatch     (FILE *out, int pc);
int emit_sm_pat_deref_dispatch   (FILE *out, int pc);
/* EC-UNI-2d: pat family x86 shims — non-uniform dispatchers wrapped to (FILE *, const SM_t *) signature.
 * Private types (sm_op_template_t, emit_sm_args_t, pat_arg_label) stay inside emit_sm.c via these shims. */
int emit_sm_pat_lit_template            (FILE *out, const SM_t *ins);
int emit_sm_pat_refname_template        (FILE *out, const SM_t *ins);
int emit_sm_pat_capture_template        (FILE *out, const SM_t *ins);
int emit_sm_pat_capture_fn_template     (FILE *out, const SM_t *ins);
int emit_sm_pat_capture_fn_args_template(FILE *out, const SM_t *ins);
int emit_sm_pat_usercall_template       (FILE *out, const SM_t *ins);
int emit_sm_pat_usercall_args_template  (FILE *out, const SM_t *ins);
int emit_sm_exec_stmt_template          (FILE *out, const SM_t *ins);
/* EC-UNI-13(b): exposed for SM_templates/sm_calls.c IS_X86 arm.  SM_CALL_FN and
 * SM_SUSPEND_VALUE both route through this (legacy x86 behavior). */
int emit_sm_call_dispatch               (FILE *out, const SM_t *ins, int pc);
/* EC-UNI-13(c): exposed for SM_templates/sm_defines.c IS_X86 arm.  SM_DEFINE_ENTRY
 * emits noop+annotation + push rbp / mov rbp,rsp and sets g_in_define_body=1;
 * SM_DEFINE emits noop+annotation only.  Legacy x86 dispatchers; the new template
 * calls them as a black box (no behaviour change). */
int emit_sm_define_entry_dispatch       (FILE *out, const SM_t *ins, int pc, const SM_sequence_t *prog);
int emit_sm_define_dispatch             (FILE *out, const SM_t *ins, int pc);
extern int g_in_define_body;
/* EC-UNI-13(d): exposed for SM_templates/sm_bb_calls.c IS_X86 arm.  SM_BB_ONCE_PROC
 * routes through rt_pl_once (PJ-9c Prolog predicate invocation); SM_BB_PUMP_PROC
 * emits a direct `call .L<entry_pc>` to the Icon proc's SM-lowered body (IJ-HELLO-3).
 * JVM/JS/NET/WASM arms are no-ops — those backends never emit these opcodes today. */
int emit_sm_bb_once_proc_dispatch       (FILE *out, const SM_t *ins, int pc);
int emit_sm_bb_pump_proc_dispatch       (FILE *out, const SM_t *ins, int pc);
/* EC-UNI-3: feature flag — when non-zero, emit_walk_codegen routes the 52 templated opcodes through
 * SM_template fns (which call the same dispatchers under IS_X86). Byte-identical by construction. */
extern int g_emit_use_unified_dispatch;
/*---- compat macros -------------------------------------------------------*/
#define sm_codegen_text(prog,out,src)  emit_walk_codegen(prog,out,src)
#define flat_is_eligible_node(nd)      emit_flat_eligible(nd)
#define patnd_is_fully_invariant(nd)   emit_flat_invariant(nd)
#endif
