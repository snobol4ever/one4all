/*
 * emitter_text.c — GAS/text-mode implementation of emitter_v
 *
 * EM-7c-bb-three-column (2026-05-09):
 *   Every BB-box body line emits in three-column shape
 *
 *     LABEL:                   ; ACTION           ; GOTO
 *     col 1 (24 wide)          ; col 2 (16 wide)  ; col 3 (free)
 *
 *   Separators are literal ` ; ` (GAS: ; is statement separator on x86).
 *   text_label_define   → label-only line (cols 2+3 empty)
 *   text_emit_insn      → action-only line (cols 1+3 empty)
 *   text_emit_jmp       → goto-only line  (cols 1+2 empty)
 *
 *   No .byte walls — every instruction is a real mnemonic.  Greek-only
 *   port names; `bb`/`BB` prefix banned (no Latin alpha/beta...).
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet
 * Sprint:  EM-7b'' / EM-7c-bb-three-column / GOAL-MODE4-EMIT
 */

#include "emitter_v.h"
#include "bb_emit.h"     /* bb3c_format */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct { FILE *out; int pos; } text_ctx_t;
#define CTX(e) ((text_ctx_t *)((e)->ctx))
static FILE *outf(emitter_v *e) { FILE *f = CTX(e)->out; return f ? f : stdout; }

/* ── three-column shorthands ──────────────────────────────────────────────── */
/* EM-7c-bb-three-column-split (2026-05-09): take mnemonic and args separately,
 * so col 2 holds ONLY the mnemonic and col 3 holds the operands.  The prior
 * emit3c_action stuffed both into col 2 and overflowed the 16-wide field. */

static void emit3c_op(emitter_v *e, const char *mn, const char *fmt, ...)
{
    char buf[256];
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
    } else {
        buf[0] = '\0';
    }
    bb3c_format(outf(e), "", mn ? mn : "", buf);
}

/* Goto-only: col 1 empty, col 2 = jmp/je/jne/..., col 3 = target. */
static void emit3c_jmp(emitter_v *e, const char *mn, const char *target)
{
    bb3c_format(outf(e), "", mn ? mn : "", target ? target : "");
}

/* ── emit_insn: one three-column line per instruction (action column) ─────── */

static void text_emit_insn(emitter_v *e, const bb_insn_desc_t *d)
{
    uint64_t a0 = d->a0;
    uint32_t a1 = d->a1;
    uint8_t  a2 = d->a2;

    switch (d->kind) {
    /* 64-bit reg ← imm64 */
    case BB_INSN_MOV_R10_IMM64: emit3c_op(e,"mov","r10, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; break;
    case BB_INSN_MOV_RAX_IMM64: emit3c_op(e,"mov","rax, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; break;
    case BB_INSN_MOV_RDI_IMM64: emit3c_op(e,"mov","rdi, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; break;
    case BB_INSN_MOV_RSI_IMM64: emit3c_op(e,"mov","rsi, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; break;
    case BB_INSN_MOV_RDX_IMM64: emit3c_op(e,"mov","rdx, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; break;
    case BB_INSN_MOV_RCX_IMM64: emit3c_op(e,"mov","rcx, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; break;
    /* 32-bit reg ← imm32 */
    case BB_INSN_MOV_ESI_IMM32: emit3c_op(e,"mov","esi, %u",  a1); CTX(e)->pos+=5; break;
    case BB_INSN_MOV_EAX_IMM32: emit3c_op(e,"mov","eax, %u",  a1); CTX(e)->pos+=5; break;
    case BB_INSN_ADD_EAX_IMM32: emit3c_op(e,"add","eax, %u",  a1); CTX(e)->pos+=5; break;
    case BB_INSN_SUB_EAX_IMM32: emit3c_op(e,"sub","eax, %u",  a1); CTX(e)->pos+=5; break;
    case BB_INSN_CMP_EAX_IMM32: emit3c_op(e,"cmp","eax, %u",  a1); CTX(e)->pos+=5; break;
    case BB_INSN_CMP_ESI_IMM8:  emit3c_op(e,"cmp","esi, %u", (unsigned)a2); CTX(e)->pos+=3; break;
    /* memory loads */
    case BB_INSN_MOV_EAX_RCXMEM:   emit3c_op(e,"mov","eax, [rcx]", NULL); CTX(e)->pos+=2; break;
    case BB_INSN_MOV_RAX_RCXMEM:   emit3c_op(e,"mov","rax, [rcx]", NULL); CTX(e)->pos+=3; break;
    case BB_INSN_CMP_EAX_RCXMEM:   emit3c_op(e,"cmp","eax, [rcx]", NULL); CTX(e)->pos+=2; break;
    case BB_INSN_MOV_EAX_R10MEM:   emit3c_op(e,"mov","eax, [r10]", NULL); CTX(e)->pos+=3; break;
    case BB_INSN_MOV_R10MEM_EAX:   emit3c_op(e,"mov","[r10], eax", NULL); CTX(e)->pos+=3; break;
    /* reg-reg */
    case BB_INSN_MOV_ECX_EAX:      emit3c_op(e,"mov","ecx, eax", NULL); CTX(e)->pos+=2; break;
    case BB_INSN_MOV_RDI_RAX:      emit3c_op(e,"mov","rdi, rax", NULL); CTX(e)->pos+=3; break;
    case BB_INSN_MOV_RDX_RAX:      emit3c_op(e,"mov","rdx, rax", NULL); CTX(e)->pos+=3; break;
    case BB_INSN_CMP_EAX_ECX:      emit3c_op(e,"cmp","eax, ecx", NULL); CTX(e)->pos+=2; break;
    case BB_INSN_TEST_EAX_EAX:     emit3c_op(e,"test","eax, eax", NULL); CTX(e)->pos+=2; break;
    case BB_INSN_TEST_RAX_RAX:     emit3c_op(e,"test","rax, rax", NULL); CTX(e)->pos+=3; break;
    case BB_INSN_XOR_EDX_EDX:      emit3c_op(e,"xor","edx, edx", NULL); CTX(e)->pos+=2; break;
    case BB_INSN_MOVSXD_RCX_R10MEM:emit3c_op(e,"movsxd","rcx, dword ptr [r10]", NULL); CTX(e)->pos+=3; break;
    case BB_INSN_LEA_RAX_RAXRCX:   emit3c_op(e,"lea","rax, [rax+rcx]", NULL); CTX(e)->pos+=4; break;
    /* control */
    case BB_INSN_RET:      emit3c_op(e,"ret", NULL); CTX(e)->pos+=1; break;
    case BB_INSN_CALL_RAX: emit3c_op(e,"call","rax", NULL); CTX(e)->pos+=2; break;
    /* EM-7c-symbolic: RIP-relative symbol load and PLT call */
    case BB_INSN_LEA_RCX_SYM:
        emit3c_op(e,"lea","rcx, [rip + %s]", d->sym ? d->sym : "??sym??");
        CTX(e)->pos += 7; break;
    case BB_INSN_CALL_SYM_PLT:
        emit3c_op(e,"call","%s@PLT", d->sym ? d->sym : "??sym??");
        CTX(e)->pos += 5; break;
    /* BB_INSN_LEA_R10_SYM: lea r10, [rip + sym]  (7 bytes) */
    case BB_INSN_LEA_R10_SYM:
        emit3c_op(e,"lea","r10, [rip + %s]", d->sym ? d->sym : "??sym??");
        CTX(e)->pos += 7; break;
    }
}

/* ── label_define: pure-label line (cols 2+3 empty) ──────────────────────── */
static void text_label_define(emitter_v *e, bb_label_t *lbl)
{
    char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", lbl->name);
    bb3c_format(outf(e), lbuf, "", "");
}

/* ── emit_jmp: jump line (col 2 = jmp/je/..., col 3 = target) ────────────── */
static void text_emit_jmp(emitter_v *e, bb_label_t *target, jmp_kind_t kind)
{
    const char *mn[] = {"jmp","je","jne","jl","jge","jg"};
    emit3c_jmp(e, mn[(int)kind < 6 ? (int)kind : 0], target->name);
    CTX(e)->pos += 6;
}

/* ── global_sym: directive line (col 2 = .global, col 3 = name) ──────────── */
static void text_global_sym(emitter_v *e, const char *name)
{
    bb3c_format(outf(e), "", ".global", name ? name : "");
}

/* ── fprintf_raw ───────────────────────────────────────────────────────────── */
static void text_fprintf_raw(emitter_v *e, const char *fmt, ...)
{ va_list ap; va_start(ap,fmt); vfprintf(outf(e),fmt,ap); va_end(ap); }

/* ── pos ───────────────────────────────────────────────────────────────────── */
static int text_pos(emitter_v *e) { return CTX(e)->pos; }

/* ── constructor ───────────────────────────────────────────────────────────── */
static const emitter_v text_tmpl = {
    .emit_insn    = text_emit_insn,
    .label_define = text_label_define,
    .emit_jmp     = text_emit_jmp,
    .global_sym   = text_global_sym,
    .fprintf_raw  = text_fprintf_raw,
    .pos          = text_pos,
    .intern_str   = NULL,   /* set by bb_build_flat_text when strtab is available */
    .is_text      = 1,
    .ctx          = NULL,
};

emitter_v *emitter_text_new(FILE *out)
{
    emitter_v *e = malloc(sizeof(emitter_v));
    if (!e) return NULL;
    *e = text_tmpl;
    text_ctx_t *ctx = calloc(1, sizeof(text_ctx_t));
    if (!ctx) { free(e); return NULL; }
    ctx->out = out; ctx->pos = 0;
    e->ctx = ctx;
    return e;
}

void emitter_free(emitter_v *e) { if (!e) return; free(e->ctx); free(e); }

int emitter_end(emitter_v *e)
{
    if (!e) return 0;
    if (e->emit_insn == text_emit_insn)
        return CTX(e)->pos;
    return bb_emit_end();
}
