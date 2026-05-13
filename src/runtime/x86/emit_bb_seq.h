/* emit_bb_seq.h — L3: compound BB helper sequences.
 *
 * These functions combine L0–L2 primitives into multi-instruction sequences
 * used by BB box templates (L4).  Each function handles all four emit modes.
 *
 * Calls: emit_buf.h (L0), emit_form.h (L1), emit_text3c.h (L2),
 *        emit_insn.h (L2), emit_mode.h (L2), emit_label.h (L2).
 */

#ifndef EMIT_BB_SEQ_H
#define EMIT_BB_SEQ_H

#include "emit_defs.h"
#include <stdint.h>

/* ---- inc/frame helpers ------------------------------------------------ */
void emit_bb_inc_mem_r13_disp8      (uint8_t disp);
void emit_push_rbp_frame            (void);
void emit_pop_rbp_frame_ret         (void);

/* ---- brokered ABI helpers --------------------------------------------- */
void emit_brokered_prologue         (void);
void emit_brokered_epilogue_ret     (int result);

/* ---- strtab / symbol lea helpers -------------------------------------- */
void emit_lea_rdi_strtab_sym        (const char *sym_label, uint64_t in_proc_ptr);
void emit_lea_rdx_strtab_sym        (const char *sym_label, uint64_t in_proc_ptr);
void emit_lea_rsi_strtab_sym        (const char *sym_label, uint64_t in_proc_ptr);

/* ---- imm32 move helpers ----------------------------------------------- */
void emit_mov_edi_imm32             (int val);
void emit_mov_edx_imm32             (int val);

/* ---- retskip helpers -------------------------------------------------- */
void emit_jz_retskip                (int pc);
void emit_retskip_label             (int pc);

/* ---- entry / call helpers --------------------------------------------- */
void emit_movabs_rdi_entry          (uint64_t entry_ptr);
void emit_call_sym_param            (const char *sym_or_param);
void emit_noop_macro                (const char *macro_name);

/* ---- port-call helpers ------------------------------------------------ */
void emit_bb_port_call(uint64_t zeta_ptr, const char *fn_name, uint64_t fn_fallback,
                       int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail);
void emit_bb_port_call_rip(uint64_t zeta_ptr, const char *zeta_label,
                           const char *fn_name, uint64_t fn_fallback,
                           int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail);

/* ---- delta/sigma comparison helpers ----------------------------------- */
void emit_load_delta_cmp_imm        (int n, bb_label_t *lbl_succ, bb_label_t *lbl_fail);
void emit_load_siglen_sub_cmp_delta (int n, uint64_t siglen_addr,
                                     bb_label_t *lbl_succ, bb_label_t *lbl_fail);
void emit_sigma_plus_delta_to_rdi   (uint64_t sigma_addr, uint64_t siglen_addr);
void emit_bounds_check_delta_plus_len(int len, uint64_t siglen_addr, bb_label_t *lbl_fail);

#endif /* EMIT_BB_SEQ_H */
