/*
 * emitter.h — x86-64 emitter: form-based API, no vtable.
 *
 * Global state (g_is_text, bb_emit_out, g_emit_pos, g_emit_text_mode) replaces
 * emitter_t.  All functions read that state directly — no pointer threading.
 *
 * Two lifecycle helpers replace the old constructors:
 *   emitter_init_binary(buf, size)   — set globals, begin binary blob
 *   emitter_init_text(out, mode)     — set globals, direct to FILE*
 *   emitter_end()                    — finalise; returns byte count (binary) or pos (text)
 *
 * Instruction-form functions encode one x86-64 encoding class each.
 * The caller supplies the opcode bytes; the function supplies the framing.
 * This eliminates one hardcoded function per instruction variant.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-DEVTABLE / GOAL-MODE4-EMIT
 */

#ifndef EMITTER_H
#define EMITTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include "emitter_bb_gen.h"   /* bb_emit_mode_t, bb_label_t, jmp_kind_t, bb_buf_t */

/*============================================================================
 * Global emitter state (set by emitter_init_*; read by all emit functions)
 *==========================================================================*/

extern int  g_is_text;        /* 0=binary, 1=text/macro_def  */
extern int  g_emit_text_mode; /* TEXT_MODE_INVOCATION / _DEFINITION */
extern int  g_emit_pos;       /* text-mode byte-position tracker    */

/* Backward-compat stub: templates declare emitter_t *e but never dereference it. */
typedef int emitter_t;

#define TEXT_MODE_INVOCATION  0   /* emit macro invocation lines (default) */
#define TEXT_MODE_DEFINITION  1   /* emit .macro/.endm bodies for sm_macros.s */

/*============================================================================
 * Lifecycle
 *==========================================================================*/

void emitter_init_binary(bb_buf_t buf, int size);
void emitter_init_text  (FILE *out, int mode);   /* mode: TEXT_MODE_* */
int  emitter_end        (void);                  /* binary: bb_emit_end(); text: g_emit_pos */

/*============================================================================
 * Instruction-form functions
 *
 * Each function encodes one x86-64 encoding class.
 * Opcode bytes are caller-supplied; textual mnemonics are caller-supplied.
 * TEXT path writes three-column GAS; BINARY path writes raw bytes.
 *==========================================================================*/

/* reg64 <- imm64  (REX + reg-byte + 8-byte imm)
 *   prefix: REX byte (0x48 rax/rdi/rsi/rdx/rcx, 0x49 r10/r11)
 *   reg:    low opcode byte (0xB8 rax, 0xBF rdi, 0xBE rsi, 0xBA rdx/r10, 0xB9 rcx)
 *   mnem:   GAS register name ("rax", "rdi", "r10", ...)                  */
void emit_form_reg64_imm64(uint8_t prefix, uint8_t reg, uint64_t val, const char *mnem);

/* reg32 <- imm32  (opcode-byte + 4-byte imm)
 *   op:   short-form opcode (0xBE esi, 0xB8 eax)
 *   mnem: "esi", "eax", ...                                               */
void emit_form_reg32_imm32(uint8_t op, uint32_t val, const char *mnem);

/* ALU eax, imm32  (1-byte opcode + 4-byte imm; short eax encoding)
 *   op:   0x05 add, 0x2D sub, 0x3D cmp
 *   mnem: "add", "sub", "cmp"                                             */
void emit_form_alu_eax_imm32(uint8_t op, uint32_t val, const char *mnem);

/* ALU esi, imm8  (0x83 ModRM + 1-byte imm; ModRM encodes sub-opcode)
 *   modrm: 0xFE = cmp esi
 *   mnem:  "cmp"                                                           */
void emit_form_alu_esi_imm8(uint8_t modrm, uint8_t val, const char *mnem);

/* reg-reg fixed encoding — 2, 3 bytes (no operand slots)
 *   text: full "mnem dst, src" string written verbatim in text col 2+3   */
void emit_form_reg_reg2(uint8_t b0, uint8_t b1,             const char *text);
void emit_form_reg_reg3(uint8_t b0, uint8_t b1, uint8_t b2, const char *text);

/* memory fixed encoding — 2, 3, 4 bytes  (e.g. [rcx], [r10]) */
void emit_form_mem2(uint8_t b0, uint8_t b1,                         const char *text);
void emit_form_mem3(uint8_t b0, uint8_t b1, uint8_t b2,             const char *text);
void emit_form_mem4(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, const char *text);

/* [r13 + disp8] — three prefix bytes + displacement byte
 *   b0,b1,b2: e.g. 0x41,0xFF,0x45 for "inc dword [r13+disp8]"
 *   text_fmt: GAS format with one %u for disp                             */
void emit_form_r13_disp8(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t disp,
                          const char *text_fmt);

/* nullary — fixed byte sequence, fixed text (no operand slots) */
void emit_form_nullary1(uint8_t b0,                         const char *text);
void emit_form_nullary2(uint8_t b0, uint8_t b1,             const char *text);
void emit_form_nullary3(uint8_t b0, uint8_t b1, uint8_t b2, const char *text);

/* symbolic — TEXT: RIP-relative; BINARY: imm64 (in-process JIT fallback) */
void emit_sym_lea_rcx (const char *sym, uint64_t addr_fallback);
void emit_sym_lea_r10 (const char *sym, uint64_t addr_fallback);
void emit_call_sym_plt(const char *sym, uint64_t fn_fallback);

/*============================================================================
 * Named convenience wrappers (thin inlines calling form functions)
 *==========================================================================*/

static inline void emit_mov_r10_imm64(uint64_t v) { emit_form_reg64_imm64(0x49,0xBA,v,"r10"); }
static inline void emit_mov_rax_imm64(uint64_t v) { emit_form_reg64_imm64(0x48,0xB8,v,"rax"); }
static inline void emit_mov_rsi_imm64(uint64_t v) { emit_form_reg64_imm64(0x48,0xBE,v,"rsi"); }
static inline void emit_mov_rdx_imm64(uint64_t v) { emit_form_reg64_imm64(0x48,0xBA,v,"rdx"); }
static inline void emit_mov_rcx_imm64(uint64_t v) { emit_form_reg64_imm64(0x48,0xB9,v,"rcx"); }

static inline void emit_mov_eax_imm32(uint32_t v) { emit_form_reg32_imm32(0xB8,v,"eax"); }

static inline void emit_add_eax_imm32(uint32_t v) { emit_form_alu_eax_imm32(0x05,v,"add"); }
static inline void emit_sub_eax_imm32(uint32_t v) { emit_form_alu_eax_imm32(0x2D,v,"sub"); }
static inline void emit_cmp_eax_imm32(uint32_t v) { emit_form_alu_eax_imm32(0x3D,v,"cmp"); }
static inline void emit_cmp_esi_imm8 (uint8_t  v) { emit_form_alu_esi_imm8(0xFE,v,"cmp"); }

static inline void emit_mov_ecx_eax    (void) { emit_form_reg_reg2(0x89,0xC1,     "mov ecx, eax"                ); }
static inline void emit_mov_rdi_rax    (void) { emit_form_reg_reg3(0x48,0x89,0xC7,"mov rdi, rax"                ); }
static inline void emit_mov_rdx_rax    (void) { emit_form_reg_reg3(0x48,0x89,0xC2,"mov rdx, rax"                ); }
static inline void emit_cmp_eax_ecx    (void) { emit_form_reg_reg2(0x39,0xC8,     "cmp eax, ecx"                ); }
static inline void emit_xor_edx_edx    (void) { emit_form_reg_reg2(0x31,0xD2,     "xor edx, edx"                ); }

static inline void emit_mov_eax_rcxmem   (void) { emit_form_mem2(0x8B,0x01,         "mov eax, [rcx]"            ); }
static inline void emit_mov_rax_rcxmem   (void) { emit_form_mem3(0x48,0x8B,0x01,    "mov rax, [rcx]"            ); }
static inline void emit_cmp_eax_rcxmem   (void) { emit_form_mem2(0x3B,0x01,         "cmp eax, [rcx]"            ); }
static inline void emit_mov_eax_r10mem   (void) { emit_form_mem3(0x41,0x8B,0x02,    "mov eax, [r10]"            ); }
static inline void emit_mov_r10mem_eax   (void) { emit_form_mem3(0x41,0x89,0x02,    "mov [r10], eax"            ); }
static inline void emit_movsxd_rcx_r10mem(void) { emit_form_mem3(0x49,0x63,0x0A,    "movsxd rcx, dword ptr [r10]"); }
static inline void emit_lea_rax_raxrcx   (void) { emit_form_mem4(0x48,0x8D,0x04,0x08,"lea rax, [rax+rcx]"      ); }

static inline void emit_call_rax (void) { emit_form_nullary2(0xFF,0xD0, "call rax"); }
static inline void emit_pop_rbp  (void) { emit_form_nullary1(0x5D,      "pop rbp" ); }

static inline void emit_inc_mem_r13_disp8(uint8_t disp) {
    emit_form_r13_disp8(0x41,0xFF,0x45, disp, "inc dword ptr [r13 + %u]");
}

/*============================================================================
 * Composite helpers (defined in emitter.c)
 *==========================================================================*/

void emit_load_r10_delta_ptr (uint64_t addr);   /* r10 = &delta             */
void emit_load_delta         (void);            /* eax = [r10]              */
void emit_store_delta        (void);            /* [r10] = eax              */
void emit_add_delta_imm      (int32_t v);
void emit_sub_delta_imm      (int32_t v);
void emit_load_sigma         (uint64_t sigma_addr);
void emit_load_siglen        (uint64_t siglen_addr);
void emit_sigma_plus_delta   (uint64_t sigma_addr);
void emit_cmp_eax_siglen     (uint64_t siglen_addr);

/* Label + jump */
void emit_label_define_bb    (bb_label_t *lbl);
void emit_label_name         (const char *name);
void emit_pc_label           (int pc);
void emit_jmp_label          (bb_label_t *target, jmp_kind_t kind);

/* Text-only structural output (no-ops in binary) */
void emit_section            (const char *name);
void emit_directive          (const char *line);
void emit_global_sym         (const char *name);
void emit_banner             (const char *text);
void emit_minor_break        (const char *text);
void emit_blank_line         (void);
void emit_fprintf_raw        (const char *fmt, ...);

/* Data emission */
void emit_data_quad          (uint64_t val);
void emit_data_quad_sym      (const char *sym);
void emit_data_string        (const char *bytes, size_t len);
void emit_data_long          (int32_t val);

/* BB compound emissions */
void emit_bb_zeta_rdi        (uint64_t ptr, const char *sym);
void emit_bb_dispatch_jne_jmp(bb_label_t *lbl_succ, bb_label_t *lbl_fail);
void emit_bb_port_label      (const char *pfx, char port);
void emit_bb_port_jmp        (const char *pfx, char port);

/* Macro hooks */
void emit_macro_begin        (const char *name, const char *const *params, int nparams);
void emit_macro_param_ref    (const char *name);
void emit_macro_end          (void);
void emit_pad_to_blob_size   (void);

/* Utility */
FILE *emitter_text_out       (void);   /* bb_emit_out (for legacy callers needing FILE*) */
int   emitter_pos            (void);   /* g_is_text ? g_emit_pos : bb_emit_pos           */

void emitter_init_macro_def(FILE *out);  /* TEXT_MODE_DEFINITION — for sm_macros.s regen */

#endif /* EMITTER_H */

