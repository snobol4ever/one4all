/* sm_templates.h — forward declarations for all SM template functions.
   Include in emit_core.c so emit_wasm_from_sm (and future unified walkers) can call them.
   EC-UNI-8.1: each function is now defined in its own SM_templates/sm_<opcode>.c file
   (one TU per opcode, mirroring BB_templates/ layout). Section comments below group
   functions by the original family file for review convenience only — they no longer
   correspond to physical files. */
#pragma once
#include "emit_core.h"
#include "SM_templates/sm_ctx.h"
#include "sm_prog.h"
/* group: push/pop literals & variables */
void sm_push_lit_i(const SM_Instr * instr, FILE * out);
void sm_push_lit_s(const SM_Instr * instr, FILE * out);
void sm_push_lit_f(const SM_Instr * instr, FILE * out);
void sm_push_null (const SM_Instr * instr, FILE * out);
void sm_void_pop  (const SM_Instr * instr, FILE * out);
void sm_push_var  (const SM_Instr * instr, FILE * out);
void sm_store_var (const SM_Instr * instr, FILE * out);
/* group: arithmetic */
void sm_concat    (const SM_Instr * instr, FILE * out);
void sm_neg       (const SM_Instr * instr, FILE * out);
void sm_coerce_num(const SM_Instr * instr, FILE * out);
void sm_exp       (const SM_Instr * instr, FILE * out);
void sm_add       (const SM_Instr * instr, FILE * out);
void sm_sub       (const SM_Instr * instr, FILE * out);
void sm_mul       (const SM_Instr * instr, FILE * out);
void sm_div       (const SM_Instr * instr, FILE * out);
void sm_mod       (const SM_Instr * instr, FILE * out);
/* group: compare & stno */
void sm_stno      (const SM_Instr * instr, FILE * out);
void sm_acomp     (const SM_Instr * instr, FILE * out);
void sm_lcomp     (const SM_Instr * instr, FILE * out);
/* group: control flow (jump/halt/return) */
int  sm_jump      (const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_jump_s    (const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_jump_f    (const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_halt      (const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_return    (const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_freturn   (const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_nreturn   (const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out);
/* group: SM_PAT_* + SM_EXEC_STMT */
void sm_pat_lit          (const SM_Instr * instr, FILE * out);
void sm_pat_any          (const SM_Instr * instr, FILE * out);
void sm_pat_any_i        (const SM_Instr * instr, int i, FILE * out);
void sm_pat_notany       (const SM_Instr * instr, int i, FILE * out);
void sm_pat_span         (const SM_Instr * instr, int i, FILE * out);
void sm_pat_break        (const SM_Instr * instr, int i, FILE * out);
void sm_pat_len          (const SM_Instr * instr, FILE * out);
void sm_pat_pos          (const SM_Instr * instr, FILE * out);
void sm_pat_rpos         (const SM_Instr * instr, FILE * out);
void sm_pat_tab          (const SM_Instr * instr, FILE * out);
void sm_pat_rtab         (const SM_Instr * instr, FILE * out);
void sm_pat_arb          (const SM_Instr * instr, FILE * out);
void sm_pat_rem          (const SM_Instr * instr, FILE * out);
void sm_pat_bal          (const SM_Instr * instr, FILE * out);
void sm_pat_fence0       (const SM_Instr * instr, FILE * out);
void sm_pat_fence1       (const SM_Instr * instr, FILE * out);
void sm_pat_abort        (const SM_Instr * instr, FILE * out);
void sm_pat_fail         (const SM_Instr * instr, FILE * out);
void sm_pat_succeed      (const SM_Instr * instr, FILE * out);
void sm_pat_eps          (const SM_Instr * instr, FILE * out);
void sm_pat_deref        (const SM_Instr * instr, FILE * out);
void sm_pat_arbno        (const SM_Instr * instr, FILE * out);
void sm_pat_cat          (const SM_Instr * instr, FILE * out);
void sm_pat_alt          (const SM_Instr * instr, FILE * out);
void sm_pat_refname      (const SM_Instr * instr, FILE * out);
void sm_pat_capture      (const SM_Instr * instr, FILE * out);
void sm_pat_capture_fn   (const SM_Instr * instr, FILE * out);
void sm_pat_capture_fn_args(const SM_Instr * instr, FILE * out);
void sm_pat_usercall     (const SM_Instr * instr, FILE * out);
void sm_pat_usercall_args(const SM_Instr * instr, FILE * out);
void sm_exec_stmt        (const SM_Instr * instr, FILE * out);
