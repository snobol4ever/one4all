#ifndef EMIT_SM_H
#define EMIT_SM_H
#include "emit_core.h"
#include "sm_prog.h"
#include "emit_bb.h"
#include "IR.h"
#include <stdio.h>
/*---- SM opcode walker (mode-4 text codegen) ------------------------------*/
void emit_sm_selftest(void);
int  emit_walk_codegen(SM_Program * prog, FILE * out, const char * src_path);
/* EC-BB-UNIFY-2: phase-2 simulator returns an IR_t* pattern root built into the caller-supplied cfg arena (NULL on empty window). */
IR_t * emit_walk_phase2(const SM_Program * prog, int phase2_start, int phase2_end, IR_block_t * cfg, int * out_variant);
/*---- flat-glob eligibility -----------------------------------------------*/
int  emit_flat_eligible (const IR_t * nd);
int  emit_flat_invariant(const IR_t * nd);
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
/* EC-UNI-1: x86 GAS text dispatchers — exposed so SM_template fns can call them from IS_X86_TEXT arms. */
int emit_push_lit_i_line       (FILE *out, const SM_Instr *ins, int pc);
int emit_sm_push_lit_s_dispatch(FILE *out, const SM_Instr *ins, int pc);
int emit_sm_push_lit_f_dispatch(FILE *out, const SM_Instr *ins, int pc);
int emit_sm_push_var_dispatch  (FILE *out, const SM_Instr *ins, int pc);
int emit_sm_store_var_dispatch (FILE *out, const SM_Instr *ins, int pc);
int emit_sm_push_null_dispatch (FILE *out, int pc);
int emit_sm_pop                (FILE *out, int pc);
/* EC-UNI-2: arith family x86 dispatchers */
int edp4_sm_arith              (FILE *out, const SM_Instr *ins, int pc);  /* SM_ADD/SUB/MUL/DIV/MOD */
int emit_sm_concat_dispatch    (FILE *out, int pc);
int emit_sm_neg_dispatch       (FILE *out, int pc);
int emit_sm_coerce_num_dispatch(FILE *out, int pc);
int emit_sm_exp_dispatch       (FILE *out, int pc);
/*---- compat macros -------------------------------------------------------*/
#define sm_codegen_text(prog,out,src)  emit_walk_codegen(prog,out,src)
#define flat_is_eligible_node(nd)      emit_flat_eligible(nd)
#define patnd_is_fully_invariant(nd)   emit_flat_invariant(nd)
#endif
