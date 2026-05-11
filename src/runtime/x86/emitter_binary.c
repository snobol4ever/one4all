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

    switch (d->kind) {
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

/* ── constructor ──────────────────────────────────────────────────────────── */
static const emitter_t binary_tmpl = {
    .emit_insn    = binary_emit_insn,
    .label_define = binary_label_define,
    .emit_jmp     = binary_emit_jmp,
    .global_sym   = binary_global_sym,
    .fprintf_raw  = binary_fprintf_raw,
    .pos          = binary_pos,
    .intern_str   = NULL,
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
