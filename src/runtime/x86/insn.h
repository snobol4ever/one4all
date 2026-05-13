/* insn.h — leaf x86 instruction emitters (RW-1 + RW-6 absorbed emit_insn.c).
 *
 * Each function emits exactly one x86 instruction.
 * Pattern: if (IS_TEXT) { text; return; } / binary bytes below.
 * IS_TEXT / IS_BIN are the only mode checks allowed at this layer.
 *
 * RW-6: emit_insn.c deleted; all functions now here.
 */

#ifndef INSN_H
#define INSN_H

#include "emit_defs.h"
#include "emit_mode.h"
#include <stdint.h>

/* ---- Mode-test macros ------------------------------------------------- */
/* IS_TEXT: true for any output-to-text mode (TEXT, TEXT_INLINE, MACRO_DEF) */
#define IS_TEXT  (bb_emit_mode != EMIT_BINARY_WIRED && \
                  bb_emit_mode != EMIT_BINARY_BROKERED)
#define IS_BIN   (bb_emit_mode == EMIT_BINARY_WIRED || \
                  bb_emit_mode == EMIT_BINARY_BROKERED)
#define IS_WIRED     (bb_emit_mode == EMIT_BINARY_WIRED)
#define IS_BROKERED  (bb_emit_mode == EMIT_BINARY_BROKERED)

/* ---- mov family ------------------------------------------------------- */
void insn_mov_eax_i32       (uint32_t v);
void insn_mov_rax_i64       (uint64_t v);
void insn_mov_rcx_i64       (uint64_t v);
void insn_mov_rdx_i64       (uint64_t v);
void insn_mov_rsi_i64       (uint64_t v);
void insn_mov_rdi_i64       (uint64_t v);
void insn_mov_edx_i32       (uint32_t v);
void insn_mov_edi_i32       (uint32_t v);
void insn_mov_esi_i32       (int v);
void insn_mov_rbp_rsp       (void);
void insn_mov_rsp_rbp       (void);
void insn_mov_ecx_eax       (void);
void insn_mov_rdi_rax       (void);
void insn_mov_eax_r10mem    (void);  /* mov eax, [r10]   — load Δ  */
void insn_mov_eax_rcxmem    (void);  /* mov eax, [rcx]              */
void insn_mov_rax_rcxmem    (void);  /* mov rax, [rcx]              */

/* ---- lea --------------------------------------------------------------- */
void insn_lea_rcx_rip_sym   (const char *sym, uint64_t addr);
void insn_lea_r10_rip_sym   (const char *sym, uint64_t addr);
void insn_lea_rax_rax_rcx   (void);  /* lea rax, [rax+rcx]          */

/* ---- movzx / movsxd --------------------------------------------------- */
void insn_movzx_eax_rdi_off8(uint8_t off);
void insn_movsxd_rcx_r10mem (void);  /* movsxd rcx, dword [r10]    */

/* ---- cmp --------------------------------------------------------------- */
void insn_cmp_esi_i8        (uint8_t v);
void insn_cmp_esi_i32       (uint32_t v);
void insn_cmp_al_i8         (uint8_t v);
void insn_cmp_eax_i32       (uint32_t v);
void insn_cmp_eax_ecx       (void);
void insn_cmp_eax_rcxmem    (void);  /* cmp eax, [rcx]              */

/* ---- add / sub --------------------------------------------------------- */
void insn_add_rsp_i8        (uint8_t v);
void insn_sub_rsp_i8        (uint8_t v);
void insn_add_eax_i32       (uint32_t v);
void insn_sub_eax_i32       (uint32_t v);
void insn_add_delta_i       (int v);  /* mov+add+mov Δ sequence      */
void insn_sub_delta_i       (int v);  /* mov+sub+mov Δ sequence      */

/* ---- test / xor -------------------------------------------------------- */
void insn_test_rax_rax      (void);
void insn_test_eax_eax      (void);
void insn_xor_eax_eax       (void);

/* ---- inc --------------------------------------------------------------- */
void insn_inc_r13_disp8     (uint8_t disp);

/* ---- push / pop (X-group) --------------------------------------------- */
void insn_push_rbp          (void);
void insn_pop_rbp           (void);
void insn_push_r10          (void);
void insn_pop_r10           (void);
void insn_push_r12          (void);
void insn_pop_r12           (void);

/* ---- call / ret / nop -------------------------------------------------- */
void insn_ret               (void);
void insn_nop               (void);
void insn_call_rax          (void);
void insn_call_plt          (const char *sym, uint64_t fn_fallback);

/* ---- jcc / jmp (X-group) ---------------------------------------------- */
void insn_jmp_r8            (bb_label_t *t);
void insn_jmp_r32           (bb_label_t *t);
void insn_je_r8             (bb_label_t *t);
void insn_je_r32            (bb_label_t *t);
void insn_jne_r8            (bb_label_t *t);
void insn_jne_r32           (bb_label_t *t);
void insn_jl_r8             (bb_label_t *t);
void insn_jl_r32            (bb_label_t *t);
void insn_jge_r8            (bb_label_t *t);
void insn_jge_r32           (bb_label_t *t);
void insn_jg_r32            (bb_label_t *t);

#endif /* INSN_H */
