/* sm_templates.h — forward declarations for all SM template functions.
   Include in emit_core.c so emit_wasm_from_sm (and future unified walkers) can call them.
   EC-UNI-8.1: each function is defined in its own SM_templates/sm_<opcode>.c file
   (one TU per opcode, mirroring BB_templates/ layout). Section comments below group
   functions by the original family file for review convenience only — they no longer
   correspond to physical files.
   EC-UNI-10(b)(c): every top-level SM template is parameterless and reads g_emit.* —
   the four pat-with-i variants (any/notany/span/break) read g_emit.i instead of taking
   `int i`.  See emit_globals.h for the g_emit struct shape. */
#pragma once
#include "emit_core.h"
#include "emit_globals.h"
#include "SM.h"
/* group: push/pop literals & variables */
void sm_push_lit_i(void);
void sm_push_lit_s(void);
void sm_push_lit_f(void);
void sm_push_null (void);
void sm_push_null_noflip(void);
void sm_void_pop  (void);
void sm_push_var  (void);
void sm_store_var (void);
/* group: arithmetic */
void sm_concat    (void);
void sm_neg       (void);
void sm_coerce_num(void);
void sm_exp       (void);
void sm_add       (void);
void sm_sub       (void);
void sm_mul       (void);
void sm_div       (void);
void sm_mod       (void);
/* group: compare & stno */
void sm_stno      (void);
void sm_acomp     (void);
void sm_lcomp     (void);
/* group: control flow (jump/halt/return) — EC-UNI-10(b): parameterless, read from g_emit */
int  sm_jump      (void);
int  sm_jump_s    (void);
int  sm_jump_f    (void);
int  sm_halt      (void);
int  sm_return    (void);
int  sm_freturn   (void);
int  sm_nreturn   (void);
void sm_label     (void);
/* group: SM_PAT_* + SM_EXEC_STMT — EC-UNI-10(c): parameterless, read from g_emit */
void sm_pat_lit          (void);
void sm_pat_any          (void);
void sm_pat_any_i        (void);
void sm_pat_notany       (void);
void sm_pat_span         (void);
void sm_pat_break        (void);
void sm_pat_len          (void);
void sm_pat_pos          (void);
void sm_pat_rpos         (void);
void sm_pat_tab          (void);
void sm_pat_rtab         (void);
void sm_pat_arb          (void);
void sm_pat_rem          (void);
void sm_pat_bal          (void);
void sm_pat_fence0       (void);
void sm_pat_fence1       (void);
void sm_pat_abort        (void);
void sm_pat_fail         (void);
void sm_pat_succeed      (void);
void sm_pat_eps          (void);
void sm_pat_deref        (void);
void sm_pat_arbno        (void);
void sm_pat_cat          (void);
void sm_pat_alt          (void);
void sm_pat_refname      (void);
void sm_pat_capture      (void);
void sm_pat_capture_fn   (void);
void sm_pat_capture_fn_args(void);
void sm_pat_usercall     (void);
void sm_pat_usercall_args(void);
void sm_exec_stmt        (void);
/* group: calls — EC-UNI-13(b): SM_CALL_FN and SM_SUSPEND_VALUE.  Return value matches
 * the sm_jump/sm_halt convention (1 = arm produced a terminal jump that consumes the
 * silo walker's per-iteration fallthrough; 0 = walker should emit its own next-pc step). */
int  sm_call_fn          (void);
int  sm_suspend_value    (void);
/* group: define — EC-UNI-13(c): SM_DEFINE_ENTRY and SM_DEFINE.  Same return-value
 * convention as sm_call_fn, but all arms currently return 0 (no terminal jump). */
int  sm_define_entry     (void);
int  sm_define           (void);
/* group: bb_calls — EC-UNI-13(d): SM_BB_ONCE_PROC and SM_BB_PUMP_PROC.  Today only
 * the IS_X86 arm carries logic (PJ-9c Prolog rt_pl_once + IJ-HELLO-3 Icon proc
 * direct call); IS_JVM/JS/NET/WASM are honest no-op stubs matching the silo
 * walkers' `default: break;` fallthrough.  Return 0 (no terminal jump). */
int  sm_bb_once_proc     (void);
int  sm_bb_pump_proc     (void);
/* group: expr/incr family — EC-UNI-14(c)(4).  SM_PUSH_EXPR may be dead in practice (no live
 * lowering path observed to emit it across icon/prolog/snobol4/snocone/rebus/raku gates) but the
 * lower.c emit_push_expr fn still exists, so the template stays for completeness.  SM_INCR/DECR
 * are emitted only by sm_interp_test.c — also vestigial.  PUSH_EXPRESSION/CALL_EXPRESSION are
 * live (beauty.sno emits them).  Return void (no terminal jump). */
void sm_push_expr        (void);
void sm_push_expression  (void);
void sm_call_expression  (void);
void sm_incr             (void);
void sm_decr             (void);
