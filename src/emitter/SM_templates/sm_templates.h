/* sm_templates.h — forward declarations for all SM template functions.
   Include in emit_core.c so emit_wasm_from_sm (and future unified walkers) can call them.
   EC-UNI-8.1: each function is now defined in its own SM_templates/sm_<opcode>.c file
   (one TU per opcode, mirroring BB_templates/ layout). Section comments below group
   functions by the original family file for review convenience only — they no longer
   correspond to physical files. */
#pragma once
#include "emit_core.h"
#include "SM_templates/sm_ctx.h"
#include "SM.h"
/* group: push/pop literals & variables */
void sm_push_lit_i(const SM_t * instr, FILE * out);
void sm_push_lit_s(const SM_t * instr, FILE * out);
void sm_push_lit_f(const SM_t * instr, FILE * out);
void sm_push_null (const SM_t * instr, FILE * out);
void sm_void_pop  (const SM_t * instr, FILE * out);
void sm_push_var  (const SM_t * instr, FILE * out);
void sm_store_var (const SM_t * instr, FILE * out);
/* group: arithmetic */
void sm_concat    (const SM_t * instr, FILE * out);
void sm_neg       (const SM_t * instr, FILE * out);
void sm_coerce_num(const SM_t * instr, FILE * out);
void sm_exp       (const SM_t * instr, FILE * out);
void sm_add       (const SM_t * instr, FILE * out);
void sm_sub       (const SM_t * instr, FILE * out);
void sm_mul       (const SM_t * instr, FILE * out);
void sm_div       (const SM_t * instr, FILE * out);
void sm_mod       (const SM_t * instr, FILE * out);
/* group: compare & stno */
void sm_stno      (const SM_t * instr, FILE * out);
void sm_acomp     (const SM_t * instr, FILE * out);
void sm_lcomp     (const SM_t * instr, FILE * out);
/* group: control flow (jump/halt/return) */
int  sm_jump      (const SM_t * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_jump_s    (const SM_t * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_jump_f    (const SM_t * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_halt      (const SM_t * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_return    (const SM_t * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_freturn   (const SM_t * instr, const sm_ctx_t * ctx, FILE * out);
int  sm_nreturn   (const SM_t * instr, const sm_ctx_t * ctx, FILE * out);
/* group: SM_PAT_* + SM_EXEC_STMT */
void sm_pat_lit          (const SM_t * instr, FILE * out);
void sm_pat_any          (const SM_t * instr, FILE * out);
void sm_pat_any_i        (const SM_t * instr, int i, FILE * out);
void sm_pat_notany       (const SM_t * instr, int i, FILE * out);
void sm_pat_span         (const SM_t * instr, int i, FILE * out);
void sm_pat_break        (const SM_t * instr, int i, FILE * out);
void sm_pat_len          (const SM_t * instr, FILE * out);
void sm_pat_pos          (const SM_t * instr, FILE * out);
void sm_pat_rpos         (const SM_t * instr, FILE * out);
void sm_pat_tab          (const SM_t * instr, FILE * out);
void sm_pat_rtab         (const SM_t * instr, FILE * out);
void sm_pat_arb          (const SM_t * instr, FILE * out);
void sm_pat_rem          (const SM_t * instr, FILE * out);
void sm_pat_bal          (const SM_t * instr, FILE * out);
void sm_pat_fence0       (const SM_t * instr, FILE * out);
void sm_pat_fence1       (const SM_t * instr, FILE * out);
void sm_pat_abort        (const SM_t * instr, FILE * out);
void sm_pat_fail         (const SM_t * instr, FILE * out);
void sm_pat_succeed      (const SM_t * instr, FILE * out);
void sm_pat_eps          (const SM_t * instr, FILE * out);
void sm_pat_deref        (const SM_t * instr, FILE * out);
void sm_pat_arbno        (const SM_t * instr, FILE * out);
void sm_pat_cat          (const SM_t * instr, FILE * out);
void sm_pat_alt          (const SM_t * instr, FILE * out);
void sm_pat_refname      (const SM_t * instr, FILE * out);
void sm_pat_capture      (const SM_t * instr, FILE * out);
void sm_pat_capture_fn   (const SM_t * instr, FILE * out);
void sm_pat_capture_fn_args(const SM_t * instr, FILE * out);
void sm_pat_usercall     (const SM_t * instr, FILE * out);
void sm_pat_usercall_args(const SM_t * instr, FILE * out);
void sm_exec_stmt        (const SM_t * instr, FILE * out);
