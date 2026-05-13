#ifndef EMIT_INSN_H
#define EMIT_INSN_H

#include "emit_defs.h"
#include <stdint.h>

/* Single x86 instruction emitters — binary bytes or text mnemonic per mode */
void bb_insn_mov_eax_imm32(uint32_t imm);
void bb_insn_mov_rax_imm64(uint64_t imm);
void bb_insn_ret(void);
void bb_insn_nop(void);
void bb_insn_call_rax(void);
void bb_insn_jmp_rel8 (bb_label_t *target);
void bb_insn_jmp_rel32(bb_label_t *target);
void bb_insn_jl_rel8  (bb_label_t *target);
void bb_insn_jge_rel8 (bb_label_t *target);
void bb_insn_je_rel8  (bb_label_t *target);
void bb_insn_jne_rel8 (bb_label_t *target);
void bb_insn_jne_rel32(bb_label_t *target);
void bb_insn_je_rel32 (bb_label_t *target);
void bb_insn_jl_rel32 (bb_label_t *target);
void bb_insn_jge_rel32(bb_label_t *target);
void bb_insn_jg_rel32 (bb_label_t *target);
void bb_insn_cmp_esi_imm8 (uint8_t imm);
void bb_insn_cmp_esi_imm32(uint32_t imm);
void bb_insn_movzx_eax_rdi_off8(uint8_t off);
void bb_insn_cmp_al_imm8(uint8_t imm);
void bb_insn_xor_eax_eax(void);
void bb_insn_push_rbp(void);
void bb_insn_pop_rbp(void);
void bb_insn_mov_rbp_rsp(void);
void bb_insn_sub_rsp_imm8(uint8_t imm);
void bb_insn_add_rsp_imm8(uint8_t imm);

/* Mode-aware compound instruction emitters */
void emit_ret          (void);
void emit_push_r10     (void);
void emit_pop_r10      (void);
void emit_test_rax_rax (void);
void emit_test_eax_eax (void);
void emit_mov_rdi_imm64(uint64_t val);
void emit_call_sym_plt (const char *sym, uint64_t fn_fallback);
void emit_mov_esi_imm32(int val);
void emit_add_delta_imm(int v);
void emit_sub_delta_imm(int v);

/* EM-RAW-PURGE: additional insn helpers */
void bb_insn_mov_rsp_rbp(void);
void bb_insn_mov_rdx_imm64(uint64_t v);
void bb_insn_mov_edx_imm32(uint32_t v);
void bb_insn_mov_edi_imm32(uint32_t v);
void bb_insn_mov_rsi_imm64(uint64_t v);
void bb_insn_push_r12(void);
void bb_insn_pop_r12(void);
void bb_insn_inc_r13_disp8(uint8_t disp);
void bb_insn_mov_rcx_imm64(uint64_t v);
void bb_insn_mov_eax_r10mem(void);
void bb_insn_cmp_eax_imm32(uint32_t v);
void bb_insn_mov_eax_mem_rcx(void);
void bb_insn_sub_eax_imm32(uint32_t v);
void bb_insn_mov_ecx_eax(void);
void bb_insn_cmp_eax_ecx(void);
void bb_insn_mov_rax_mem_rcx(void);
void bb_insn_movsxd_rcx_r10mem(void);
void bb_insn_lea_rax_rax_rcx(void);
void bb_insn_mov_rdi_rax(void);
void bb_insn_add_eax_imm32(uint32_t v);
void bb_insn_cmp_eax_mem_rcx(void);

#endif /* EMIT_INSN_H */
