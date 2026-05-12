/*
 * emitter_text.c — GAS/text-mode implementation of emitter_t
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

#include "emitter.h"
#include "bb_emit.h"     /* bb3c_format */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct { FILE *out; int pos; } text_ctx_t;
#define CTX(e) ((text_ctx_t *)((e)->ctx))
static FILE *outf(emitter_t *e) { FILE *f = CTX(e)->out; return f ? f : stdout; }

/* ── three-column shorthands ──────────────────────────────────────────────── */
/* EM-7c-bb-three-column-split (2026-05-09): take mnemonic and args separately,
 * so col 2 holds ONLY the mnemonic and col 3 holds the operands.  The prior
 * emit3c_action stuffed both into col 2 and overflowed the 16-wide field. */

static void emit3c_op(emitter_t *e, const char *mn, const char *fmt, ...)
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

/* Goto-only: col 1 empty, col 2 = jmp/je/jne/..., col 3 = target.
 * EM-FORMAT-BB-FUSED-GOTOS: routes through bb3c_emit_jmp for cond+uncond
 * fusion (defined in bb_emit.c). */
static void emit3c_jmp(emitter_t *e, const char *mn, const char *target)
{
    bb3c_emit_jmp(outf(e), mn ? mn : "", target ? target : "");
}

/* ── emit_insn: one three-column line per instruction (action column) ─────── */

static void text_emit_insn(emitter_t *e, const bb_insn_desc_t *d)
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
    /* EM-MODE4-IS-MODE3-DUMP-c: SM-State field manipulation */
    case BB_INSN_INC_MEM_R13_DISP8:
        emit3c_op(e,"inc","dword ptr [r13 + %u]", (unsigned)a2);
        CTX(e)->pos += 4; break;
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
    /* push r10 / pop r10 — preserve flat-BB LOCAL across runtime call (2 bytes each) */
    case BB_INSN_PUSH_R10:
        emit3c_op(e,"push","r10", NULL); CTX(e)->pos += 2; break;
    case BB_INSN_POP_R10:
        emit3c_op(e,"pop","r10", NULL); CTX(e)->pos += 2; break;
    case BB_INSN_POP_RBP:
        emit3c_op(e,"pop","rbp", NULL); CTX(e)->pos += 1; break;
    }
}

/* ── label_define: pure-label line (cols 2+3 empty) ──────────────────────── */
static void text_label_define(emitter_t *e, bb_label_t *lbl)
{
    char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", lbl->name);
    bb3c_format(outf(e), lbuf, "", "");
}

/* ── emit_jmp: jump line (col 2 = jmp/je/..., col 3 = target) ────────────── */
static void text_emit_jmp(emitter_t *e, bb_label_t *target, jmp_kind_t kind)
{
    const char *mn[] = {"jmp","je","jne","jl","jge","jg"};
    emit3c_jmp(e, mn[(int)kind < 6 ? (int)kind : 0], target->name);
    CTX(e)->pos += 6;
}

/* ── global_sym: directive line (col 2 = .global, col 3 = name) ──────────── */
static void text_global_sym(emitter_t *e, const char *name)
{
    bb3c_format(outf(e), "", ".global", name ? name : "");
}

/* ── fprintf_raw ───────────────────────────────────────────────────────────── */
static void text_fprintf_raw(emitter_t *e, const char *fmt, ...)
{
    /* EM-FORMAT-BB-FUSED-GOTOS: any raw text emission (banners, EV_TEXT
     * blocks) MUST flush any deferred cond-jmp first, otherwise the
     * cond-jmp would land AFTER the raw text in the file when it
     * eventually flushes.  But we MUST NOT flush a pending label here —
     * that would regress EM-FORMAT-BB-LONE-LABELS by emitting the label
     * standalone before the banner.  Pending label stays in buffer; the
     * next bb3c_format call after the banner consumes it via the
     * empty-col-1 fusion path. */
    bb3c_flush_pending_cjmp_only();
    va_list ap; va_start(ap,fmt); vfprintf(outf(e),fmt,ap); va_end(ap);
}

/* ── pos ───────────────────────────────────────────────────────────────────── */
static int text_pos(emitter_t *e) { return CTX(e)->pos; }

/* ── EM-MODE4-IS-MODE3-DUMP-b: new surface for SM templates ──────────────────
 *
 * Each method below corresponds to one entry in the design-doc vtable
 * (MIGRATION-MODE4-IS-MODE3-DUMP.md §"The vtable surface").  Text-backend
 * implementations emit GAS asm directly.  No template calls these yet —
 * sub-rung -c is the first caller (SM_HALT).  Implementations here are
 * faithful to the design but trivially small until they have real
 * callers; revisit if a caller demands different shape. */

/* ── structural markers ────────────────────────────────────────────────── */

static void text_label_name(emitter_t *e, const char *name)
{
    char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", name ? name : "");
    bb3c_format(outf(e), lbuf, "", "");
}

static void text_pc_label(emitter_t *e, int pc)
{
    char lbuf[64]; snprintf(lbuf, sizeof(lbuf), ".L%d:", pc);
    bb3c_format(outf(e), lbuf, "", "");
}

static void text_section(emitter_t *e, const char *name)
{
    /* Canonical short forms: .text / .data / .rodata / .bss go in
     * directly; anything else gets ".section <name>" wrapping. */
    if (!name) return;
    bb3c_flush_pending_cjmp_only();
    if (name[0] == '.' &&
        (strcmp(name, ".text") == 0 || strcmp(name, ".data") == 0 ||
         strcmp(name, ".rodata") == 0 || strcmp(name, ".bss") == 0))
    {
        fprintf(outf(e), "%s\n", name);
    } else {
        fprintf(outf(e), ".section %s\n", name);
    }
}

static void text_directive(emitter_t *e, const char *line)
{
    if (!line) return;
    bb3c_flush_pending_cjmp_only();
    fprintf(outf(e), "    %s\n", line);
}

static void text_data_quad(emitter_t *e, uint64_t val)
{
    char buf[40]; snprintf(buf, sizeof(buf), "0x%llx", (unsigned long long)val);
    bb3c_format(outf(e), "", ".quad", buf);
}

static void text_data_quad_sym(emitter_t *e, const char *sym)
{
    bb3c_format(outf(e), "", ".quad", sym ? sym : "0");
}

static void text_data_string(emitter_t *e, const char *bytes, size_t len)
{
    /* Emit as ".ascii \"...\"" with C-style escaping for non-printable
     * bytes and the four reserved characters \\ " \n \t. */
    if (!bytes) return;
    bb3c_flush_pending_cjmp_only();
    FILE *f = outf(e);
    fputs("    .ascii \"", f);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)bytes[i];
        switch (c) {
        case '\\': fputs("\\\\", f); break;
        case '"' : fputs("\\\"", f); break;
        case '\n': fputs("\\n",  f); break;
        case '\t': fputs("\\t",  f); break;
        default:
            if (c >= 0x20 && c < 0x7f) fputc(c, f);
            else                       fprintf(f, "\\x%02x", c);
            break;
        }
    }
    fputs("\"\n", f);
}

static void text_data_long(emitter_t *e, int32_t val)
{
    char buf[24]; snprintf(buf, sizeof(buf), "%d", (int)val);
    bb3c_format(outf(e), "", ".long", buf);
}

static void text_bb_zeta_rdi(emitter_t *e, uint64_t ptr, const char *sym)
{
    /* TEXT: lea rdi, [rip + sym] — RIP-relative reference to static .data zeta. */
    (void)ptr;
    char arg[128];
    snprintf(arg, sizeof(arg), "rdi, [rip + %s]", sym ? sym : "0");
    bb3c_format(outf(e), "", "lea", arg);
}

static void text_bb_dispatch_jne_jmp(emitter_t *e,
                                     bb_label_t *lbl_succ, bb_label_t *lbl_fail)
{
    /* TEXT: fused three-column "test rax,rax; jne x; jmp y". */
    char buf[256];
    int o = 0;
    o += snprintf(buf + o, sizeof(buf) - o, "rax, rax;");
    while (o < 27 && o < (int)sizeof(buf) - 1) buf[o++] = ' ';
    buf[o] = '\0';
    snprintf(buf + o, sizeof(buf) - o, "jne %s; jmp %s",
             lbl_succ ? lbl_succ->name : "?",
             lbl_fail ? lbl_fail->name : "?");
    bb3c_format(outf(e), "", "test", buf);
}

static void text_pad_to_blob_size(emitter_t *e)
{
    /* TEXT: no-op (text size is not byte-counted by GAS in any way that
     * affects the asm output).  Templates may call this freely. */
    (void)e;
}

/* ── BB-port primitives (Greek-port semantic labels α/β/γ/ω) ──────────── */

static const char *greek_for_port(char port)
{
    /* Accept ASCII a/b/g/o as canonical inputs (templates pass these as
     * char constants; UTF-8 Greek can't fit in a char anyway).  Map to
     * the UTF-8 Greek glyphs in the emitted label. */
    switch (port) {
    case 'a': return "α";    /* alpha — try */
    case 'b': return "β";    /* beta — retry */
    case 'g': return "γ";    /* gamma — succeed */
    case 'o': return "ω";    /* omega — fail */
    default:  return "?";
    }
}

static void text_bb_port_label(emitter_t *e, const char *box_prefix, char port)
{
    char lbuf[256];
    snprintf(lbuf, sizeof(lbuf), "%s_%s:",
             box_prefix ? box_prefix : "", greek_for_port(port));
    bb3c_format(outf(e), lbuf, "", "");
}

static void text_bb_port_jmp(emitter_t *e, const char *box_prefix, char port)
{
    char tbuf[256];
    snprintf(tbuf, sizeof(tbuf), "%s_%s",
             box_prefix ? box_prefix : "", greek_for_port(port));
    bb3c_emit_jmp(outf(e), "jmp", tbuf);
    CTX(e)->pos += 5;
}

static void text_bb_box_banner(emitter_t *e, const char *kind, const char *args)
{
    /* 120-char minor rule + "    # BOX <kind>(<args>)" line.  The
     * existing flat_emit_banner_rule shape (sess 2026-05-10
     * EM-FORMAT-BANNER-COLLAPSE-SPACE) drops the space between # and
     * the rule character; match it. */
    bb3c_flush_pending_cjmp_only();
    FILE *f = outf(e);
    fputs("#", f);
    for (int i = 1; i < 120; i++) fputc('-', f);
    fputc('\n', f);
    fprintf(f, "    # BOX %s(%s)\n",
            kind ? kind : "?", args ? args : "");
}

/* ── formatting / readability ──────────────────────────────────────────── */

static void text_comment(emitter_t *e, const char *text)
{
    bb3c_flush_pending_cjmp_only();
    fprintf(outf(e), "    # %s\n", text ? text : "");
}

static void text_banner(emitter_t *e, const char *text)
{
    bb3c_flush_pending_cjmp_only();
    FILE *f = outf(e);
    fputs("#", f);
    for (int i = 1; i < 120; i++) fputc('=', f);
    fputc('\n', f);
    fprintf(f, "    # %s\n", text ? text : "");
    fputs("#", f);
    for (int i = 1; i < 120; i++) fputc('=', f);
    fputc('\n', f);
}

static void text_minor_break(emitter_t *e, const char *text)
{
    bb3c_flush_pending_cjmp_only();
    FILE *f = outf(e);
    fputs("#", f);
    for (int i = 1; i < 120; i++) fputc('-', f);
    fputc('\n', f);
    if (text && *text) fprintf(f, "    # %s\n", text);
}

static void text_blank_line(emitter_t *e)
{
    bb3c_flush_pending_cjmp_only();
    fputc('\n', outf(e));
}

/* ── macro_def hooks (text-INVOCATION mode: emit macro call + suppress body)
 *    (text-DEFINITION mode: emit .macro NAME … .endm) ──────────────────── */

/* Body-suppression flag for INVOCATION mode lives in text_ctx_t.suppress.
 * Extending the context here would require a struct-layout change to
 * text_ctx_t — for sub-rung -b we keep the suppress flag as a backend-
 * private static, since this surface has no caller yet.  Sub-rung -c
 * will revisit if needed (e.g. if SM_HALT's template wants the
 * INVOCATION+suppress behaviour).  TODO: promote to ctx field if -c
 * needs per-emitter state. */
static int g_text_macro_suppress = 0;

static void text_macro_begin(emitter_t *e, const char *name,
                             const char *const *params, int nparams)
{
    bb3c_flush_pending_cjmp_only();
    FILE *f = outf(e);
    if (e->text_mode == TEXT_MODE_DEFINITION) {
        fprintf(f, ".macro %s", name ? name : "?");
        for (int i = 0; i < nparams; i++) {
            fprintf(f, " %s", params[i] ? params[i] : "?");
        }
        fputc('\n', f);
    } else {
        /* INVOCATION mode: emit just the call site, body suppressed. */
        fprintf(f, "    %s", name ? name : "?");
        for (int i = 0; i < nparams; i++) {
            fprintf(f, "%s%s", (i == 0 ? " " : ", "),
                    params[i] ? params[i] : "?");
        }
        fputc('\n', f);
        g_text_macro_suppress = 1;
    }
}

static void text_macro_param_ref(emitter_t *e, const char *name)
{
    /* Only meaningful inside a macro body when DEFINITION mode. */
    if (e->text_mode != TEXT_MODE_DEFINITION) return;
    fprintf(outf(e), "\\%s", name ? name : "?");
}

static void text_macro_end(emitter_t *e)
{
    if (e->text_mode == TEXT_MODE_DEFINITION) {
        bb3c_flush_pending_cjmp_only();
        fputs(".endm\n", outf(e));
    } else {
        g_text_macro_suppress = 0;
    }
}

/* ── constructor ───────────────────────────────────────────────────────────── */
static const emitter_t text_tmpl = {
    .emit_insn    = text_emit_insn,
    .label_define = text_label_define,
    .emit_jmp     = text_emit_jmp,
    .global_sym   = text_global_sym,
    .fprintf_raw  = text_fprintf_raw,
    .pos          = text_pos,
    .intern_str   = NULL,   /* set by bb_build_flat_text when strtab is available */
    /* EM-MODE4-IS-MODE3-DUMP-b extensions */
    .label_name       = text_label_name,
    .pc_label         = text_pc_label,
    .section          = text_section,
    .directive        = text_directive,
    .data_quad        = text_data_quad,
    .data_quad_sym    = text_data_quad_sym,
    .data_string          = text_data_string,
    .data_long            = text_data_long,
    .bb_zeta_rdi          = text_bb_zeta_rdi,
    .bb_dispatch_jne_jmp  = text_bb_dispatch_jne_jmp,
    .pad_to_blob_size     = text_pad_to_blob_size,
    .bb_port_label    = text_bb_port_label,
    .bb_port_jmp      = text_bb_port_jmp,
    .bb_box_banner    = text_bb_box_banner,
    .comment          = text_comment,
    .banner           = text_banner,
    .minor_break      = text_minor_break,
    .blank_line       = text_blank_line,
    .macro_begin      = text_macro_begin,
    .macro_param_ref  = text_macro_param_ref,
    .macro_end        = text_macro_end,
    .text_mode    = TEXT_MODE_INVOCATION,
    .is_text      = 1,
    .ctx          = NULL,
};

emitter_t *emitter_text_new(FILE *out)
{
    return emitter_text_new_mode(out, TEXT_MODE_INVOCATION);
}

emitter_t *emitter_text_new_mode(FILE *out, emitter_text_mode_t mode)
{
    emitter_t *e = malloc(sizeof(emitter_t));
    if (!e) return NULL;
    *e = text_tmpl;
    e->text_mode = mode;
    text_ctx_t *ctx = calloc(1, sizeof(text_ctx_t));
    if (!ctx) { free(e); return NULL; }
    ctx->out = out; ctx->pos = 0;
    e->ctx = ctx;
    return e;
}

void emitter_free(emitter_t *e) { if (!e) return; free(e->ctx); free(e); }

/* EM-FORMAT-BB lone-label fusion (2026-05-09):
 * Public accessor so callers in bb_flat.c can route their emissions
 * through bb_emit.c's `bb3c_format`, sharing the pending-label fusion
 * buffer.  Returns NULL for non-text emitters (binary mode). */
FILE *emitter_text_file(emitter_t *e)
{
    if (!e || e->emit_insn != text_emit_insn) return NULL;
    return outf(e);
}

int emitter_end(emitter_t *e)
{
    if (!e) return 0;
    if (e->emit_insn == text_emit_insn)
        return CTX(e)->pos;
    return bb_emit_end();
}
