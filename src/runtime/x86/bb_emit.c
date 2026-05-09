/*
 * bb_emit.c — Dual-Mode x86-64 Emitter (M-DYN-1)
 *
 * See bb_emit.h for design notes.
 */

#include "bb_emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

/* ── global state ───────────────────────────────────────────────────────── */

bb_emit_mode_t  bb_emit_mode = EMIT_TEXT;
FILE           *bb_emit_out  = NULL;   /* set by caller; defaults to stdout */

bb_buf_t        bb_emit_buf  = NULL;
int             bb_emit_pos  = 0;
int             bb_emit_size = 0;

bb_patch_t      bb_patch_list[BB_PATCH_MAX];
int             bb_patch_count = 0;

/* ── label ──────────────────────────────────────────────────────────────── */

void bb_label_init(bb_label_t *lbl, const char *name)
{
    strncpy(lbl->name, name, BB_LABEL_NAME_MAX - 1);
    lbl->name[BB_LABEL_NAME_MAX - 1] = '\0';
    lbl->offset = BB_LABEL_UNRESOLVED;
}

void bb_label_initf(bb_label_t *lbl, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(lbl->name, BB_LABEL_NAME_MAX, fmt, ap);
    va_end(ap);
    lbl->offset = BB_LABEL_UNRESOLVED;
}

void bb_label_define(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        FILE *f = bb_emit_out ? bb_emit_out : stdout;
        char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", lbl->name);
        bb3c_format(f, lbuf, "", "");
        return;
    }

    /* Binary mode: record offset, patch all pending forward refs */
    lbl->offset = bb_emit_pos;

    for (int i = 0; i < bb_patch_count; i++) {
        bb_patch_t *p = &bb_patch_list[i];
        if (p->label != lbl) continue;

        int target = lbl->offset;

        if (p->kind == PATCH_REL8) {
            /* rel8: displacement = target - (site + 1) */
            int disp = target - (p->site + 1);
            if (disp < -128 || disp > 127) {
                fprintf(stderr,
                        "bb_label_define: rel8 overflow for '%s': disp=%d\n",
                        lbl->name, disp);
                abort();
            }
            bb_emit_buf[p->site] = (uint8_t)(int8_t)disp;
        } else {
            /* rel32: displacement = target - (site + 4) */
            int disp = target - (p->site + 4);
            uint32_t u;
            memcpy(&u, &disp, 4);
            bb_emit_buf[p->site + 0] = (uint8_t)(u      );
            bb_emit_buf[p->site + 1] = (uint8_t)(u >>  8);
            bb_emit_buf[p->site + 2] = (uint8_t)(u >> 16);
            bb_emit_buf[p->site + 3] = (uint8_t)(u >> 24);
        }

        /* Remove from patch list (swap with last) */
        bb_patch_list[i] = bb_patch_list[--bb_patch_count];
        i--;   /* re-check this slot */
    }
}

/* ── session ────────────────────────────────────────────────────────────── */

void bb_emit_begin(bb_buf_t buf, int size)
{
    bb_emit_buf   = buf;
    bb_emit_pos   = 0;
    bb_emit_size  = size;
    bb_patch_count = 0;
}

int bb_emit_end(void)
{
    if (bb_patch_count > 0) {
        fprintf(stderr, "bb_emit_end: %d unresolved forward reference(s):\n",
                bb_patch_count);
        for (int i = 0; i < bb_patch_count; i++)
            fprintf(stderr, "  site=%d label='%s'\n",
                    bb_patch_list[i].site,
                    bb_patch_list[i].label->name);
        abort();
    }
    return bb_emit_pos;
}

/* ── patch helpers ──────────────────────────────────────────────────────── */

void bb_emit_patch_rel8(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        /* EM-7c-bb-three-column: a direct rel8 patch from TEXT mode
         * means the emitter is in BINARY-byte mode by mistake.  TEXT
         * jumps go via bb_insn_*_rel8 / EV_JMP which emit "jmp <name>"
         * by mnemonic. */
        fprintf(stderr,
                "bb_emit_patch_rel8: TEXT-mode reach (target='%s') — "
                "use bb_insn_*_rel8 mnemonic helpers\n",
                lbl->name);
        abort();
    }
    if (bb_label_defined(lbl)) {
        /* Already resolved — emit directly */
        int disp = lbl->offset - (bb_emit_pos + 1);
        if (disp < -128 || disp > 127) {
            fprintf(stderr,
                    "bb_emit_patch_rel8: rel8 overflow for '%s': disp=%d\n",
                    lbl->name, disp);
            abort();
        }
        bb_emit_byte((uint8_t)(int8_t)disp);
        return;
    }
    /* Unresolved: record patch site, emit placeholder */
    if (bb_patch_count >= BB_PATCH_MAX) {
        fprintf(stderr, "bb_emit_patch_rel8: patch list full\n");
        abort();
    }
    bb_patch_list[bb_patch_count].site  = bb_emit_pos;
    bb_patch_list[bb_patch_count].label = lbl;
    bb_patch_list[bb_patch_count].kind  = PATCH_REL8;
    bb_patch_count++;
    bb_emit_byte(0x00);   /* placeholder */
}

void bb_emit_patch_rel32(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        /* EM-7c-bb-three-column: a direct rel32 patch from TEXT mode
         * means the emitter is in BINARY-byte mode by mistake.  TEXT
         * jumps go via bb_insn_*_rel32 / EV_JMP which emit "jmp <name>"
         * by mnemonic. */
        fprintf(stderr,
                "bb_emit_patch_rel32: TEXT-mode reach (target='%s') — "
                "use bb_insn_*_rel32 mnemonic helpers\n",
                lbl->name);
        abort();
    }
    if (bb_label_defined(lbl)) {
        int disp = lbl->offset - (bb_emit_pos + 4);
        bb_emit_i32(disp);
        return;
    }
    if (bb_patch_count >= BB_PATCH_MAX) {
        fprintf(stderr, "bb_emit_patch_rel32: patch list full\n");
        abort();
    }
    bb_patch_list[bb_patch_count].site  = bb_emit_pos;
    bb_patch_list[bb_patch_count].label = lbl;
    bb_patch_list[bb_patch_count].kind  = PATCH_REL32;
    bb_patch_count++;
    bb_emit_u32(0x00000000);   /* placeholder */
}

/* ── byte primitives ────────────────────────────────────────────────────── */

/*
 * EM-7b: byte primitives are dual-mode.
 *
 * BINARY: write to bb_emit_buf, advance bb_emit_pos, bounds-check.
 * TEXT:   emit ".byte 0xNN" line to bb_emit_out.  No buffer, no
 *         bounds check.  bb_emit_pos still advances so that any code
 *         interested in "how far have I emitted" sees a consistent
 *         counter — but TEXT-mode label patching does not need it
 *         (jumps go through symbolic helpers like bb_insn_jmp_rel32
 *         which emit "jmp <name>" and let the assembler resolve).
 *
 * Path 1 from the EM-7b scoping audit (sess #74): preserves bb_flat.c
 * call sites unchanged.  The resulting .s contains walls of .byte
 * directives — acceptable for now per the readability standard
 * (mode-4 readability lives in the SM-side emitter; BB-box bytes
 * may be raw .byte sequences linked via external α/β/γ/ω labels).
 */

void bb_emit_byte(uint8_t b)
{
    if (bb_emit_mode == EMIT_TEXT) {
        /* EM-7c-bb-three-column: TEXT mode never emits .byte hex walls.
         * Every reachable instruction routes through a named bb_insn_*
         * helper (mnemonic) or emitter_v->emit_insn (mnemonic).  A
         * direct bb_emit_byte from TEXT mode is a bug — caller is
         * leaking BINARY-mode byte knowledge into the text path. */
        fprintf(stderr,
                "bb_emit_byte: TEXT-mode reach (b=0x%02x) — "
                "convert caller to a named bb_insn_* helper\n",
                (unsigned)b);
        abort();
    }
    if (bb_emit_pos >= bb_emit_size) {
        fprintf(stderr, "bb_emit_byte: buffer overflow at pos=%d size=%d\n",
                bb_emit_pos, bb_emit_size);
        abort();
    }
    bb_emit_buf[bb_emit_pos++] = b;
}

void bb_emit_u16(uint16_t v)
{
    bb_emit_byte((uint8_t)(v     ));
    bb_emit_byte((uint8_t)(v >> 8));
}

void bb_emit_u32(uint32_t v)
{
    bb_emit_byte((uint8_t)(v      ));
    bb_emit_byte((uint8_t)(v >>  8));
    bb_emit_byte((uint8_t)(v >> 16));
    bb_emit_byte((uint8_t)(v >> 24));
}

void bb_emit_u64(uint64_t v)
{
    bb_emit_u32((uint32_t)(v      ));
    bb_emit_u32((uint32_t)(v >> 32));
}

void bb_emit_i8(int8_t v)   { bb_emit_byte((uint8_t)v); }
void bb_emit_i32(int32_t v) { uint32_t u; memcpy(&u, &v, 4); bb_emit_u32(u); }

/* ── text mode helpers ───────────────────────────────────────────────────── */

void bb_text(const char *fmt, ...)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
}

void bb_text_label(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        FILE *f = bb_emit_out ? bb_emit_out : stdout;
        char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", lbl->name);
        bb3c_format(f, lbuf, "", "");
    } else {
        bb_label_define(lbl);
    }
}

void bb_text_comment(const char *fmt, ...)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    fprintf(f, "; ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
}

/* ── BB three-column line emission (EM-7c-bb-three-column) ──────────────────
 *
 *   LABEL:                  ACTION           GOTO
 *   col 1 (24 wide)         col 2 (16 wide)  col 3 (free)
 *
 * EM-7c-s-file-beautify (2026-05-09): removed the literal `;` separators
 * that the prior PARTIAL rung introduced.  The shape now matches SM-side
 * (`emit_three_column_line` in sm_codegen_x64_emit.c, sm_line, and
 * sm_emit_template's `render_call_line`) — one printf format
 * `%-24s%-16s %s\n` shared across the entire `.s` file.
 *
 * NULL or empty args render as whitespace of the right column width.
 * Trailing whitespace is preserved (cheap; keeps every line the same
 * shape; readers can strip if desired).  Each line ends with one '\n'.
 *
 * Examples:
 *   bb3c_format(f, "_α:",     "",                "");           // pure label
 *   bb3c_format(f, "",        "lea r10, [rip+Δ]","");           // pure action
 *   bb3c_format(f, "",        "",                "jmp _β");     // pure goto
 *   bb3c_format(f, "_body:",  "cmp esi, 0",      "je _α_body"); // merged
 */
void bb3c_format(FILE *out, const char *label, const char *action, const char *goto_)
{
    const char *L = label  ? label  : "";
    const char *A = action ? action : "";
    const char *G = goto_  ? goto_  : "";
    /* EM-7c-no-trailing-ws (2026-05-09): build into a buffer, right-trim
     * trailing whitespace, write + '\n'.  The %-24s%-16s padding produces
     * trailing spaces on label-only or empty-col3 lines; strip them so no
     * generated line has trailing whitespace before its newline. */
    char buf[768];
    int n = snprintf(buf, sizeof(buf), "%-24s%-16s %s", L, A, G);
    if (n < 0) return;
    if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
    while (n > 0 && (buf[n-1] == ' ' || buf[n-1] == '\t')) n--;
    buf[n] = '\0';
    fputs(buf, out);
    fputc('\n', out);
}

void bb3c_text(const char *label, const char *action, const char *goto_)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    bb3c_format(f, label, action, goto_);
}

/* ── x86-64 instruction helpers ─────────────────────────────────────────── */

/* EM-7c-bb-three-column-split: split (mnemonic, args) emission.
 * Col 1 empty, col 2 = mnemonic ONLY (16-wide), col 3 = formatted args.
 * Replaces the prior bb3c_action which fused mnemonic+args into col 2 and
 * overflowed the 16-wide field on every load-immediate / memory-op line. */
static void bb3c_op(const char *mn, const char *fmt, ...)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    char argbuf[256];
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(argbuf, sizeof(argbuf), fmt, ap);
        va_end(ap);
    } else {
        argbuf[0] = '\0';
    }
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    bb3c_format(f, "", mn ? mn : "", argbuf);
}

/* Goto-only line: col 1 empty, col 2 = jmp/je/jne/..., col 3 = target. */
static void bb3c_jmp(const char *mn, const char *target)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    bb3c_format(f, "", mn ? mn : "", target ? target : "");
}

void bb_insn_mov_eax_imm32(uint32_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("mov", "eax, %u", imm);
        return;
    }
    bb_emit_byte(0xB8);
    bb_emit_u32(imm);
}

void bb_insn_mov_rax_imm64(uint64_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("mov", "rax, 0x%llx", (unsigned long long)imm);
        return;
    }
    /* REX.W + B8 /r  — mov rax, imm64 */
    bb_emit_byte(0x48);
    bb_emit_byte(0xB8);
    bb_emit_u64(imm);
}

void bb_insn_ret(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("ret", NULL); return; }
    bb_emit_byte(0xC3);
}

void bb_insn_nop(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("nop", NULL); return; }
    bb_emit_byte(0x90);
}

void bb_insn_call_rax(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("call", "rax"); return; }
    bb_emit_byte(0xFF); bb_emit_byte(0xD0);
}

void bb_insn_jmp_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jmp", target->name); return;
    }
    bb_emit_byte(0xEB);
    bb_emit_patch_rel8(target);
}

void bb_insn_jmp_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jmp", target->name); return;
    }
    bb_emit_byte(0xE9);
    bb_emit_patch_rel32(target);
}

void bb_insn_jl_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jl", target->name); return;
    }
    bb_emit_byte(0x7C);
    bb_emit_patch_rel8(target);
}

void bb_insn_jge_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jge", target->name); return;
    }
    bb_emit_byte(0x7D);
    bb_emit_patch_rel8(target);
}

void bb_insn_je_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("je", target->name); return;
    }
    bb_emit_byte(0x74);
    bb_emit_patch_rel8(target);
}

void bb_insn_jne_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jne", target->name); return;
    }
    bb_emit_byte(0x75);
    bb_emit_patch_rel8(target);
}

void bb_insn_jne_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jne", target->name); return;
    }
    bb_emit_byte(0x0F); bb_emit_byte(0x85);
    bb_emit_patch_rel32(target);
}

/* jg rel32 — jump if greater (signed), near, forward ref (EM-7b) */
void bb_insn_jg_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jg", target->name); return;
    }
    bb_emit_byte(0x0F); bb_emit_byte(0x8F);
    bb_emit_patch_rel32(target);
}

void bb_insn_cmp_esi_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("cmp", "esi, %u", (unsigned)imm); return;
    }
    bb_emit_byte(0x83); bb_emit_byte(0xFE); bb_emit_byte(imm);
}

void bb_insn_cmp_esi_imm32(uint32_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("cmp", "esi, %u", imm); return;
    }
    bb_emit_byte(0x81); bb_emit_byte(0xFE); bb_emit_u32(imm);
}

void bb_insn_movzx_eax_rdi_off8(uint8_t off)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("movzx", "eax, byte [rdi + %u]", (unsigned)off); return;
    }
    /* 0F B6 47 imm8 */
    bb_emit_byte(0x0F); bb_emit_byte(0xB6);
    bb_emit_byte(0x47); bb_emit_byte(off);
}

void bb_insn_cmp_al_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("cmp", "al, %u", (unsigned)imm); return;
    }
    bb_emit_byte(0x3C); bb_emit_byte(imm);
}

void bb_insn_xor_eax_eax(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("xor", "eax, eax"); return; }
    bb_emit_byte(0x31); bb_emit_byte(0xC0);
}

void bb_insn_push_rbp(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("push", "rbp"); return; }
    bb_emit_byte(0x55);
}

void bb_insn_pop_rbp(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("pop", "rbp"); return; }
    bb_emit_byte(0x5D);
}

void bb_insn_mov_rbp_rsp(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("mov", "rbp, rsp"); return; }
    /* REX.W + 89 /r  — mov rbp, rsp  (89 E5) */
    bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0xE5);
}

void bb_insn_sub_rsp_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("sub", "rsp, %u", (unsigned)imm); return;
    }
    bb_emit_byte(0x48); bb_emit_byte(0x83); bb_emit_byte(0xEC);
    bb_emit_byte(imm);
}

void bb_insn_add_rsp_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("add", "rsp, %u", (unsigned)imm); return;
    }
    bb_emit_byte(0x48); bb_emit_byte(0x83); bb_emit_byte(0xC4);
    bb_emit_byte(imm);
}
