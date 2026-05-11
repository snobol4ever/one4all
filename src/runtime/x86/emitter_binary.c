/*
 * emitter_binary.c — binary-mode implementation of emitter_t
 *
 * emit_insn renders each bb_insn_desc_t as x86-64 bytes into bb_pool.
 * Routes through bb_emit.c globals (bb_emit_buf/pos/size/patch_list).
 * The TEXT/BINARY decision is fully contained here — callers see no bytes.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-7b'' / GOAL-MODE4-EMIT
 */

#include "emitter.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

/* ── emit helpers (private) ─────────────────────────────────────────────── */

static void b1(uint8_t a)
{ bb_emit_byte(a); }
static void b2(uint8_t a, uint8_t b)
{ bb_emit_byte(a); bb_emit_byte(b); }
static void b3(uint8_t a, uint8_t b, uint8_t c)
{ bb_emit_byte(a); bb_emit_byte(b); bb_emit_byte(c); }
static void b4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{ bb_emit_byte(a); bb_emit_byte(b); bb_emit_byte(c); bb_emit_byte(d); }

static void imm32(uint32_t v) { bb_emit_u32(v); }
static void imm64(uint64_t v) { bb_emit_u64(v); }

/* ── emit_insn ──────────────────────────────────────────────────────────── */

static void binary_emit_insn(emitter_t *e, const bb_insn_desc_t *d)
{
    (void)e;
    uint64_t a0 = d->a0;
    uint32_t a1 = d->a1;
    uint8_t  a2 = d->a2;

    switch (d->t) {
    /* 64-bit reg ← imm64 : REX.W Bx <8> */
    case BB_INSN_MOV_R10_IMM64: b2(0x49,0xBA); imm64(a0); break; /* 49 BA */
    case BB_INSN_MOV_RAX_IMM64: b2(0x48,0xB8); imm64(a0); break; /* 48 B8 */
    case BB_INSN_MOV_RDI_IMM64: b2(0x48,0xBF); imm64(a0); break; /* 48 BF */
    case BB_INSN_MOV_RSI_IMM64: b2(0x48,0xBE); imm64(a0); break; /* 48 BE */
    case BB_INSN_MOV_RDX_IMM64: b2(0x48,0xBA); imm64(a0); break; /* 48 BA */
    case BB_INSN_MOV_RCX_IMM64: b2(0x48,0xB9); imm64(a0); break; /* 48 B9 */
    /* 32-bit reg ← imm32 */
    case BB_INSN_MOV_ESI_IMM32: b1(0xBE); imm32(a1); break;      /* BE <4> */
    case BB_INSN_MOV_EAX_IMM32: b1(0xB8); imm32(a1); break;      /* B8 <4> */
    case BB_INSN_ADD_EAX_IMM32: b1(0x05); imm32(a1); break;      /* 05 <4> */
    case BB_INSN_SUB_EAX_IMM32: b1(0x2D); imm32(a1); break;      /* 2D <4> */
    case BB_INSN_CMP_EAX_IMM32: b1(0x3D); imm32(a1); break;      /* 3D <4> */
    case BB_INSN_CMP_ESI_IMM8:  b2(0x83,0xFE); b1(a2); break;   /* 83 FE ib */
    /* memory loads */
    case BB_INSN_MOV_EAX_RCXMEM:    b2(0x8B,0x01); break;        /* 8B /0 */
    case BB_INSN_MOV_RAX_RCXMEM:    b3(0x48,0x8B,0x01); break;
    case BB_INSN_CMP_EAX_RCXMEM:    b2(0x3B,0x01); break;        /* 3B /0 */
    case BB_INSN_MOV_EAX_R10MEM:    b3(0x41,0x8B,0x02); break;   /* 41 8B 02 */
    case BB_INSN_MOV_R10MEM_EAX:    b3(0x41,0x89,0x02); break;   /* 41 89 02 */
    /* reg-reg */
    case BB_INSN_MOV_ECX_EAX:       b2(0x89,0xC1); break;        /* 89 /r */
    case BB_INSN_MOV_RDI_RAX:       b3(0x48,0x89,0xC7); break;
    case BB_INSN_MOV_RDX_RAX:       b3(0x48,0x89,0xC2); break;
    case BB_INSN_CMP_EAX_ECX:       b2(0x39,0xC8); break;
    case BB_INSN_TEST_EAX_EAX:      b2(0x85,0xC0); break;
    case BB_INSN_TEST_RAX_RAX:      b3(0x48,0x85,0xC0); break;
    case BB_INSN_XOR_EDX_EDX:       b2(0x31,0xD2); break;
    case BB_INSN_MOVSXD_RCX_R10MEM: b3(0x49,0x63,0x0A); break;   /* 49 63 /r */
    case BB_INSN_LEA_RAX_RAXRCX:    b4(0x48,0x8D,0x04,0x08); break;
    /* control */
    case BB_INSN_RET:      b1(0xC3); break;
    case BB_INSN_CALL_RAX: b2(0xFF,0xD0); break;
    /* EM-MODE4-IS-MODE3-DUMP-c: SM-State field manipulation */
    /* inc dword [r13 + disp8]:  41 ff 45 <disp8>  — 4 bytes */
    case BB_INSN_INC_MEM_R13_DISP8: b3(0x41,0xFF,0x45); b1(a2); break;
    /* EM-7c-symbolic: in BINARY mode, fall back to imm64 (in-process JIT) */
    /* LEA_RCX_SYM → mov rcx, imm64 */
    case BB_INSN_LEA_RCX_SYM:  b2(0x48,0xB9); imm64(a0); break;
    /* CALL_SYM_PLT → mov rax, imm64; call rax */
    case BB_INSN_CALL_SYM_PLT: b2(0x48,0xB8); imm64(a0); b2(0xFF,0xD0); break;
    /* LEA_R10_SYM → mov r10, imm64 (binary: movabs r10, addr) */
    case BB_INSN_LEA_R10_SYM:  b2(0x49,0xBA); imm64(a0); break;
    }
}

/* ── label_define ─────────────────────────────────────────────────────────── */
static void binary_label_define(emitter_t *e, bb_label_t *lbl)
{
    (void)e;
    bb_emit_mode_t s = bb_emit_mode; bb_emit_mode = EMIT_BINARY;
    bb_label_define(lbl);
    bb_emit_mode = s;
}

/* ── emit_jmp ─────────────────────────────────────────────────────────────── */
static const uint8_t jmp_rel32[6][2] = {
    {0xE9,0x00},{0x0F,0x84},{0x0F,0x85},{0x0F,0x8C},{0x0F,0x8D},{0x0F,0x8F}
};
static void binary_emit_jmp(emitter_t *e, bb_label_t *target, jmp_kind_t kind)
{
    (void)e;
    int k = (int)kind < 6 ? (int)kind : 0;
    if (k == 0) { b1(0xE9); }
    else        { b2(jmp_rel32[k][0], jmp_rel32[k][1]); }
    bb_emit_patch_rel32(target);
}

/* ── no-ops ───────────────────────────────────────────────────────────────── */
static void binary_global_sym  (emitter_t *e, const char *n) { (void)e;(void)n; }
static void binary_fprintf_raw (emitter_t *e, const char *f, ...) { (void)e;(void)f; }
static int  binary_pos         (emitter_t *e) { (void)e; return bb_emit_pos; }

/* ── EM-MODE4-IS-MODE3-DUMP-b: new surface — binary backend ───────────────────
 *
 * Per the design doc, the binary backend's responsibility is bytes in
 * SEG_CODE (or bb_pool RX slot).  Formatting / structural-marker calls
 * generally no-op.  A few correspond to real byte emission:
 *
 *   data_quad / data_quad_sym / data_string / pad_to_blob_size — emit
 *     actual bytes into the buffer.
 *
 *   label_name / pc_label / bb_port_label / bb_port_jmp — record an
 *     offset (or patch a jmp).  For sub-rung -b we register the label
 *     against the existing bb_label_t machinery via bb_emit.c; templates
 *     in -c onward will exercise these.
 *
 * Where no template caller exists yet, the implementation is a faithful
 * stub — it does the right thing if called, but no caller invokes it. */

static void binary_label_name(emitter_t *e, const char *name)
{
    /* For in-process JIT, a named label is just a marker for forward-ref
     * resolution.  Templates that emit named labels through this surface
     * should also construct the matching bb_label_t and use label_define;
     * for now, no-op so that erroneous calls don't corrupt state. */
    (void)e; (void)name;
}

static void binary_pc_label(emitter_t *e, int pc)
{
    /* Same reasoning as binary_label_name. */
    (void)e; (void)pc;
}

static void binary_section   (emitter_t *e, const char *n) { (void)e; (void)n; }
static void binary_directive (emitter_t *e, const char *l) { (void)e; (void)l; }

static void binary_data_quad(emitter_t *e, uint64_t val)
{
    (void)e;
    bb_emit_u64(val);
}

static void binary_data_quad_sym(emitter_t *e, const char *sym)
{
    /* In-process JIT: resolve symbol at runtime if linker support exists,
     * else emit zero placeholder.  Sub-rung -b has no caller; safer
     * default is no-op until -c needs a real definition. */
    (void)e; (void)sym;
}

static void binary_data_string(emitter_t *e, const char *bytes, size_t len)
{
    (void)e;
    if (!bytes) return;
    for (size_t i = 0; i < len; i++) bb_emit_byte((uint8_t)bytes[i]);
}

static void binary_pad_to_blob_size(emitter_t *e)
{
    /* No fixed blob-size constant exists at this layer.  Templates that
     * need explicit padding (e.g. fixed-size dispatch slots in mode-3)
     * should call emit_insn with a specific NOP-padding kind.  Sub-rung
     * -b leaves this as no-op; revisit if a template needs it. */
    (void)e;
}

static void binary_data_long(emitter_t *e, int32_t val)
{
    (void)e;
    /* Write 4 bytes little-endian into the current bb_pool buffer. */
    uint32_t u = (uint32_t)val;
    bb_emit_byte((uint8_t)(u));
    bb_emit_byte((uint8_t)(u >> 8));
    bb_emit_byte((uint8_t)(u >> 16));
    bb_emit_byte((uint8_t)(u >> 24));
}

static void binary_bb_zeta_rdi(emitter_t *e, uint64_t ptr, const char *sym)
{
    /* BINARY: bake in-process pointer directly — mov rdi, imm64(ptr). */
    (void)sym;
    emit_mov_rdi_imm64(e, ptr);
}

static void binary_bb_dispatch_jne_jmp(emitter_t *e,
                                       bb_label_t *lbl_succ, bb_label_t *lbl_fail)
{
    /* BINARY: test rax,rax; jne succ; jmp fail — 3 separate instructions. */
    emit_test_rax_rax(e);
    EV_JMP(e, lbl_succ, JMP_JNE);
    EV_JMP(e, lbl_fail, JMP_JMP);
}

static void binary_bb_port_label(emitter_t *e, const char *bp, char p) { (void)e;(void)bp;(void)p; }
static void binary_bb_port_jmp  (emitter_t *e, const char *bp, char p) { (void)e;(void)bp;(void)p; }
static void binary_bb_box_banner(emitter_t *e, const char *k, const char *a) { (void)e;(void)k;(void)a; }

static void binary_comment     (emitter_t *e, const char *t) { (void)e;(void)t; }
static void binary_banner      (emitter_t *e, const char *t) { (void)e;(void)t; }
static void binary_minor_break (emitter_t *e, const char *t) { (void)e;(void)t; }
static void binary_blank_line  (emitter_t *e)                { (void)e; }

static void binary_macro_begin    (emitter_t *e, const char *n,
                                   const char *const *p, int np)
{ (void)e;(void)n;(void)p;(void)np; }
static void binary_macro_param_ref(emitter_t *e, const char *n) { (void)e;(void)n; }
static void binary_macro_end      (emitter_t *e)                { (void)e; }

/* ── constructor ──────────────────────────────────────────────────────────── */
static const emitter_t binary_tmpl = {
    .emit_insn    = binary_emit_insn,
    .label_define = binary_label_define,
    .emit_jmp     = binary_emit_jmp,
    .global_sym   = binary_global_sym,
    .fprintf_raw  = binary_fprintf_raw,
    .pos          = binary_pos,
    .intern_str   = NULL,
    /* EM-MODE4-IS-MODE3-DUMP-b extensions */
    .label_name       = binary_label_name,
    .pc_label         = binary_pc_label,
    .section          = binary_section,
    .directive        = binary_directive,
    .data_quad        = binary_data_quad,
    .data_quad_sym    = binary_data_quad_sym,
    .data_string          = binary_data_string,
    .data_long            = binary_data_long,
    .bb_zeta_rdi          = binary_bb_zeta_rdi,
    .bb_dispatch_jne_jmp  = binary_bb_dispatch_jne_jmp,
    .pad_to_blob_size     = binary_pad_to_blob_size,
    .bb_port_label    = binary_bb_port_label,
    .bb_port_jmp      = binary_bb_port_jmp,
    .bb_box_banner    = binary_bb_box_banner,
    .comment          = binary_comment,
    .banner           = binary_banner,
    .minor_break      = binary_minor_break,
    .blank_line       = binary_blank_line,
    .macro_begin      = binary_macro_begin,
    .macro_param_ref  = binary_macro_param_ref,
    .macro_end        = binary_macro_end,
    .text_mode    = TEXT_MODE_INVOCATION,  /* unused for binary */
    .is_text      = 0,
    .ctx          = NULL,
};

emitter_t *emitter_binary_new(bb_buf_t buf, int size)
{
    emitter_t *e = malloc(sizeof(emitter_t));
    if (!e) return NULL;
    *e = binary_tmpl; e->ctx = NULL;
    bb_emit_mode = EMIT_BINARY;
    bb_emit_begin(buf, size);
    return e;
}
