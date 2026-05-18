/* emit_form.h — L1 encoding forms: raw-byte helpers and inline static insn wrappers */
#ifndef EMIT_FORM_H
#define EMIT_FORM_H
#include "emit_core.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
extern int g_is_text;
extern int g_emit_text_mode;
extern int g_emit_pos;
typedef int emitter_t;
/*--- encoding primitives (called by insn_* leaf functions) ---------------*/
void emit_form_reg64_imm64  (uint8_t prefix, uint8_t reg, uint64_t val, const char * mnem);
void emit_form_reg32_imm32  (uint8_t op, uint32_t val, const char * mnem);
void emit_form_alu_eax_imm32(uint8_t op, uint32_t val, const char * mnem);
void emit_form_alu_esi_imm8 (uint8_t modrm, uint8_t val, const char * mnem);
void emit_form_reg_reg2     (uint8_t b0, uint8_t b1, const char * text);
void emit_form_reg_reg3     (uint8_t b0, uint8_t b1, uint8_t b2, const char * text);
void emit_form_mem2         (uint8_t b0, uint8_t b1, const char * text);
void emit_form_mem3         (uint8_t b0, uint8_t b1, uint8_t b2, const char * text);
void emit_form_mem4         (uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, const char * text);
void emit_form_r13_disp8    (uint8_t b0, uint8_t b1, uint8_t b2, uint8_t disp, const char * text_fmt);
void emit_form_nullary1     (uint8_t b0, const char * text);
void emit_form_nullary2     (uint8_t b0, uint8_t b1, const char * text);
void emit_form_nullary3     (uint8_t b0, uint8_t b1, uint8_t b2, const char * text);
void emit_sym_lea_rcx       (const char * sym, uint64_t addr_fallback);
void emit_sym_lea_r10       (const char * sym, uint64_t addr_fallback);
void emit_call_sym_plt      (const char * sym, uint64_t fn_fallback);
void emitter_init_binary    (bb_buf_t buf, int size);
void emitter_init_text      (FILE * out, int mode);
void emitter_init_macro_def (FILE * out);
FILE * emitter_text_out     (void);
int    emitter_pos          (void);
int    emitter_end          (void);
/*--- inline static insn wrappers (TEXT or binary via emit_form_*) --------*/
static inline void emit_mov_r10_imm64    (uint64_t v)   { emit_form_reg64_imm64(0x49,0xBA,v,"r10");                           }
static inline void emit_mov_rax_imm64    (uint64_t v)   { emit_form_reg64_imm64(0x48,0xB8,v,"rax");                           }
static inline void emit_mov_rcx_imm64    (uint64_t v)   { emit_form_reg64_imm64(0x48,0xB9,v,"rcx");                           }
static inline void emit_mov_eax_imm32    (uint32_t v)   { emit_form_reg32_imm32(0xB8,v,"eax");                                }
static inline void emit_sub_eax_imm32    (uint32_t v)   { emit_form_alu_eax_imm32(0x2D,v,"sub");                              }
static inline void emit_cmp_eax_imm32    (uint32_t v)   { emit_form_alu_eax_imm32(0x3D,v,"cmp");                              }
static inline void emit_cmp_esi_imm8     (uint8_t  v)   { emit_form_alu_esi_imm8(0xFE,v,"cmp");                               }
static inline void emit_mov_ecx_eax      (void)         { emit_form_reg_reg2(0x89,0xC1,      "mov ecx, eax"                ); }
static inline void emit_mov_rdx_rax      (void)         { emit_form_reg_reg3(0x48,0x89,0xC2, "mov rdx, rax"                ); }
static inline void emit_cmp_eax_ecx      (void)         { emit_form_reg_reg2(0x39,0xC8,      "cmp eax, ecx"                ); }
static inline void emit_xor_edx_edx      (void)         { emit_form_reg_reg2(0x31,0xD2,      "xor edx, edx"                ); }
static inline void emit_mov_eax_rcxmem   (void)         { emit_form_mem2(0x8B,0x01,          "mov eax, [rcx]"              ); }
static inline void emit_mov_rax_rcxmem   (void)         { emit_form_mem3(0x48,0x8B,0x01,     "mov rax, [rcx]"              ); }
static inline void emit_cmp_eax_rcxmem   (void)         { emit_form_mem2(0x3B,0x01,          "cmp eax, [rcx]"              ); }
static inline void emit_mov_eax_r10mem   (void)         { emit_form_mem3(0x41,0x8B,0x02,     "mov eax, [r10]"              ); }
static inline void emit_mov_r10mem_eax   (void)         { emit_form_mem3(0x41,0x89,0x02,     "mov [r10], eax"              ); }
static inline void emit_movsxd_rcx_r10mem(void)         { emit_form_mem3(0x49,0x63,0x0A,     "movsxd rcx, dword ptr [r10]" ); }
static inline void emit_lea_rax_raxrcx   (void)         { emit_form_mem4(0x48,0x8D,0x04,0x08,"lea rax, [rax+rcx]"          ); }
static inline void emit_call_rax         (void)         { emit_form_nullary2(0xFF,0xD0,      "call rax"                    ); }
static inline void emit_pop_rbp          (void)         { emit_form_nullary1(0x5D,           "pop rbp"                     ); }
static inline void emit_inc_mem_r13_disp8(uint8_t disp) { emit_form_r13_disp8(0x41,0xFF,0x45,disp,"inc dword ptr [r13 + %u]"); }
/*--- delta / sigma helpers (declared in emit_core.h, defined in emit_core.c) */
void emit_load_r10_delta_ptr (uint64_t addr);
void emit_load_delta         (void);                    void emit_store_delta        (void);
void emit_add_delta_imm      (int32_t v);               void emit_sub_delta_imm      (int32_t v);
void emit_load_sigma         (uint64_t sigma_addr);     void emit_load_siglen        (uint64_t siglen_addr);
void emit_sigma_plus_delta   (uint64_t sigma_addr);     void emit_cmp_eax_siglen     (uint64_t siglen_addr);
void emit_label_define_bb    (bb_label_t * lbl);
void emit_bb_port_label      (const char * pfx, char port);
void emit_bb_port_jmp        (const char * pfx, char port);
#endif
