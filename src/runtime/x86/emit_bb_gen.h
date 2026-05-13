
#ifndef EMITTER_BB_GEN_H
#define EMITTER_BB_GEN_H

#include "emit_defs.h"
#include "emit_buf.h"
#include "emit_label.h"
#include "emit_text3c.h"
#include "emit_insn.h"
#include "bb_pool.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

extern bb_emit_mode_t bb_emit_mode;

extern int g_bb_emit_format;
extern FILE          *bb_emit_out;

/* emit_mode_set — central setter for emit pass mode. */
void emit_mode_set(bb_emit_mode_t m, FILE *out);

extern int g_in_text_macro_body;
FILE *emit_outf(void);
void bb3c_op (const char *mn, const char *fmt, ...);
void bb3c_jmp(const char *mn, const char *target);

void emit_jmp(bb_label_t *target, jmp_kind_t kind);
void emit_bb_inc_mem_r13_disp8(uint8_t disp);
void emit_pad_to_blob_size(void);
void emit_macro_begin(const char *name, const char *const *params, int nparams);
void emit_macro_end(void);

void emit_lea_rdi_strtab_sym(const char *sym_label, uint64_t in_proc_ptr);
void emit_lea_rdx_strtab_sym(const char *sym_label, uint64_t in_proc_ptr);
void emit_mov_edi_imm32(int val);
void emit_mov_edx_imm32(int val);
void emit_movabs_rdi_entry(uint64_t entry_ptr);
void emit_call_sym_param(const char *sym_or_param);
void emit_jz_retskip(int pc);
void emit_retskip_label(int pc);
void emit_noop_macro(const char *macro_name);

void emit_bb_port_call(uint64_t zeta_ptr, const char *fn_name, uint64_t fn_fallback,
                    int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail);

/* emit_bb_port_call_rip — like emit_bb_port_call but TEXT/INLINE mode uses */
void emit_bb_format_port(bb_label_t *lbl_entry, const char *macro_name, const char *args);

int  emit_bb_is_format_mode(void);
void fmt_body_append(const char *instr, const char *operands);

void emit_bb_port_call_rip(uint64_t zeta_ptr, const char *zeta_label,
                        const char *fn_name, uint64_t fn_fallback,
                        int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail);

void emit_load_delta_cmp_imm(int n, bb_label_t *lbl_succ, bb_label_t *lbl_fail);

void emit_load_siglen_sub_cmp_delta(int n, uint64_t siglen_addr,
                                 bb_label_t *lbl_succ, bb_label_t *lbl_fail);

void emit_lea_rsi_strtab_sym(const char *sym_label, uint64_t in_proc_ptr);

void emit_sigma_plus_delta_to_rdi(uint64_t sigma_addr, uint64_t siglen_addr);

void emit_bounds_check_delta_plus_len(int len, uint64_t siglen_addr, bb_label_t *lbl_fail);

void emit_brokered_prologue(void);

void emit_brokered_epilogue_ret(int result);

void emit_push_rbp_frame(void);

void emit_pop_rbp_frame_ret(void);

void bb_emit_patch_rel8 (bb_label_t *lbl);
void bb_emit_patch_rel32(bb_label_t *lbl);

#endif
