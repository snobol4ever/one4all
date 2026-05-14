#ifndef RUNTIME_X86_TEMPLATES_EMIT_TEMPLATES_H
#define RUNTIME_X86_TEMPLATES_EMIT_TEMPLATES_H
#include "snobol4.h"
#include "emit.h"
#include "snobol4_patnd.h"
/*---- SM scalar templates -------------------------------------------------*/
/* SM_HALT — pc++ then ret.  Mode-3 in-process; mode-4: rt_halt_tos@PLT. */
void emit_sm_halt           ();
/* SM_PUSH_LIT_I — movabs rdi, val; call rt_push_int@PLT. */
void emit_sm_push_lit_i     (int64_t val);
void emit_sm_push_lit_f     (double val);
void emit_sm_push_lit_s     (const char * str_lbl, uint64_t str_ptr, int len);
void emit_sm_push_var       (const char * name_lbl, uint64_t name_ptr);
void emit_sm_store_var      (const char * name_lbl, uint64_t name_ptr);
void emit_sm_push_expr      (uint64_t ptr_val);
void emit_sm_push_expression(uint64_t entry_ptr, int arity);
void emit_sm_call_expression(const char * tgt_sym);
void emit_sm_exec_stmt      (const char * subj_lbl, uint64_t subj_ptr, int has_repl);
void emit_sm_call_fn        (const char * name_lbl, uint64_t name_ptr, int nargs);
void emit_sm_coerce_num     ();
void emit_sm_exp            ();         void emit_sm_neg            ();
void emit_sm_incr           (int64_t n);void emit_sm_decr           (int64_t n);
void emit_sm_acomp          (int op);   void emit_sm_lcomp          (int op);
void emit_sm_define         ();         void emit_sm_define_entry   ();
/*---- SM jump templates ---------------------------------------------------*/
void emit_sm_jump           (int target_pc);
void emit_sm_jump_s         (int target_pc);
void emit_sm_jump_f         (int target_pc);
/*---- SM label / stno markers ---------------------------------------------*/
/* SM_LABEL / SM_STNO structural markers. */
void emit_sm_label          ();
void emit_sm_stno           (int stno, int lineno, const char * src_text);
/*---- SM arith dispatch ---------------------------------------------------*/
/* SM arithmetic family (ADD/SUB/MUL/DIV/MOD). */
void emit_sm_arith_op       (int op_enum, const char * macro_name);
void emit_sm_op             (int op);
void emit_sm_arith_dispatch (int op);
/*---- SM return family ----------------------------------------------------*/
/* SM_RETURN / SM_RETURN_VARIANT family. */
void emit_sm_return         ();
void emit_sm_return_variant (int kind, int cond, int pc);
void emit_sm_freturn        (int pc);   void emit_sm_nreturn        (int pc);
void emit_sm_return_s       (int pc);   void emit_sm_return_f       (int pc);
void emit_sm_freturn_s      (int pc);   void emit_sm_freturn_f      (int pc);
void emit_sm_nreturn_s      (int pc);   void emit_sm_nreturn_f      (int pc);
/*---- SM generator / coroutine templates ----------------------------------*/
void emit_sm_suspend        ();         void emit_sm_resume         ();
void emit_sm_suspend_value  ();         void emit_sm_gen_tick       ();
void emit_sm_bb_pump        ();         void emit_sm_bb_once        ();
void emit_sm_bb_once_proc   ();         void emit_sm_bb_pump_proc   ();
void emit_sm_bb_pump_case   ();         void emit_sm_bb_pump_sm     ();
void emit_sm_bb_pump_every  ();         void emit_sm_bb_pump_ast    ();
void emit_sm_unhandled_op   (int op);
void emit_sm_load_glocal    ();         void emit_sm_store_glocal   ();
void emit_sm_icmp_gt        ();         void emit_sm_icmp_lt        ();
void emit_sm_load_frame     ();         void emit_sm_store_frame    ();
/*---- SM_PAT_* opcode templates -------------------------------------------*/
void emit_sm_pat_eps        ();         void emit_sm_pat_arb        ();
void emit_sm_pat_rem        ();         void emit_sm_pat_fail       ();
void emit_sm_pat_succeed    ();         void emit_sm_pat_abort      ();
void emit_sm_pat_bal        ();         void emit_sm_pat_fence      ();
void emit_sm_pat_fence1     ();         void emit_sm_pat_span       ();
void emit_sm_pat_break      ();         void emit_sm_pat_any        ();
void emit_sm_pat_notany     ();         void emit_sm_pat_len        ();
void emit_sm_pat_pos        ();         void emit_sm_pat_rpos       ();
void emit_sm_pat_tab        ();         void emit_sm_pat_rtab       ();
void emit_sm_pat_arbno      ();         void emit_sm_pat_cat        ();
void emit_sm_pat_alt        ();         void emit_sm_pat_deref      ();
void emit_sm_pat_lit        (const char * name_lbl, uint64_t name_ptr);
void emit_sm_pat_refname    (const char * name_lbl, uint64_t name_ptr);
void emit_sm_pat_usercall   (const char * name_lbl, uint64_t name_ptr);
void emit_sm_pat_capture        (const char * name_lbl, uint64_t name_ptr, int kind);
void emit_sm_pat_usercall_args  (const char * name_lbl, uint64_t name_ptr, int nargs);
void emit_sm_pat_capture_fn     (const char * fname_lbl, uint64_t fname_ptr, int is_imm,
                                  const char * namelist_lbl, uint64_t namelist_ptr);
void emit_sm_pat_capture_fn_args(const char * fname_lbl, uint64_t fname_ptr, int is_imm, int nargs);
/*---- BB box templates (stateless) ----------------------------------------*/
#include "emit_bb.h"
/* XCHR — literal-string-match box. */
void emit_bb_xchr (PATND_t * p, const char * lit_label,
                   bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xeps (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xfail(bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xfarb(bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xstar(bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xabrt(bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xsucf(bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xbal (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xfnce(bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xvar (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xcat (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xor  (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
/*---- BB box templates (charset / int-cursor) -----------------------------*/
void emit_bb_charset(bb_box_fn c_fn, const char * c_fn_name, const char * kind_name, const char * chars,
                     bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xlnth  (long long num, bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xtb    (long long num, bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xrtb   (long long num, bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xposi  (int n,         bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xrpsi  (int n,         bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
/* XBRKX (BREAKX) — break-from-variable box. */
void emit_bb_xbrkx(const char * chars,
                   bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
/*---- BB box templates (named / call) -------------------------------------*/
void emit_bb_xatp    (const char * varname,
                      bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xdsar   (const char * varname,
                      bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xnme    (bb_box_fn child_fn, const char * varname,
                      bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xfnme   (bb_box_fn child_fn, const char * varname,
                      bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xcallcap(bb_box_fn child_fn, const char * fnc_name,
                      bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_xarbn   (bb_box_fn child_fn,
                      bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
/*---- BB box templates (Icon generators) ----------------------------------*/
void emit_bb_icon_to     (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_icon_to_by  (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_icon_iterate(bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_icon_alt    (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_icon_every  (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_icon_limit  (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_icon_bang   (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_icon_lconcat(bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
void emit_bb_icon_seq    (bb_label_t * lbl_succ, bb_label_t * lbl_fail, bb_label_t * lbl_β);
#endif
