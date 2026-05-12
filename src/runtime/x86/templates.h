/*
 * templates/templates.h — declarations for per-opcode / per-box templates.
 *
 * One declaration per template function.  Templates live in
 * `templates/{sm,bb}_<name>.c` files.  Callers (sm_codegen.c for
 * mode-3 binary; sm_codegen_x64_emit.c for mode-4 text; future
 * regen_macros tool for sm_macros.s) include this header and invoke
 * the appropriate template with an emitter_t constructed for their
 * target backend.
 *
 * Sub-rung -c lands the first template: emit_sm_halt.  Sub-rungs -d
 * through -p add more, one per rung, alternating SM ↔ BB.
 *
 * Authors: Lon Jones Cherryholmes · Claude Opus 4.7
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-c / GOAL-MODE4-EMIT
 */

#ifndef RUNTIME_X86_TEMPLATES_TEMPLATES_H
#define RUNTIME_X86_TEMPLATES_TEMPLATES_H

/* Dependency pull-ins for everything this header declares.  Putting
 * them at the top of the header (rather than scattered between
 * declarations) means callers get all the types they need by including
 * this one header. */
#include "snobol4.h"          /* DESCR_t, SPEC_t (transitive deps of patnd) */
#include "emitter.h"          /* emitter_t */
#include "bb_emit.h"          /* bb_label_t */
#include "snobol4_patnd.h"    /* PATND_t */

/* ── SM opcode templates ──────────────────────────────────────────────── */

/* SM_HALT — pc++ then ret.  Mode-3 in-process; mode-4: rt_halt_tos@PLT.
 * See templates/sm_halt.c for Option C sanctioned-exception rationale. */
void emit_sm_halt(emitter_t *e);

/* SM_PUSH_LIT_I — movabs rdi, val; call rt_push_int@PLT.
 * No mode-3/mode-4 divergence; template used by mode-4; mode-3 still
 * uses Standard blob pending ME-4+ (sub-rung -f, sess 2026-05-11). */
void emit_sm_push_lit_i(emitter_t *e, int64_t val);

/* SM_VOID_POP — call rt_pop_void@PLT; discard TOS.
 * No mode-3/mode-4 divergence; template used by mode-4; mode-3 still
 * uses Standard blob (sub-rung -h, sess 2026-05-11). */
void emit_sm_void_pop(emitter_t *e);

/* ── BB box templates ─────────────────────────────────────────────────── */

/* XCHR — literal-string-match box.  Sub-rung -d (2026-05-11).
 * EM-TEMPLATE-PURITY-3: lit_label added; is_text guards removed. */
void emit_bb_xchr(emitter_t *e, PATND_t *p,
                  const char *lit_label,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                  bb_label_t *lbl_β);

/* Charset-family (SPAN/BREAK/ANY/NOTANY) — sub-rung -e (2026-05-11).
 * EM-TEMPLATE-PURITY-2: callback params removed; t_bb_port_call used. */
#include "bb_flat.h"   /* bb_box_fn */

void emit_bb_charset(emitter_t *e,
                     bb_box_fn c_fn,
                     const char *c_fn_name,
                     const char *kind_name,
                     const char *chars,
                     bb_label_t *lbl_succ,
                     bb_label_t *lbl_fail,
                     bb_label_t *lbl_β);

/* Integer-cursor family (LEN/TAB/RTAB) — sub-rung -g (2026-05-11).
 * EM-TEMPLATE-PURITY-2: callback params removed; t_bb_port_call used. */
void emit_bb_intcur(emitter_t *e,
                    bb_box_fn c_fn,
                    const char *c_fn_name,
                    const char *kind_name,
                    long long num,
                    bb_label_t *lbl_succ,
                    bb_label_t *lbl_fail,
                    bb_label_t *lbl_β);
void emit_bb_xlnth(emitter_t *e, long long num,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xtb  (emitter_t *e, long long num,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xrtb (emitter_t *e, long long num,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);

/* XBRKX (BREAKX) — break-from-variable box. Sub-rung -i (2026-05-11).
 * EM-TEMPLATE-PURITY-2: callback params removed; t_bb_port_call used. */
void emit_bb_xbrkx(emitter_t *e,
                   const char *chars,
                   bb_label_t *lbl_succ,
                   bb_label_t *lbl_fail,
                   bb_label_t *lbl_β);

/* SM_JUMP family — sub-rung -j (2026-05-11). */
void emit_sm_jump  (emitter_t *e, int target_pc);
void emit_sm_jump_s(emitter_t *e, int target_pc);
void emit_sm_jump_f(emitter_t *e, int target_pc);

/* XPOSI/XRPSI (POS/RPOS) — sub-rung -k (2026-05-11). */
void emit_bb_xposi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xrpsi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);

/* SM arithmetic family (ADD/SUB/MUL/DIV/MOD) — sub-rung -l (2026-05-11).
 * op_enum: SM opcode integer; macro_name: GAS macro name (ADD_NUM etc.). */
void emit_sm_arith_op(emitter_t *e, int op_enum, const char *macro_name);

/* XEPS/XFAIL/XFARB (EPS/FAIL/ARB) — sub-rung -m (2026-05-11). */
void emit_bb_xeps (emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xfail(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xfarb(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);

/* SM nullary RT-call family — sub-rung -n (2026-05-11).
 * SM_CONCAT:     pop right+left, push concat result → rt_concat()
 * SM_PUSH_NULL:  push null descriptor               → rt_push_null()
 * SM_COERCE_NUM: coerce TOS string to number        → rt_coerce_num() */
void emit_sm_concat    (emitter_t *e);
void emit_sm_push_null (emitter_t *e);
void emit_sm_coerce_num(emitter_t *e);

/* SM_LABEL / SM_STNO structural markers — sub-rung -o (2026-05-11).
 * BINARY: no-op (label system places .LpcN; no x86 bytes).
 * TEXT:   LABEL → one three-column LABEL macro line.
 *         STNO  → 120-char #= banner + STNO macro line. */
void emit_sm_label(emitter_t *e);
void emit_sm_stno (emitter_t *e, int stno, int lineno, const char *src_text);

/* SM_CALL_FN — sub-rung -p (2026-05-11).
 *   name_lbl:    strtab label string (e.g. ".S42") for TEXT/MACRO_DEF
 *   name_ptr:    in-process string pointer for BINARY
 *   nargs:       number of arguments on the value stack
 * Emits: lea rdi, [rip+lbl]; mov esi, nargs; call rt_call@PLT */
void emit_sm_call_fn(emitter_t *e, const char *name_lbl,
                     uint64_t name_ptr, int nargs);

/* SM_RETURN / SM_RETURN_VARIANT family — sub-rung -q (2026-05-11).
 * emit_sm_return: MACRO_DEF source of truth for RETURN macro (ret).
 * emit_sm_return_variant: MACRO_DEF source of truth for RETURN_VARIANT macro
 *   (mov edi,kind; mov esi,cond; call rt_do_return@PLT; test/jz/ret/label).
 * TEXT dispatch uses sm_emit_ret / sm_emit_ret_var (proven path). */
void emit_sm_return(emitter_t *e);
void emit_sm_return_variant(emitter_t *e, int kind, int cond, int pc);

/* SM_PAT_* opcode templates — sub-rung -r (2026-05-11).
 *
 * Nullary group (sm_pat_nullary.c) — no args at call site; pop value(s)
 * from stack, call rt_pat_*, push result pattern. */
void emit_sm_pat_eps        (emitter_t *e);
void emit_sm_pat_arb        (emitter_t *e);
void emit_sm_pat_rem        (emitter_t *e);
void emit_sm_pat_fail       (emitter_t *e);
void emit_sm_pat_succeed    (emitter_t *e);
void emit_sm_pat_abort      (emitter_t *e);
void emit_sm_pat_bal        (emitter_t *e);
void emit_sm_pat_fence      (emitter_t *e);
void emit_sm_pat_fence1     (emitter_t *e);
void emit_sm_pat_span       (emitter_t *e);
void emit_sm_pat_break      (emitter_t *e);
void emit_sm_pat_any        (emitter_t *e);
void emit_sm_pat_notany     (emitter_t *e);
void emit_sm_pat_len        (emitter_t *e);
void emit_sm_pat_pos        (emitter_t *e);
void emit_sm_pat_rpos       (emitter_t *e);
void emit_sm_pat_tab        (emitter_t *e);
void emit_sm_pat_rtab       (emitter_t *e);
void emit_sm_pat_arbno      (emitter_t *e);
void emit_sm_pat_cat        (emitter_t *e);
void emit_sm_pat_alt        (emitter_t *e);
void emit_sm_pat_deref      (emitter_t *e);

/* String-arg group (sm_pat_lbl.c) — one strtab label argument. */
void emit_sm_pat_lit        (emitter_t *e, const char *name_lbl, uint64_t name_ptr);
void emit_sm_pat_refname    (emitter_t *e, const char *name_lbl, uint64_t name_ptr);
void emit_sm_pat_usercall   (emitter_t *e, const char *name_lbl, uint64_t name_ptr);

/* String+int group (sm_pat_capture.c) — strtab label + integer. */
void emit_sm_pat_capture        (emitter_t *e, const char *name_lbl, uint64_t name_ptr, int kind);
void emit_sm_pat_usercall_args  (emitter_t *e, const char *name_lbl, uint64_t name_ptr, int nargs);

/* Three-arg group (sm_pat_capture_fn.c) — fname + is_imm + namelist/nargs. */
void emit_sm_pat_capture_fn     (emitter_t *e,
                                  const char *fname_lbl, uint64_t fname_ptr,
                                  int is_imm,
                                  const char *namelist_lbl, uint64_t namelist_ptr);
void emit_sm_pat_capture_fn_args(emitter_t *e,
                                  const char *fname_lbl, uint64_t fname_ptr,
                                  int is_imm, int nargs);

/* SM_PUSH_VAR / SM_STORE_VAR — sub-rung -s (2026-05-11).
 * Both take a strtab label (SM_TPL_LBL): lea rdi,[rip+\lbl]; call rt@PLT. */
void emit_sm_push_var  (emitter_t *e, const char *name_lbl, uint64_t name_ptr);
void emit_sm_store_var (emitter_t *e, const char *name_lbl, uint64_t name_ptr);

/* SM_PUSH_LIT_S — sub-rung -s (2026-05-11).
 * SM_TPL_LBL_INT32: lea rdi,[rip+\lbl]; mov esi,\n; call rt_push_str@PLT. */
void emit_sm_push_lit_s(emitter_t *e,
                         const char *str_lbl, uint64_t str_ptr, int len);

/* SM_PUSH_EXPRESSION / SM_CALL_EXPRESSION / SM_EXEC_STMT — sub-rung -s.
 * MACRO_DEF source of truth for the expression/statement execution macros. */
void emit_sm_push_expression(emitter_t *e, uint64_t entry_ptr, int arity);
void emit_sm_call_expression (emitter_t *e, const char *tgt_sym);
void emit_sm_exec_stmt       (emitter_t *e,
                               const char *subj_lbl, uint64_t subj_ptr,
                               int has_repl);

/* ── New templates added sess 2026-05-12 (EM-TEMPLATE-COMPLETE) ──────── */

/* SM_PUSH_LIT_F — push real literal via rt_push_real_bits (bit-pattern in rdi). */
void emit_sm_push_lit_f(emitter_t *e, double val);

/* SM_PUSH_EXPR — push frozen DT_E descriptor (ptr in rdi). */
void emit_sm_push_expr(emitter_t *e, uint64_t ptr_val);

/* SM_PUSH_NULL_NOFLIP — push null, preserve last_ok. */
void emit_sm_push_null_noflip(emitter_t *e);

/* SM_EXP / SM_NEG — arithmetic. */
void emit_sm_exp(emitter_t *e);
void emit_sm_neg(emitter_t *e);

/* SM_INCR / SM_DECR — pop TOS, add/sub immediate n, push. */
void emit_sm_incr(emitter_t *e, int64_t n);
void emit_sm_decr(emitter_t *e, int64_t n);

/* SM_ACOMP / SM_LCOMP — comparison, op=EKind. */
void emit_sm_acomp(emitter_t *e, int op);
void emit_sm_lcomp(emitter_t *e, int op);

/* SM_DEFINE_ENTRY / SM_DEFINE — no-op markers. */
void emit_sm_define_entry(emitter_t *e);
void emit_sm_define(emitter_t *e);

/* FRETURN/NRETURN/RETURN_S/F family — all delegate to emit_sm_return_variant. */
void emit_sm_freturn  (emitter_t *e, int pc);
void emit_sm_nreturn  (emitter_t *e, int pc);
void emit_sm_return_s (emitter_t *e, int pc);
void emit_sm_return_f (emitter_t *e, int pc);
void emit_sm_freturn_s(emitter_t *e, int pc);
void emit_sm_freturn_f(emitter_t *e, int pc);
void emit_sm_nreturn_s(emitter_t *e, int pc);
void emit_sm_nreturn_f(emitter_t *e, int pc);

/* M5 generator opcodes — trap via rt_unhandled_sm. */
void emit_sm_suspend       (emitter_t *e);
void emit_sm_resume        (emitter_t *e);
void emit_sm_suspend_value (emitter_t *e);
void emit_sm_gen_tick      (emitter_t *e);
void emit_sm_bb_pump       (emitter_t *e);
void emit_sm_bb_once       (emitter_t *e);
void emit_sm_bb_once_proc  (emitter_t *e);
void emit_sm_bb_pump_proc  (emitter_t *e);
void emit_sm_bb_pump_case  (emitter_t *e);
void emit_sm_bb_pump_sm    (emitter_t *e);
void emit_sm_bb_pump_every (emitter_t *e);
void emit_sm_bb_pump_ast   (emitter_t *e);
void emit_sm_load_glocal   (emitter_t *e);
void emit_sm_store_glocal  (emitter_t *e);
void emit_sm_icmp_gt       (emitter_t *e);
void emit_sm_icmp_lt       (emitter_t *e);
void emit_sm_load_frame    (emitter_t *e);
void emit_sm_store_frame   (emitter_t *e);


/* ── New BB box templates (sess 2026-05-12, EM-TEMPLATE-COMPLETE) ─────── */

/* Nullary-stateful BB boxes (same shape as XFARB/XEPS/XFAIL). */
void emit_bb_xstar(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xabrt(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xsucf(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xbal (emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xfnce(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);

/* Variant — β→fail stub (never flat-eligible). */
void emit_bb_xvar (emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);

/* Composite — structural stubs (handled inline by flat_emit_node). */
void emit_bb_xcat (emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xor  (emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);

/* String-arg stateful boxes. */
void emit_bb_xatp (emitter_t *e, const char *varname,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xdsar(emitter_t *e, const char *varname,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);

/* Child-fn + string boxes (capture family). */
void emit_bb_xnme (emitter_t *e, bb_box_fn child_fn, const char *varname,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xfnme(emitter_t *e, bb_box_fn child_fn, const char *varname,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_xcallcap(emitter_t *e, bb_box_fn child_fn, const char *fnc_name,
                      bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);

/* Child-fn box (ARBNO). */
void emit_bb_xarbn(emitter_t *e, bb_box_fn child_fn,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);

/* Icon generator boxes (GOAL-ICON-BB-NATIVE). */
void emit_bb_icon_to   (emitter_t *e,
                        bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_icon_to_by(emitter_t *e,
                        bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_icon_iterate(emitter_t *e,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_icon_alt(emitter_t *e,
                      bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_icon_every(emitter_t *e,
                        bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_icon_limit(emitter_t *e,
                        bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_icon_bang(emitter_t *e,
                       bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_icon_lconcat(emitter_t *e,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
void emit_bb_icon_seq(emitter_t *e,
                      bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);

#endif /* RUNTIME_X86_TEMPLATES_TEMPLATES_H */
