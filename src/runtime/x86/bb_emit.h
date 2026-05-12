/*
 * bb_emit.h — Dual-Mode x86-64 Emitter (M-DYN-1)
 *
 * Every NASM macro in snobol4_asm.mac has a corresponding C function
 * mac_*() in bb_macros.c.  Those functions call the primitives here.
 *
 * MODE SWITCH
 * -----------
 * bb_emit_mode controls all output:
 *
 *   EMIT_TEXT   — write GAS assembly text to bb_emit_out (FILE*).
 *                 Labels are symbolic strings.  Output is a .s file
 *                 fed to GAS → ELF → linker.
 *
 *   EMIT_BINARY_WIRED — write raw x86-64 bytes into the current bb_pool
 *                 buffer (flat/live mode).  One contiguous blob for the
 *                 entire pattern tree.  Boxes jmp directly to each other's
 *                 α/β/γ/ω labels within the blob.  Broker calls the blob
 *                 ONCE at α entry (esi=0); backtracking is internal jmp.
 *                 r10 = &Δ loaded in preamble; rdi=ζ ignored (ζ=NULL).
 *
 *   EMIT_BINARY_BROKERED — write raw x86-64 bytes into bb_pool, one
 *                 blob per box (brokered mode).  Each blob has a full
 *                 C ABI entry: rdi=ζ heap struct, esi=port discriminator
 *                 (cmp esi,0; je α; jmp β), ret to return to broker.
 *                 Broker calls fn(ζ,0) for α and fn(ζ,1) for β separately.
 *
 *   EMIT_MACRO_DEF — emit .macro NAME ... .endm body (sm_macros.s regen).
 *
 * LABEL SYSTEM
 * ------------
 * bb_label_t carries both a symbolic name (text mode) and a buffer
 * offset (binary mode).  Labels start unresolved (offset == -1).
 * bb_label_define() resolves a label at the current emit position and
 * patches all pending forward references to it.
 *
 * PATCH LIST
 * ----------
 * When binary mode emits a jump to an unresolved label, it records a
 * (patch_site, label_id) entry.  bb_label_define() walks the list and
 * fills in the correct rel8 or rel32 displacement.
 *
 * USAGE SEQUENCE (binary mode)
 * ----------------------------
 *   bb_emit_begin(buf, size);        // attach emitter to pool buffer
 *   mac_LIT_α(...);                  // emit bytes + record patches
 *   mac_LIT_β(...);
 *   bb_emit_end();                   // resolve all patches, returns bytes written
 *   bb_seal(buf, bytes_written);     // mprotect RW→RX
 *   box_fn fn = (box_fn)buf;
 *   fn(subject, len);
 */

#ifndef BB_EMIT_H
#define BB_EMIT_H

#include "bb_pool.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* ── mode ───────────────────────────────────────────────────────────────── */

typedef enum {
    EMIT_TEXT             = 0,
    EMIT_BINARY_WIRED     = 1,
    EMIT_BINARY_BROKERED  = 2,   /* per-box blobs, C ABI entry, rdi=ζ, ret */
    EMIT_MACRO_DEF        = 3    /* emit .macro NAME ... .endm body (sm_macros.s regen) */
} bb_emit_mode_t;

extern bb_emit_mode_t bb_emit_mode;
extern FILE          *bb_emit_out;   /* text mode: output FILE* (default stdout) */

/* emit_mode_set — central setter for emit pass mode.
 *   m   = EMIT_BINARY_WIRED / EMIT_BINARY_BROKERED / EMIT_TEXT / EMIT_MACRO_DEF
 *   out = FILE* for text / macro_def modes; ignored (pass NULL) for binary.
 * Every low-level emit function consults bb_emit_mode to decide what to do.
 * Templates and the helpers they call do not carry the mode in their args. */
void emit_mode_set(bb_emit_mode_t m, FILE *out);

/* ── three-way low-level helpers (no emitter_t *e parameter) ────────────── */
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
void t_comment(const char *text);
void t_bb_box_banner(const char *kind, const char *args);

/* Instruction helpers — three-way, no emitter_t parameter. */
void t_inc_mem_r13_disp8(uint8_t disp);
void t_ret(void);
void t_pad_to_blob_size(void);
void t_mov_rdi_imm64(uint64_t val);
void t_call_sym_plt(const char *sym, uint64_t fn_fallback);
void t_macro_begin(const char *name, const char *const *params, int nparams);
void t_macro_end(void);

/* ── label ──────────────────────────────────────────────────────────────── */

#define BB_LABEL_NAME_MAX  80
#define BB_LABEL_UNRESOLVED (-1)

typedef struct {
    char name[BB_LABEL_NAME_MAX];   /* symbolic name — text mode + diagnostics */
    int  offset;                    /* byte offset in current buffer; -1=unresolved */
} bb_label_t;

/* Initialise a label with a symbolic name, unresolved offset */
void bb_label_init(bb_label_t *lbl, const char *name);

/* Initialise a label with printf-style name formatting */
void bb_label_initf(bb_label_t *lbl, const char *fmt, ...);

/* Define label at the current emit position (binary: sets offset + patches).
 * In text mode: emits "name:" on its own line. */
void bb_label_define(bb_label_t *lbl);

/* True if label has been defined (offset != BB_LABEL_UNRESOLVED) */
#define bb_label_defined(lbl)  ((lbl)->offset != BB_LABEL_UNRESOLVED)

/* ── jump kind (used by t_emit_jmp and emitter_t vtable) ────────────────── */
/* Defined here so both bb_emit.h and emitter.h consumers share one enum.   */

typedef enum {
    JMP_JMP = 0,
    JMP_JE,
    JMP_JNE,
    JMP_JL,
    JMP_JGE,
    JMP_JG,
} jmp_kind_t;

/* Three-way jmp helpers (no emitter_t *e parameter). */
void t_test_rax_rax(void);
void t_emit_jmp(bb_label_t *target, jmp_kind_t kind);

/* t_noop_macro — emit one three-column line with macro_name in col 2,
 * no operands.  Used by SM_LABEL and SM_STNO templates: the .LpcN: label
 * is consumed by the line; the macro body is empty so it assembles to nothing.
 *   BINARY:    no-op (label placement is the caller's job via bb_label_define)
 *   TEXT:      bb3c_format("", macro_name, "")
 *   MACRO_DEF: same as TEXT */
void t_lea_rdi_strtab_sym(const char *sym_label, uint64_t in_proc_ptr);
void t_lea_rdx_strtab_sym(const char *sym_label, uint64_t in_proc_ptr);
void t_mov_esi_imm32(int val);
void t_mov_edi_imm32(int val);
void t_mov_edx_imm32(int val);
void t_movabs_rdi_entry(uint64_t entry_ptr);
void t_call_sym_param(const char *sym_or_param);
void t_test_eax_eax(void);
void t_jz_retskip(int pc);
void t_retskip_label(int pc);
void t_noop_macro(const char *macro_name);

/* t_banner_stno — emit the SM_STNO major banner.
 *   BINARY:    no-op
 *   TEXT/MACRO_DEF: 120-char #= rule, "# stmt N  (line L):  <src>", #= rule */
void t_banner_stno(int stno, int lineno, const char *src_text);

/* ── BB port helpers (EM-TEMPLATE-PURITY-2) ────────────────────────────── */

/* t_label_define — define a label at the current emit position.
 *   BINARY:    resolves offset + patches forward refs.
 *   TEXT:      emits "name:\n". */
void t_label_define(bb_label_t *lbl);

/* t_bb_port_call — emit one α or β port body for a stateful box.
 *   Pattern: mov rdi, zeta_ptr; mov esi, port; call fn@PLT;
 *            test rax, rax; jne lbl_succ; jmp lbl_fail.
 *   TEXT: three-column lines.  BINARY: raw bytes.
 *   `port` is 0 for α, 1 for β. */
void t_bb_port_call(uint64_t zeta_ptr, const char *fn_name, uint64_t fn_fallback,
                    int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail);

/* t_load_delta_cmp_imm — load cursor (Δ), compare to n, jump.
 *   Pattern: eax = Δ; cmp eax, n; jne lbl_fail; jmp lbl_succ.
 *   BINARY: mov eax,[r10]; cmp eax,imm32; jne; jmp.
 *   TEXT: three-column lines. */
void t_load_delta_cmp_imm(int n, bb_label_t *lbl_succ, bb_label_t *lbl_fail);

/* t_load_siglen_sub_cmp_delta — load Σlen, subtract n, compare to Δ, jump.
 *   Pattern: eax = Σlen; eax -= n; ecx = eax; eax = Δ; cmp eax, ecx;
 *            jne lbl_fail; jmp lbl_succ.  Used by RPOS(n).
 *   BINARY: raw bytes.  TEXT: three-column lines. */
void t_load_siglen_sub_cmp_delta(int n, uint64_t siglen_addr,
                                 bb_label_t *lbl_succ, bb_label_t *lbl_fail);

/* t_lea_rsi_strtab_sym — load strtab string address into rsi.
 *   BINARY:    mov rsi, in_proc_ptr  (48 BE <8>)
 *   TEXT:      lea rcx, [rip + sym_label]; mov rsi, rcx
 *   MACRO_DEF: same as TEXT with \lbl parameter */
void t_lea_rsi_strtab_sym(const char *sym_label, uint64_t in_proc_ptr);

/* t_add_delta_imm — Δ += v  (load, add imm32, store back via [r10]).
 *   BINARY: mov eax,[r10]; add eax,imm32; mov [r10],eax
 *   TEXT:   three-column lines. */
void t_add_delta_imm(int v);

/* t_sub_delta_imm — Δ -= v  (load, sub imm32, store back via [r10]).
 *   BINARY: mov eax,[r10]; sub eax,imm32; mov [r10],eax
 *   TEXT:   three-column lines. */
void t_sub_delta_imm(int v);

/* t_sigma_plus_delta_to_rdi — rdi = Σ + Δ.
 *   BINARY: movabs rcx,&Σ; mov rax,[rcx]; movsxd rcx,[r10]; lea rax,[rax+rcx]; mov rdi,rax
 *   TEXT:   three-column lines. */
void t_sigma_plus_delta_to_rdi(uint64_t sigma_addr, uint64_t siglen_addr);

/* t_bounds_check_delta_plus_len — eax = Δ + len; cmp eax, Σlen; jg lbl_fail.
 *   BINARY: mov eax,[r10]; add eax,imm32; movabs rcx,&Σlen; cmp eax,[rcx]; jg fail
 *   TEXT:   three-column lines. */
void t_bounds_check_delta_plus_len(int len, uint64_t siglen_addr, bb_label_t *lbl_fail);

/* t_push_rbp_frame — establish a C-style stack frame at function entry.
 * Emits: push rbp; mov rbp, rsp; sub rsp, 8
 * The sub rsp,8 maintains the (rsp+8) mod 16 == 0 ABI invariant after
 * the push rbp.  Required at the entry of every mode-4 user-function body
 * (SM_DEFINE_ENTRY) because cfn() is called from call_native_chunk via a
 * C-ABI call, and the body subsequently makes C-ABI calls (rt_match_variant
 * etc.) whose callees use SSE-aligned access (movaps -0x60(%rbp)).
 *   BINARY: bytes 55 48 89 E5 48 83 EC 08
 *   TEXT:   three-column lines (push, mov, sub) */
void t_push_rbp_frame(void);

/* t_pop_rbp_frame_ret — undo t_push_rbp_frame and return.
 * Emits: mov rsp, rbp; pop rbp; ret  (the mov rsp,rbp strips the sub rsp,8).
 *   BINARY: bytes 48 89 EC 5D C3
 *   TEXT:   three-column lines (mov, pop, ret) */
void t_pop_rbp_frame_ret(void);

/* ── binary mode state ──────────────────────────────────────────────────── */

extern bb_buf_t  bb_emit_buf;   /* current pool buffer */
extern int       bb_emit_pos;   /* current write position (bytes written so far) */
extern int       bb_emit_size;  /* total buffer size */

/* Attach emitter to a freshly-allocated pool buffer */
void bb_emit_begin(bb_buf_t buf, int size);

/* Finalise: resolve any remaining patches, return bytes written.
 * Aborts if any labels are still unresolved. */
int  bb_emit_end(void);

/* ── patch list ─────────────────────────────────────────────────────────── */

/* Maximum simultaneous forward references (generous: deep ARBNO patterns) */
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

/* Record a forward reference at the current position.
 * Emits a 0-placeholder of the appropriate width. */
void bb_emit_patch_rel8 (bb_label_t *lbl);
void bb_emit_patch_rel32(bb_label_t *lbl);

/* ── byte primitives ────────────────────────────────────────────────────── */

/* These are the only functions that write bytes.  All higher-level
 * helpers (instruction emitters, mac_* functions) call these. */

void bb_emit_byte(uint8_t b);
void bb_emit_u16 (uint16_t v);
void bb_emit_u32 (uint32_t v);
void bb_emit_u64 (uint64_t v);
void bb_emit_i8  (int8_t   v);
void bb_emit_i32 (int32_t  v);

/* ── x86-64 instruction helpers ─────────────────────────────────────────── */
/*
 * One function per instruction form used by the mac_* layer.
 * Names follow: bb_insn_MNEMONIC_OPERAND_FORM
 *
 * Registers are implicit (SysV AMD64 calling convention throughout):
 *   subject ptr  = rdi
 *   subject len  = esi / rsi
 *   cursor       = a .bss slot or stack slot addressed by label
 *   scratch      = rax, rcx, rdx
 *
 * In text mode these emit the NASM instruction string.
 * In binary mode these emit raw bytes.
 */

/* mov eax, imm32 */
void bb_insn_mov_eax_imm32(uint32_t imm);

/* mov rax, imm64  (for absolute C shim addresses) */
void bb_insn_mov_rax_imm64(uint64_t imm);

/* ret */
void bb_insn_ret(void);

/* nop */
void bb_insn_nop(void);

/* call rax */
void bb_insn_call_rax(void);

/* jmp rel8  — short unconditional jump, forward ref supported */
void bb_insn_jmp_rel8(bb_label_t *target);

/* jmp rel32 — near unconditional jump, forward ref supported */
void bb_insn_jmp_rel32(bb_label_t *target);

/* jl  rel8  — jump if less (SF≠OF), forward ref */
void bb_insn_jl_rel8 (bb_label_t *target);

/* jge rel8  — jump if greater-or-equal, forward ref */
void bb_insn_jge_rel8(bb_label_t *target);

/* je  rel8  — jump if equal (ZF=1), forward ref */
void bb_insn_je_rel8 (bb_label_t *target);

/* jne rel8  — jump if not equal, forward ref */
void bb_insn_jne_rel8(bb_label_t *target);

/* jne rel32 — jump if not equal, near, forward ref */
void bb_insn_jne_rel32(bb_label_t *target);

/* jg  rel32 — jump if greater (signed), near, forward ref (EM-7b) */
void bb_insn_jg_rel32 (bb_label_t *target);

/* cmp esi, imm8  — compare subject length against literal */
void bb_insn_cmp_esi_imm8(uint8_t imm);

/* cmp esi, imm32 */
void bb_insn_cmp_esi_imm32(uint32_t imm);

/* movzx eax, byte [rdi + imm8]  — load subject byte at offset */
void bb_insn_movzx_eax_rdi_off8(uint8_t off);

/* cmp al, imm8 */
void bb_insn_cmp_al_imm8(uint8_t imm);

/* xor eax, eax  — zero eax */
void bb_insn_xor_eax_eax(void);

/* push rbp / pop rbp / mov rbp,rsp */
void bb_insn_push_rbp(void);
void bb_insn_pop_rbp(void);
void bb_insn_mov_rbp_rsp(void);

/* sub rsp, imm8 / add rsp, imm8 */
void bb_insn_sub_rsp_imm8(uint8_t imm);
void bb_insn_add_rsp_imm8(uint8_t imm);

/* ── text mode helpers ───────────────────────────────────────────────────── */

/* Emit a raw text line (text mode only — no-op in binary mode) */
void bb_text(const char *fmt, ...);

/* Emit a label definition line "name:\n" (text) or define offset (binary) */
void bb_text_label(bb_label_t *lbl);

/* Emit a comment line (text mode only) */
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

/* EM-FORMAT-BB lone-label fusion (2026-05-09):
 * Flush any pending label held by `bb3c_format`'s fusion buffer.  Call
 * once at end-of-file before closing the output stream so a trailing
 * label-only emission doesn't get silently dropped.  No-op if buffer
 * is empty. */
void bb3c_flush_pending(void);

/* EM-FORMAT-BB-FUSED-GOTOS (2026-05-09):
 * Flush only the deferred conditional-jmp buffer (NOT the pending label).
 * Used by raw-write paths (banners, EV_TEXT) that physically write to
 * the FILE* without going through bb3c_format — they must land AFTER
 * any cond-jmp emission, but the pending label is a separate concern
 * (handled by bb3c_format's own logic when the next bb3c content
 * arrives).  Symmetric with bb3c_flush_pending which flushes the label. */
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

#endif /* BB_EMIT_H */
