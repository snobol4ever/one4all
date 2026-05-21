/* bb_templates.h — forward declarations for all BB template functions.
   Include this in emit_core.c so emit_bb_node can call them.
   Each function is defined in BB_templates/bb_<kind>.c.
   EC-UNI-10(c): all top-level BB templates are parameterless and read g_emit.node / g_emit.out.
   bb_capture keeps `int imm` as a genuine call-site discriminator (BB_PAT_ASSIGN_IMM vs
   BB_PAT_ASSIGN_COND), mirroring the sm_pat_any_i(int i) precedent in the remaining SM
   family files. */
#pragma once
#include "emit_core.h"
#include "emit_globals.h"
#include "emit_ir.h"
#include <stdio.h>

void bb_lit    (void);
void bb_any    (void);
void bb_notany (void);
void bb_span   (void);
void bb_break  (void);
void bb_arb    (void);
void bb_arbno  (void);
void bb_cat    (void);
void bb_alt    (void);
void bb_len    (void);
void bb_pos    (void);
void bb_tab    (void);
void bb_rem    (void);
void bb_fence  (void);
void bb_abort  (void);
void bb_capture(int imm);
void bb_pl_arith  (void);
void bb_pl_atom   (void);
void bb_pl_builtin(void);
void bb_pl_call   (void);
/* EC-UNI BB layer total over BB_op_t: 76 stub kinds added in one pass. */
void bb_lit_i                  (void);
void bb_lit_s                  (void);
void bb_lit_f                  (void);
void bb_lit_nul                (void);
void bb_var                    (void);
void bb_assign                 (void);
void bb_augop                  (void);
void bb_binop                  (void);
void bb_unop                   (void);
void bb_call                   (void);
void bb_seq                    (void);
void bb_fail                   (void);
void bb_succeed                (void);
void bb_goto                   (void);
void bb_return                 (void);
void bb_if                     (void);
void bb_alternate              (void);
void bb_to_by                  (void);
void bb_every                  (void);
void bb_while                  (void);
void bb_until                  (void);
void bb_repeat                 (void);
void bb_ctl_alt                (void);
void bb_size                   (void);
void bb_case                   (void);
void bb_limit                  (void);
void bb_suspend                (void);
void bb_proc                   (void);
void bb_scan                   (void);
void bb_nonnull                (void);
void bb_interrogate            (void);
void bb_not                    (void);
void bb_pat_callout            (void);
void bb_pl_choice              (void);
void bb_pl_unify               (void);
void bb_pl_cut                 (void);
void bb_pl_var                 (void);
void bb_pl_alt                 (void);
void bb_pl_seq                 (void);
void bb_icn_to                 (void);
void bb_icn_upto               (void);
void bb_icn_to_by              (void);
void bb_icn_iterate            (void);
void bb_icn_alternate          (void);
void bb_icn_limit              (void);
void bb_icn_binop              (void);
void bb_icn_to_nested          (void);
void bb_icn_proc_gen           (void);
void bb_ctl_break              (void);
void bb_next                   (void);
void bb_identical              (void);
void bb_null_test              (void);
void bb_random                 (void);
void bb_neg                    (void);
void bb_ctl_pos                (void);
void bb_cset_compl             (void);
void bb_cset_union             (void);
void bb_cset_diff              (void);
void bb_cset_inter             (void);
void bb_icn_scan               (void);
void bb_icn_keyword            (void);
void bb_binop_gen              (void);
void bb_icn_idx                (void);
void bb_icn_section            (void);
void bb_icn_list_bang          (void);
void bb_icn_record_def         (void);
void bb_icn_field_get          (void);
void bb_icn_field_set          (void);
void bb_icn_idx_set            (void);
void bb_icn_key_gen            (void);
void bb_swap                   (void);
void bb_seq_expr               (void);
void bb_initial                (void);
void bb_icn_lconcat            (void);
void bb_icn_find_gen           (void);
void bb_icn_seq_gen            (void);
