/*
 * emitter.c — unified binary+text emitter implementation
 *
 * Each leaf primitive reads bb_emit_mode (or e->is_text) and produces
 * either raw x86-64 bytes (BINARY_WIRED) or GAS mnemonics (TEXT).
 * The two output paths are side-by-side in every function so they can
 * be checked against each other at a glance.  No parallel files.
 *
 * Replaces: emitter_binary.c + emitter_text.c  (EM-UNIFY-b)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-UNIFY / GOAL-MODE4-EMIT
 */

#include "emitter.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* ── context ──────────────────────────────────────────────────────────────── */

typedef struct { FILE *out; int pos; } text_ctx_t;
#define CTX(e)  ((text_ctx_t *)((e)->ctx))
static FILE *outf(emitter_t *e) { FILE *f = CTX(e)->out; return f ? f : stdout; }

/* ── binary byte helpers (private) ───────────────────────────────────────── */

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

/* ── text three-column helpers (private) ──────────────────────────────────── */

static void emit3c_op(emitter_t *e, const char *mn, const char *fmt, ...)
{
    char buf[256]; buf[0] = '\0';
    if (fmt) { va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap); }
    bb3c_format(outf(e), "", mn ? mn : "", buf);
}
static void emit3c_jmp(emitter_t *e, const char *mn, const char *target)
{ bb3c_emit_jmp(outf(e), mn ? mn : "", target ? target : ""); }

/* ── emit_insn — binary bytes AND text mnemonic, side by side ────────────── */

static void unified_emit_insn(emitter_t *e, const bb_insn_desc_t *d)
{
    int txt = e->is_text;
    uint64_t a0 = d->a0;
    uint32_t a1 = d->a1;
    uint8_t  a2 = d->a2;

    switch (d->kind) {
    /* 64-bit reg ← imm64 */
    case BB_INSN_MOV_R10_IMM64:
        if (txt) { emit3c_op(e,"mov","r10, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; }
        else     { b2(0x49,0xBA); imm64(a0); }
        break;
    case BB_INSN_MOV_RAX_IMM64:
        if (txt) { emit3c_op(e,"mov","rax, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; }
        else     { b2(0x48,0xB8); imm64(a0); }
        break;
    case BB_INSN_MOV_RDI_IMM64:
        if (txt) { emit3c_op(e,"mov","rdi, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; }
        else     { b2(0x48,0xBF); imm64(a0); }
        break;
    case BB_INSN_MOV_RSI_IMM64:
        if (txt) { emit3c_op(e,"mov","rsi, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; }
        else     { b2(0x48,0xBE); imm64(a0); }
        break;
    case BB_INSN_MOV_RDX_IMM64:
        if (txt) { emit3c_op(e,"mov","rdx, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; }
        else     { b2(0x48,0xBA); imm64(a0); }
        break;
    case BB_INSN_MOV_RCX_IMM64:
        if (txt) { emit3c_op(e,"mov","rcx, 0x%llx",(unsigned long long)a0); CTX(e)->pos+=10; }
        else     { b2(0x48,0xB9); imm64(a0); }
        break;
    /* 32-bit reg ← imm32 */
    case BB_INSN_MOV_ESI_IMM32:
        if (txt) { emit3c_op(e,"mov","esi, %u",a1); CTX(e)->pos+=5; }
        else     { b1(0xBE); imm32(a1); }
        break;
    case BB_INSN_MOV_EAX_IMM32:
        if (txt) { emit3c_op(e,"mov","eax, %u",a1); CTX(e)->pos+=5; }
        else     { b1(0xB8); imm32(a1); }
        break;
    case BB_INSN_ADD_EAX_IMM32:
        if (txt) { emit3c_op(e,"add","eax, %u",a1); CTX(e)->pos+=5; }
        else     { b1(0x05); imm32(a1); }
        break;
    case BB_INSN_SUB_EAX_IMM32:
        if (txt) { emit3c_op(e,"sub","eax, %u",a1); CTX(e)->pos+=5; }
        else     { b1(0x2D); imm32(a1); }
        break;
    case BB_INSN_CMP_EAX_IMM32:
        if (txt) { emit3c_op(e,"cmp","eax, %u",a1); CTX(e)->pos+=5; }
        else     { b1(0x3D); imm32(a1); }
        break;
    case BB_INSN_CMP_ESI_IMM8:
        if (txt) { emit3c_op(e,"cmp","esi, %u",(unsigned)a2); CTX(e)->pos+=3; }
        else     { b2(0x83,0xFE); b1(a2); }
        break;
    /* memory loads/stores */
    case BB_INSN_MOV_EAX_RCXMEM:
        if (txt) { emit3c_op(e,"mov","eax, [rcx]",NULL); CTX(e)->pos+=2; }
        else     { b2(0x8B,0x01); }
        break;
    case BB_INSN_MOV_RAX_RCXMEM:
        if (txt) { emit3c_op(e,"mov","rax, [rcx]",NULL); CTX(e)->pos+=3; }
        else     { b3(0x48,0x8B,0x01); }
        break;
    case BB_INSN_CMP_EAX_RCXMEM:
        if (txt) { emit3c_op(e,"cmp","eax, [rcx]",NULL); CTX(e)->pos+=2; }
        else     { b2(0x3B,0x01); }
        break;
    case BB_INSN_MOV_EAX_R10MEM:
        if (txt) { emit3c_op(e,"mov","eax, [r10]",NULL); CTX(e)->pos+=3; }
        else     { b3(0x41,0x8B,0x02); }
        break;
    case BB_INSN_MOV_R10MEM_EAX:
        if (txt) { emit3c_op(e,"mov","[r10], eax",NULL); CTX(e)->pos+=3; }
        else     { b3(0x41,0x89,0x02); }
        break;
    /* reg-reg */
    case BB_INSN_MOV_ECX_EAX:
        if (txt) { emit3c_op(e,"mov","ecx, eax",NULL); CTX(e)->pos+=2; }
        else     { b2(0x89,0xC1); }
        break;
    case BB_INSN_MOV_RDI_RAX:
        if (txt) { emit3c_op(e,"mov","rdi, rax",NULL); CTX(e)->pos+=3; }
        else     { b3(0x48,0x89,0xC7); }
        break;
    case BB_INSN_MOV_RDX_RAX:
        if (txt) { emit3c_op(e,"mov","rdx, rax",NULL); CTX(e)->pos+=3; }
        else     { b3(0x48,0x89,0xC2); }
        break;
    case BB_INSN_CMP_EAX_ECX:
        if (txt) { emit3c_op(e,"cmp","eax, ecx",NULL); CTX(e)->pos+=2; }
        else     { b2(0x39,0xC8); }
        break;
    case BB_INSN_TEST_EAX_EAX:
        if (txt) { emit3c_op(e,"test","eax, eax",NULL); CTX(e)->pos+=2; }
        else     { b2(0x85,0xC0); }
        break;
    case BB_INSN_TEST_RAX_RAX:
        if (txt) { emit3c_op(e,"test","rax, rax",NULL); CTX(e)->pos+=3; }
        else     { b3(0x48,0x85,0xC0); }
        break;
    case BB_INSN_XOR_EDX_EDX:
        if (txt) { emit3c_op(e,"xor","edx, edx",NULL); CTX(e)->pos+=2; }
        else     { b2(0x31,0xD2); }
        break;
    case BB_INSN_MOVSXD_RCX_R10MEM:
        if (txt) { emit3c_op(e,"movsxd","rcx, dword ptr [r10]",NULL); CTX(e)->pos+=3; }
        else     { b3(0x49,0x63,0x0A); }
        break;
    case BB_INSN_LEA_RAX_RAXRCX:
        if (txt) { emit3c_op(e,"lea","rax, [rax+rcx]",NULL); CTX(e)->pos+=4; }
        else     { b4(0x48,0x8D,0x04,0x08); }
        break;
    /* control */
    case BB_INSN_RET:
        if (txt) { emit3c_op(e,"ret",NULL); CTX(e)->pos+=1; }
        else     { b1(0xC3); }
        break;
    case BB_INSN_CALL_RAX:
        if (txt) { emit3c_op(e,"call","rax",NULL); CTX(e)->pos+=2; }
        else     { b2(0xFF,0xD0); }
        break;
    /* SM-State field: inc dword [r13 + disp8] */
    case BB_INSN_INC_MEM_R13_DISP8:
        if (txt) { emit3c_op(e,"inc","dword ptr [r13 + %u]",(unsigned)a2); CTX(e)->pos+=4; }
        else     { b3(0x41,0xFF,0x45); b1(a2); }
        break;
    /* symbolic — TEXT: RIP-relative; BINARY: movabs imm64 (in-process JIT) */
    case BB_INSN_LEA_RCX_SYM:
        if (txt) { emit3c_op(e,"lea","rcx, [rip + %s]",d->sym?d->sym:"??"); CTX(e)->pos+=7; }
        else     { b2(0x48,0xB9); imm64(a0); }
        break;
    case BB_INSN_CALL_SYM_PLT:
        if (txt) { emit3c_op(e,"call","%s@PLT",d->sym?d->sym:"??"); CTX(e)->pos+=5; }
        else     { b2(0x48,0xB8); imm64(a0); b2(0xFF,0xD0); }
        break;
    case BB_INSN_LEA_R10_SYM:
        if (txt) { emit3c_op(e,"lea","r10, [rip + %s]",d->sym?d->sym:"??"); CTX(e)->pos+=7; }
        else     { b2(0x49,0xBA); imm64(a0); }
        break;
    /* push/pop r10 — preserve flat-BB LOCAL across PLT call */
    case BB_INSN_PUSH_R10:
        if (txt) { emit3c_op(e,"push","r10",NULL); CTX(e)->pos+=2; }
        else     { b2(0x41,0x52); }
        break;
    case BB_INSN_POP_R10:
        if (txt) { emit3c_op(e,"pop","r10",NULL); CTX(e)->pos+=2; }
        else     { b2(0x41,0x5A); }
        break;
    case BB_INSN_POP_RBP:
        if (txt) { emit3c_op(e,"pop","rbp",NULL); CTX(e)->pos+=1; }
        else     { b1(0x5D); }
        break;
    }
}

/* ── label_define ─────────────────────────────────────────────────────────── */

static void unified_label_define(emitter_t *e, bb_label_t *lbl)
{
    if (e->is_text) {
        char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", lbl->name);
        bb3c_format(outf(e), lbuf, "", "");
    } else {
        bb_emit_mode_t s = bb_emit_mode; bb_emit_mode = EMIT_BINARY_WIRED;
        bb_label_define(lbl);
        bb_emit_mode = s;
    }
}

/* ── emit_jmp ─────────────────────────────────────────────────────────────── */

static void unified_emit_jmp(emitter_t *e, bb_label_t *target, jmp_kind_t kind)
{
    static const char *mn[] = {"jmp","je","jne","jl","jge","jg"};
    int k = (int)kind < 6 ? (int)kind : 0;
    if (e->is_text) {
        emit3c_jmp(e, mn[k], target->name);
        CTX(e)->pos += 6;
    } else {
        static const uint8_t ops[6][2] = {
            {0xE9,0x00},{0x0F,0x84},{0x0F,0x85},{0x0F,0x8C},{0x0F,0x8D},{0x0F,0x8F}
        };
        if (k == 0) { b1(0xE9); }
        else        { b2(ops[k][0], ops[k][1]); }
        bb_emit_patch_rel32(target);
    }
}

/* ── global_sym ───────────────────────────────────────────────────────────── */

static void unified_global_sym(emitter_t *e, const char *name)
{
    if (e->is_text) bb3c_format(outf(e), "", ".global", name ? name : "");
    /* binary: no-op */
}

/* ── fprintf_raw ──────────────────────────────────────────────────────────── */

static void unified_fprintf_raw(emitter_t *e, const char *fmt, ...)
{
    if (!e->is_text) return;
    bb3c_flush_pending_cjmp_only();
    va_list ap; va_start(ap, fmt); vfprintf(outf(e), fmt, ap); va_end(ap);
}

/* ── pos ──────────────────────────────────────────────────────────────────── */

static int unified_pos(emitter_t *e)
{ return e->is_text ? CTX(e)->pos : bb_emit_pos; }

/* ── structural markers ───────────────────────────────────────────────────── */

static void unified_label_name(emitter_t *e, const char *name)
{
    if (!e->is_text) return;
    char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", name ? name : "");
    bb3c_format(outf(e), lbuf, "", "");
}

static void unified_pc_label(emitter_t *e, int pc)
{
    if (!e->is_text) return;
    char lbuf[64]; snprintf(lbuf, sizeof(lbuf), ".L%d:", pc);
    bb3c_format(outf(e), lbuf, "", "");
}

static void unified_section(emitter_t *e, const char *name)
{
    if (!e->is_text || !name) return;
    bb3c_flush_pending_cjmp_only();
    if (name[0]=='.' && (strcmp(name,".text")==0 || strcmp(name,".data")==0 ||
                          strcmp(name,".rodata")==0 || strcmp(name,".bss")==0))
        fprintf(outf(e), "%s\n", name);
    else
        fprintf(outf(e), ".section %s\n", name);
}

static void unified_directive(emitter_t *e, const char *line)
{
    if (!e->is_text || !line) return;
    bb3c_flush_pending_cjmp_only();
    fprintf(outf(e), "    %s\n", line);
}

/* ── data emission ────────────────────────────────────────────────────────── */

static void unified_data_quad(emitter_t *e, uint64_t val)
{
    if (e->is_text) {
        char buf[40]; snprintf(buf, sizeof(buf), "0x%llx", (unsigned long long)val);
        bb3c_format(outf(e), "", ".quad", buf);
    } else {
        bb_emit_u64(val);
    }
}

static void unified_data_quad_sym(emitter_t *e, const char *sym)
{
    if (e->is_text) bb3c_format(outf(e), "", ".quad", sym ? sym : "0");
    /* binary: in-process JIT has no linker; no-op until mode-4 needs it */
}

static void unified_data_string(emitter_t *e, const char *bytes, size_t len)
{
    if (!bytes) return;
    if (e->is_text) {
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
            default: if (c>=0x20 && c<0x7f) fputc(c,f); else fprintf(f,"\\x%02x",c); break;
            }
        }
        fputs("\"\n", f);
    } else {
        for (size_t i = 0; i < len; i++) bb_emit_byte((uint8_t)bytes[i]);
    }
}

static void unified_data_long(emitter_t *e, int32_t val)
{
    if (e->is_text) {
        char buf[24]; snprintf(buf, sizeof(buf), "%d", (int)val);
        bb3c_format(outf(e), "", ".long", buf);
    } else {
        uint32_t u = (uint32_t)val;
        bb_emit_byte((uint8_t)(u));      bb_emit_byte((uint8_t)(u>>8));
        bb_emit_byte((uint8_t)(u>>16));  bb_emit_byte((uint8_t)(u>>24));
    }
}

/* ── BB-specific compound emissions ──────────────────────────────────────── */

static void unified_bb_zeta_rdi(emitter_t *e, uint64_t ptr, const char *sym)
{
    if (e->is_text) {
        char arg[128]; snprintf(arg, sizeof(arg), "rdi, [rip + %s]", sym ? sym : "0");
        bb3c_format(outf(e), "", "lea", arg);
    } else {
        emit_mov_rdi_imm64(e, ptr);
    }
}

static void unified_bb_dispatch_jne_jmp(emitter_t *e,
                                        bb_label_t *lbl_succ, bb_label_t *lbl_fail)
{
    if (e->is_text) {
        char buf[256]; int o = 0;
        o += snprintf(buf+o, sizeof(buf)-o, "rax, rax;");
        while (o < 27 && o < (int)sizeof(buf)-1) buf[o++] = ' '; buf[o] = '\0';
        snprintf(buf+o, sizeof(buf)-o, "jne %s; jmp %s",
                 lbl_succ ? lbl_succ->name : "?", lbl_fail ? lbl_fail->name : "?");
        bb3c_format(outf(e), "", "test", buf);
    } else {
        emit_test_rax_rax(e);
        EV_JMP(e, lbl_succ, JMP_JNE);
        EV_JMP(e, lbl_fail, JMP_JMP);
    }
}

/* ── BB port labels / box banner ──────────────────────────────────────────── */

static const char *greek_for_port(char port)
{
    switch (port) { case 'a': return "α"; case 'b': return "β";
                    case 'g': return "γ"; case 'o': return "ω"; default: return "?"; }
}

static void unified_bb_port_label(emitter_t *e, const char *pfx, char port)
{
    if (!e->is_text) return;
    char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s_%s:", pfx?pfx:"", greek_for_port(port));
    bb3c_format(outf(e), lbuf, "", "");
}

static void unified_bb_port_jmp(emitter_t *e, const char *pfx, char port)
{
    if (!e->is_text) return;
    char tbuf[256]; snprintf(tbuf, sizeof(tbuf), "%s_%s", pfx?pfx:"", greek_for_port(port));
    bb3c_emit_jmp(outf(e), "jmp", tbuf);
    CTX(e)->pos += 5;
}

static void unified_bb_box_banner(emitter_t *e, const char *kind, const char *args)
{
    if (!e->is_text) return;
    bb3c_flush_pending_cjmp_only();
    FILE *f = outf(e);
    fputs("#", f); for (int i=1;i<120;i++) fputc('-',f); fputc('\n',f);
    fprintf(f, "    # BOX %s(%s)\n", kind?kind:"?", args?args:"");
}

/* ── formatting / readability (text-only) ─────────────────────────────────── */

static void unified_comment(emitter_t *e, const char *text)
{ if (!e->is_text) return; bb3c_flush_pending_cjmp_only(); fprintf(outf(e),"    # %s\n",text?text:""); }

static void unified_banner(emitter_t *e, const char *text)
{
    if (!e->is_text) return;
    bb3c_flush_pending_cjmp_only(); FILE *f = outf(e);
    fputs("#",f); for(int i=1;i<120;i++) fputc('=',f); fputc('\n',f);
    fprintf(f,"    # %s\n", text?text:"");
    fputs("#",f); for(int i=1;i<120;i++) fputc('=',f); fputc('\n',f);
}

static void unified_minor_break(emitter_t *e, const char *text)
{
    if (!e->is_text) return;
    bb3c_flush_pending_cjmp_only(); FILE *f = outf(e);
    fputs("#",f); for(int i=1;i<120;i++) fputc('-',f); fputc('\n',f);
    if (text && *text) fprintf(f,"    # %s\n",text);
}

static void unified_blank_line(emitter_t *e)
{ if (!e->is_text) return; bb3c_flush_pending_cjmp_only(); fputc('\n',outf(e)); }

/* ── macro hooks ──────────────────────────────────────────────────────────── */

static int g_text_macro_suppress = 0;

static void unified_macro_begin(emitter_t *e, const char *name,
                                const char *const *params, int nparams)
{
    if (!e->is_text) return;
    bb3c_flush_pending_cjmp_only(); FILE *f = outf(e);
    if (e->text_mode == TEXT_MODE_DEFINITION) {
        fprintf(f, ".macro %s", name?name:"?");
        for (int i=0; i<nparams; i++) fprintf(f," %s", params[i]?params[i]:"?");
        fputc('\n',f);
    } else {
        fprintf(f,"    %s", name?name:"?");
        for (int i=0; i<nparams; i++) fprintf(f,"%s%s",(i==0?" ":", "),params[i]?params[i]:"?");
        fputc('\n',f);
        g_text_macro_suppress = 1;
    }
}

static void unified_macro_param_ref(emitter_t *e, const char *name)
{ if (e->is_text && e->text_mode==TEXT_MODE_DEFINITION) fprintf(outf(e),"\\%s",name?name:"?"); }

static void unified_macro_end(emitter_t *e)
{
    if (!e->is_text) return;
    if (e->text_mode==TEXT_MODE_DEFINITION) { bb3c_flush_pending_cjmp_only(); fputs(".endm\n",outf(e)); }
    else g_text_macro_suppress = 0;
}

static void unified_pad_to_blob_size(emitter_t *e) { (void)e; }

/* ── vtable template ──────────────────────────────────────────────────────── */

static const emitter_t emitter_tmpl = {
    .emit_insn           = unified_emit_insn,
    .label_define        = unified_label_define,
    .emit_jmp            = unified_emit_jmp,
    .global_sym          = unified_global_sym,
    .fprintf_raw         = unified_fprintf_raw,
    .pos                 = unified_pos,
    .intern_str          = NULL,
    .label_name          = unified_label_name,
    .pc_label            = unified_pc_label,
    .section             = unified_section,
    .directive           = unified_directive,
    .data_quad           = unified_data_quad,
    .data_quad_sym       = unified_data_quad_sym,
    .data_string         = unified_data_string,
    .data_long           = unified_data_long,
    .bb_zeta_rdi         = unified_bb_zeta_rdi,
    .bb_dispatch_jne_jmp = unified_bb_dispatch_jne_jmp,
    .pad_to_blob_size    = unified_pad_to_blob_size,
    .bb_port_label       = unified_bb_port_label,
    .bb_port_jmp         = unified_bb_port_jmp,
    .bb_box_banner       = unified_bb_box_banner,
    .comment             = unified_comment,
    .banner              = unified_banner,
    .minor_break         = unified_minor_break,
    .blank_line          = unified_blank_line,
    .macro_begin         = unified_macro_begin,
    .macro_param_ref     = unified_macro_param_ref,
    .macro_end           = unified_macro_end,
    .text_mode           = TEXT_MODE_INVOCATION,
    .is_text             = 0,
    .ctx                 = NULL,
};

/* ── constructors ─────────────────────────────────────────────────────────── */

emitter_t *emitter_binary_new(bb_buf_t buf, int size)
{
    emitter_t *e = malloc(sizeof(emitter_t));
    if (!e) return NULL;
    *e = emitter_tmpl;
    e->is_text = 0; e->ctx = NULL;
    bb_emit_mode = EMIT_BINARY_WIRED;
    bb_emit_begin(buf, size);
    return e;
}

emitter_t *emitter_text_new(FILE *out)
{ return emitter_text_new_mode(out, TEXT_MODE_INVOCATION); }

emitter_t *emitter_text_new_mode(FILE *out, emitter_text_mode_t mode)
{
    emitter_t *e = malloc(sizeof(emitter_t));
    if (!e) return NULL;
    *e = emitter_tmpl;
    e->is_text = 1; e->text_mode = mode;
    text_ctx_t *ctx = calloc(1, sizeof(text_ctx_t));
    if (!ctx) { free(e); return NULL; }
    ctx->out = out; ctx->pos = 0;
    e->ctx = ctx;
    return e;
}

void emitter_free(emitter_t *e) { if (!e) return; free(e->ctx); free(e); }

FILE *emitter_text_file(emitter_t *e)
{ return (e && e->is_text) ? outf(e) : NULL; }

int emitter_end(emitter_t *e)
{
    if (!e) return 0;
    return e->is_text ? CTX(e)->pos : bb_emit_end();
}
