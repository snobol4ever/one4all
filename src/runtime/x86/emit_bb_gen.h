
#ifndef EMITTER_BB_GEN_H
#define EMITTER_BB_GEN_H

#include "emit_defs.h"
#include "bb_pool.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

extern bb_emit_mode_t bb_emit_mode;

extern int g_bb_emit_format;
extern FILE          *bb_emit_out;

/* emit_mode_set — central setter for emit pass mode. */
void emit_mode_set(bb_emit_mode_t m, FILE *out);

/* Templates and template-helpers call these directly.  Each helper consults */
void emit_comment(const char *text);
void emit_bb_box_banner(const char *kind, const char *args);

void emit_bb_inc_mem_r13_disp8(uint8_t disp);
void emit_pad_to_blob_size(void);
void emit_macro_begin(const char *name, const char *const *params, int nparams);
void emit_macro_end(void);

void bb_label_init(bb_label_t *lbl, const char *name);

void bb_label_initf(bb_label_t *lbl, const char *fmt, ...);

void bb_label_define(bb_label_t *lbl);

void emit_jmp(bb_label_t *target, jmp_kind_t kind);

void emit_lea_rdi_strtab_sym(const char *sym_label, uint64_t in_proc_ptr);
void emit_lea_rdx_strtab_sym(const char *sym_label, uint64_t in_proc_ptr);
void emit_mov_edi_imm32(int val);
void emit_mov_edx_imm32(int val);
void emit_movabs_rdi_entry(uint64_t entry_ptr);
void emit_call_sym_param(const char *sym_or_param);
void emit_jz_retskip(int pc);
void emit_retskip_label(int pc);
void emit_noop_macro(const char *macro_name);

void emit_banner_stno(int stno, int lineno, const char *src_text);

void emit_label_define(bb_label_t *lbl);

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

extern bb_buf_t  bb_emit_buf;
extern int       bb_emit_pos;
extern int       bb_emit_size;

void bb_emit_begin(bb_buf_t buf, int size);

int  bb_emit_end(void);

extern bb_patch_t bb_patch_list[BB_PATCH_MAX];
extern int        bb_patch_count;

void bb_emit_patch_rel8 (bb_label_t *lbl);
void bb_emit_patch_rel32(bb_label_t *lbl);

void bb_emit_byte(uint8_t b);
void bb_emit_u16 (uint16_t v);
void bb_emit_u32 (uint32_t v);
void bb_emit_u64 (uint64_t v);
void bb_emit_i8  (int8_t   v);
void bb_emit_i32 (int32_t  v);

void bb_insn_mov_eax_imm32(uint32_t imm);
void bb_insn_mov_rax_imm64(uint64_t imm);
void bb_insn_ret(void);
void bb_insn_nop(void);
void bb_insn_call_rax(void);
void bb_insn_jmp_rel8(bb_label_t *target);
void bb_insn_jmp_rel32(bb_label_t *target);
void bb_insn_jl_rel8 (bb_label_t *target);
void bb_insn_jge_rel8(bb_label_t *target);
void bb_insn_je_rel8 (bb_label_t *target);
void bb_insn_jne_rel8(bb_label_t *target);
void bb_insn_je_rel32 (bb_label_t *target);
void bb_insn_jl_rel32 (bb_label_t *target);
void bb_insn_jge_rel32(bb_label_t *target);
void bb_insn_jne_rel32(bb_label_t *target);
void bb_insn_jg_rel32 (bb_label_t *target);
void bb_insn_cmp_esi_imm8(uint8_t imm);
void bb_insn_cmp_esi_imm32(uint32_t imm);
void bb_insn_movzx_eax_rdi_off8(uint8_t off);
void bb_insn_cmp_al_imm8(uint8_t imm);
void bb_insn_xor_eax_eax(void);
void bb_insn_push_rbp(void);
void bb_insn_pop_rbp(void);
void bb_insn_mov_rbp_rsp(void);
void bb_insn_sub_rsp_imm8(uint8_t imm);
void bb_insn_add_rsp_imm8(uint8_t imm);

void bb_text(const char *fmt, ...);

void bb_text_label(bb_label_t *lbl);

void bb_text_comment(const char *fmt, ...);

/* ── BB three-column line emission (EM-7c-bb-three-column) ────────────────── */
void bb3c_text(const char *label, const char *action, const char *goto_);
void bb3c_format(FILE *out, const char *label, const char *action, const char *goto_);

void bb3c_flush_pending(void);

void bb3c_flush_pending_cjmp_only(void);

/* EM-FORMAT-BB-FUSED-GOTOS (2026-05-09): */
void bb3c_emit_jmp(FILE *out, const char *mn, const char *target);

/* ── instruction emitters (bb_emit_mode-aware; renamed from bb_emit_* EM-DEVTABLE) ─ */
void emit_ret            (void);
void emit_push_r10       (void);
void emit_pop_r10        (void);
void emit_test_rax_rax   (void);
void emit_test_eax_eax   (void);
void emit_mov_rdi_imm64  (uint64_t val);
void emit_call_sym_plt   (const char *sym, uint64_t fn_fallback);
void emit_mov_esi_imm32  (int val);
void emit_add_delta_imm  (int v);
void emit_sub_delta_imm  (int v);

#endif
