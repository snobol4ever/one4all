
#ifndef EMITTER_BB_GEN_H
#define EMITTER_BB_GEN_H

#include "bb_pool.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    EMIT_TEXT             = 0,
    EMIT_BINARY_WIRED     = 1,
    EMIT_BINARY_BROKERED  = 2,   /* per-box blobs, C ABI entry, rdi=ζ, ret */
    EMIT_MACRO_DEF        = 3,   /* emit .macro NAME ... .endm body (sm_macros.s regen) */
    EMIT_TEXT_INLINE      = 4    /* inline GAS; emit_macro_begin/end are no-ops */
} bb_emit_mode_t;

extern bb_emit_mode_t bb_emit_mode;

extern int g_bb_emit_format;
extern FILE          *bb_emit_out;   /* text mode: output FILE* (default stdout) */

/* emit_mode_set — central setter for emit pass mode.
 *   m   = EMIT_BINARY_WIRED / EMIT_BINARY_BROKERED / EMIT_TEXT / EMIT_MACRO_DEF
 *   out = FILE* for text / macro_def modes; ignored (pass NULL) for binary.
 * Every low-level emit function consults bb_emit_mode to decide what to do.
 * Templates and the helpers they call do not carry the mode in their args. */
void emit_mode_set(bb_emit_mode_t m, FILE *out);

/*
 * Templates and template-helpers call these directly.  Each helper consults
 * bb_emit_mode and produces one of {binary bytes, text line, macro_def line}
 * — or nothing, when "do nothing" is the right answer for that mode.
 *
 * Helpers are added incrementally as templates are ported off emitter_t.
 * Co-exist with the emitter_t vtable during the transition: existing call
 * sites that have an `emitter_t *e` keep using e->method or the inline
 * helpers in emitter.h; new template code calls these free-standing helpers
 * directly.  The `t_` prefix marks them as the template-side surface and
 * avoids collision with the existing inline `emit_*(emitter_t *e, ...)`
 * names.  Once the vtable is gone, the inlines disappear and the `t_`
 * prefix may be dropped in a rename pass.
 */
void emit_comment(const char *text);
void emit_bb_box_banner(const char *kind, const char *args);

void emit_bb_inc_mem_r13_disp8(uint8_t disp);
void emit_pad_to_blob_size(void);
void emit_macro_begin(const char *name, const char *const *params, int nparams);
void emit_macro_end(void);

#define BB_LABEL_NAME_MAX  80
#define BB_LABEL_UNRESOLVED (-1)

typedef struct {
    char name[BB_LABEL_NAME_MAX];   /* symbolic name — text mode + diagnostics */
    int  offset;                    /* byte offset in current buffer; -1=unresolved */
} bb_label_t;

void bb_label_init(bb_label_t *lbl, const char *name);

void bb_label_initf(bb_label_t *lbl, const char *fmt, ...);

void bb_label_define(bb_label_t *lbl);

#define bb_label_defined(lbl)  ((lbl)->offset != BB_LABEL_UNRESOLVED)

typedef enum {
    JMP_JMP = 0,
    JMP_JE,
    JMP_JNE,
    JMP_JL,
    JMP_JGE,
    JMP_JG,
} jmp_kind_t;

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

/* emit_bb_port_call_rip — like emit_bb_port_call but TEXT/INLINE mode uses
 *   `lea rdi, [rip + zeta_label]` instead of `mov rdi, literal`.
 *   Use for boxes whose ζ is emitted as static .data (xatp, xdsar).
 *   BINARY: ignores zeta_label; uses zeta_ptr directly (same as emit_bb_port_call).
 *   TEXT/INLINE: emits `lea rdi, [rip + zeta_label]`. */
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

extern bb_buf_t  bb_emit_buf;   /* current pool buffer */
extern int       bb_emit_pos;   /* current write position (bytes written so far) */
extern int       bb_emit_size;  /* total buffer size */

void bb_emit_begin(bb_buf_t buf, int size);

int  bb_emit_end(void);

#define BB_PATCH_MAX  512

typedef enum {
    PATCH_REL8,    /* 1-byte signed displacement, relative to patch_site+1 */
    PATCH_REL32    /* 4-byte signed displacement, relative to patch_site+4 */
} bb_patch_kind_t;

typedef struct {
    int              site;    /* offset of the displacement field in the buffer */
    bb_label_t      *label;   /* label whose offset we need */
    bb_patch_kind_t  kind;
} bb_patch_t;

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

/* ── BB three-column line emission (EM-7c-bb-three-column) ──────────────────
 *
 *   LABEL: ; ACTION ; GOTO
 *
 * Widths: label=24, action=16, goto=free.  Separators: " ; " between
 * columns.  GAS interprets `;` as a statement separator on x86, so
 * empty fields parse as empty statements (legal).  Used for BB-box
 * body lines emitted via emitter_text.c, bb_emit.c text helpers, and
 * bb_flat.c EV_TEXT blocks.
 *
 * NULL or "" arguments render as the right amount of whitespace.
 * `bb3c_text` (text mode only) writes one line; no-op in binary mode.
 *
 * `bb3c_format` is a re-usable formatter for callers that already
 * have a FILE*; it does NOT route through bb_emit_mode.
 */
void bb3c_text(const char *label, const char *action, const char *goto_);
void bb3c_format(FILE *out, const char *label, const char *action, const char *goto_);

void bb3c_flush_pending(void);

void bb3c_flush_pending_cjmp_only(void);

/* EM-FORMAT-BB-FUSED-GOTOS (2026-05-09):
 * Single entry point for emitting jmp-shape lines (col 1 empty,
 * col 2 = jmp/je/jne/jl/jge/jg, col 3 = target).  Handles cond-jmp+
 * uncond-jmp adjacency by deferring conditional jumps; if the next
 * call is an unconditional `jmp`, the two fuse onto a single line:
 *   <cond_mn>             <succ_target> ; jmp <fail_target>
 * Otherwise the deferred cond-jmp flushes standalone.  Both
 * `text_emit_jmp` (emitter_text.c) and `bb_insn_jmp/je/jne/jl/jge/
 * jg_*` (bb_emit.c, TEXT mode) route through this. */
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

#endif /* EMITTER_BB_GEN_H */
