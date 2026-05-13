/* emit_seq.h — RW-2: compound BB helper sequences, new names.
 *
 * Replaces emit_bb_seq.h. No if-statements in any body — only insn_* calls.
 * Old emit_bb_seq.c still compiled alongside; no callers changed in RW-2.
 */

#ifndef EMIT_SEQ_H
#define EMIT_SEQ_H

#include "emit_defs.h"
#include <stdint.h>

/* ---- Frame / prologue / epilogue --------------------------------------- */
void emit_seq_frame_enter       (void);
void emit_seq_frame_leave       (void);
void emit_seq_brokered_enter    (void);
void emit_seq_brokered_leave    (int result);

/* ---- Register-load (lea rXX,[rip+sym] / movabs rXX,ptr) --------------- */
void emit_seq_lea_rdi_sym       (const char *sym, uint64_t ptr);
void emit_seq_lea_rdx_sym       (const char *sym, uint64_t ptr);
void emit_seq_lea_rsi_sym       (const char *sym, uint64_t ptr);
void emit_seq_movabs_rdi        (uint64_t ptr);

/* ---- Immediate-to-register -------------------------------------------- */
void emit_seq_mov_edx_i32       (int val);
void emit_seq_mov_edi_i32       (int val);

/* ---- Cursor / r13 ----------------------------------------------------- */
void emit_seq_inc_r13           (uint8_t disp);

/* ---- Cursor / bounds --------------------------------------------------- */
void emit_seq_cmp_delta_i       (int n, bb_label_t *lbl_succ, bb_label_t *lbl_fail);
void emit_seq_cmp_siglen_delta  (int n, uint64_t siglen_addr,
                                 bb_label_t *lbl_succ, bb_label_t *lbl_fail);
void emit_seq_sigma_delta_rdi   (uint64_t sigma_addr, uint64_t siglen_addr);
void emit_seq_bounds_len        (int len, uint64_t siglen_addr, bb_label_t *lbl_fail);

/* ---- Return-skip / label ---------------------------------------------- */
void emit_seq_jz_retskip        (int pc);
void emit_seq_retskip_label     (int pc);

/* ---- BB wiring --------------------------------------------------------- */
void emit_seq_zeta_rdi          (uint64_t ptr, const char *sym);
void emit_seq_dispatch_jne_jmp  (bb_label_t *lbl_succ, bb_label_t *lbl_fail);

/* ---- Call -------------------------------------------------------------- */
void emit_seq_call_tgt          (const char *sym_or_param);
void emit_seq_noop_macro        (const char *macro_name);
void emit_seq_port_call         (uint64_t zeta_ptr, const char *fn_name,
                                 uint64_t fn_fallback, int port,
                                 bb_label_t *lbl_succ, bb_label_t *lbl_fail);
void emit_seq_port_call_rip     (uint64_t zeta_ptr, const char *zeta_label,
                                 const char *fn_name, uint64_t fn_fallback,
                                 int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail);

#endif /* EMIT_SEQ_H */
