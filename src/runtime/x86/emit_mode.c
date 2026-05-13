/* emit_mode.c — L2: emit mode globals, macro begin/end, format-port helpers.
 *
 * Owns the global emit-mode state and the functions that inspect or change it.
 * Template functions (L4) and compound BB helpers (L3) read bb_emit_mode to
 * select TEXT vs BINARY vs MACRO_DEF output paths.
 */

#include "emit_mode.h"
#include "emit_defs.h"
#include "emit_buf.h"
#include "emit_form.h"
#include "emit_text3c.h"
#include "emit_label.h"
#include "insn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

/*========================================================================*/
/* Globals                                                                */
/*========================================================================*/

bb_emit_mode_t  bb_emit_mode = EMIT_TEXT;
FILE           *bb_emit_out  = NULL;

int g_bb_emit_format  = 0;
int g_in_text_macro_body = 0;

/*========================================================================*/
/* Mode lifecycle                                                         */
/*========================================================================*/

void emit_mode_set(bb_emit_mode_t m, FILE *out)
{
    bb_emit_mode = m;
    bb_emit_out  = out;
}

FILE *emit_outf(void) { return bb_emit_out ? bb_emit_out : stdout; }

/*========================================================================*/
/* Format-port helpers                                                   */
/*========================================================================*/

int emit_bb_is_format_mode(void) {
    return g_bb_emit_format &&
           (bb_emit_mode == EMIT_TEXT || bb_emit_mode == EMIT_TEXT_INLINE);
}

static char g_fmt_label[BB_LABEL_NAME_MAX + 4];
static char g_fmt_body[512];

static void fmt_label_save(bb_label_t *lbl) {
    if (lbl && lbl->name[0]) snprintf(g_fmt_label, sizeof(g_fmt_label), "%s:", lbl->name);
    else g_fmt_label[0] = '\0';
}

void fmt_body_append(const char *instr, const char *operands) {
    char frag[128];
    if (operands && operands[0]) snprintf(frag, sizeof(frag), "%s %s", instr, operands);
    else                          snprintf(frag, sizeof(frag), "%s", instr);
    if (g_fmt_body[0]) { strncat(g_fmt_body, " ; ", sizeof(g_fmt_body) - strlen(g_fmt_body) - 1); }
    strncat(g_fmt_body, frag, sizeof(g_fmt_body) - strlen(g_fmt_body) - 1);
}

static void fmt_flush_jmp(const char *mn, bb_label_t *target) {
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    char jmp_part[BB_LABEL_NAME_MAX + 16];
    snprintf(jmp_part, sizeof(jmp_part), "%s %s", mn, target->name);
    char col3[640];
    if (g_fmt_body[0]) snprintf(col3, sizeof(col3), "%s ; %s", g_fmt_body, jmp_part);
    else                snprintf(col3, sizeof(col3), "%s", jmp_part);
    bb3c_format(f, g_fmt_label, "", col3);
    g_fmt_label[0] = '\0';
    g_fmt_body[0]  = '\0';
}

/*------------------------------------------------------------------------*/
void emit_bb_format_port(bb_label_t *lbl_entry, const char *macro_name, const char *args)
{
    if (!emit_bb_is_format_mode()) return;
    char lbl_str[BB_LABEL_NAME_MAX + 2] = "";
    if (lbl_entry && lbl_entry->name[0]) {
        snprintf(lbl_str, sizeof(lbl_str), "%s:", lbl_entry->name);
    }
    bb3c_format(bb_emit_out ? bb_emit_out : stdout,
                lbl_str,
                macro_name ? macro_name : "",
                args ? args : "");
}

/*========================================================================*/
/* Padding (no-op for current backends)                                  */
/*========================================================================*/

void emit_pad_to_blob_size(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        return;
    }
}

/*========================================================================*/
/* Macro begin / end                                                      */
/*========================================================================*/

void emit_macro_begin(const char *name, const char *const *params, int nparams)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        return;
    case EMIT_TEXT_INLINE: return;
    case EMIT_TEXT: {
        bb3c_flush_pending_cjmp_only();
        FILE *f = emit_outf();
        fprintf(f, "    %s", name ? name : "?");
        for (int i = 0; i < nparams; i++) {
            fprintf(f, "%s%s", (i == 0 ? " " : ", "),
                    params && params[i] ? params[i] : "?");
        }
        fputc('\n', f);
        g_in_text_macro_body = 1;
        return;
    }
    case EMIT_MACRO_DEF: {
        bb3c_flush_pending_cjmp_only();
        FILE *f = emit_outf();
        fprintf(f, ".macro %s", name ? name : "?");
        for (int i = 0; i < nparams; i++) {
            fprintf(f, "%s%s", (i == 0 ? " " : ", "),
                    params && params[i] ? params[i] : "?");
        }
        fputc('\n', f);
        g_in_text_macro_body = 1;
        return;
    }
    }
}

void emit_macro_end(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        return;
    case EMIT_TEXT_INLINE: return;
    case EMIT_TEXT:
        g_in_text_macro_body = 0;
        return;
    case EMIT_MACRO_DEF:
        bb3c_flush_pending_cjmp_only();
        fputs(".endm\n", emit_outf());
        g_in_text_macro_body = 0;
        return;
    }
}

/*========================================================================*/
/* emit_jmp — needs fmt_flush_jmp (defined above); lives here L2        */
/*========================================================================*/

/* Forward declaration for emit_insn.h symbols used by emit_jmp */
#include "emit_insn.h"

void emit_jmp(bb_label_t *target, jmp_kind_t kind)
{
    static const char *const mn_tab[] = { "jmp", "je", "jne", "jl", "jge", "jg" };
    const char *mn = ((unsigned)kind < 6) ? mn_tab[kind] : "jmp";
    if (emit_bb_is_format_mode()) { fmt_flush_jmp(mn, target); return; }
    switch (bb_emit_mode) {
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_emit_jmp(emit_outf(), mn, target->name);
        return;
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        switch (kind) {
        case JMP_JMP: insn_jmp_r32(target);  return;
        case JMP_JE:  insn_je_r32(target);   return;
        case JMP_JNE: insn_jne_r32(target);  return;
        case JMP_JL:  insn_jl_r32(target);   return;
        case JMP_JGE: insn_jge_r32(target);  return;
        case JMP_JG:  insn_jg_r32(target);   return;
        }
        return;
    }
}

/* emit_label_define lives here (was in emit_bb_gen.c, uses fmt_label_save) */
#include "emit_label.h"
void emit_label_define(bb_label_t *lbl)
{
    if (emit_bb_is_format_mode()) { fmt_label_save(lbl); return; }
    bb_label_define(lbl);
}

/*========================================================================*/
/* TEXT-mode convenience wrappers                                         */
/*========================================================================*/

void bb3c_op(const char *mn, const char *fmt, ...)
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

void bb3c_jmp(const char *mn, const char *target)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    bb3c_emit_jmp(f, mn ? mn : "", target ? target : "");
}

/*========================================================================*/
/* emit_buf.c absorbed: raw binary buffer globals + write primitives      */
/* (RW-6: emit_buf.c deleted; content lives here)                         */
/*========================================================================*/

#include <string.h>

bb_buf_t   bb_emit_buf   = NULL;
int        bb_emit_pos   = 0;
int        bb_emit_size  = 0;

bb_patch_t bb_patch_list[BB_PATCH_MAX];
int        bb_patch_count = 0;

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_begin(bb_buf_t buf, int size)
{
    bb_emit_buf    = buf;
    bb_emit_pos    = 0;
    bb_emit_size   = size;
    bb_patch_count = 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_patch_rel8(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        fprintf(stderr,
                "bb_emit_patch_rel8: TEXT-mode reach (target='%s') — "
                "use bb_insn_*_rel8 mnemonic helpers\n",
                lbl->name);
        abort();
    }
    if (bb_label_defined(lbl)) {
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
    if (bb_patch_count >= BB_PATCH_MAX) {
        fprintf(stderr, "bb_emit_patch_rel8: patch list full\n");
        abort();
    }
    bb_patch_list[bb_patch_count].site  = bb_emit_pos;
    bb_patch_list[bb_patch_count].label = lbl;
    bb_patch_list[bb_patch_count].kind  = PATCH_REL8;
    bb_patch_count++;
    bb_emit_byte(0x00);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_patch_rel32(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
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
    bb_emit_u32(0x00000000);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_byte(uint8_t b)
{
    if (bb_emit_mode != EMIT_BINARY_WIRED) {
        fprintf(stderr,
                "bb_emit_byte: non-BINARY-mode reach (mode=%d, b=0x%02x) — "
                "convert caller to a named bb_insn_* helper\n",
                (int)bb_emit_mode, (unsigned)b);
        abort();
    }
    if (bb_emit_pos >= bb_emit_size) {
        fprintf(stderr, "bb_emit_byte: buffer overflow at pos=%d size=%d\n",
                bb_emit_pos, bb_emit_size);
        abort();
    }
    bb_emit_buf[bb_emit_pos++] = b;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_u16(uint16_t v) { bb_emit_byte((uint8_t)(v)); bb_emit_byte((uint8_t)(v >> 8)); }
void bb_emit_u32(uint32_t v) { bb_emit_byte((uint8_t)(v)); bb_emit_byte((uint8_t)(v>>8)); bb_emit_byte((uint8_t)(v>>16)); bb_emit_byte((uint8_t)(v>>24)); }
void bb_emit_u64(uint64_t v) { bb_emit_u32((uint32_t)(v)); bb_emit_u32((uint32_t)(v >> 32)); }
void bb_emit_i8 (int8_t  v) { bb_emit_byte((uint8_t)v); }
void bb_emit_i32(int32_t v) { uint32_t u; memcpy(&u, &v, 4); bb_emit_u32(u); }

/*========================================================================*/
/* emit_form.c absorbed: text/binary form helpers, emitter init/end,      */
/* emit_sym/load/label helpers, section/data emitters                     */
/* (RW-6: emit_form.c deleted; content lives here)                        */
/*========================================================================*/

int  g_is_text        = 0;
int  g_emit_text_mode = TEXT_MODE_INVOCATION;
int  g_emit_pos       = 0;

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emitter_init_binary(bb_buf_t buf, int size)
{
    g_is_text = 0; g_emit_text_mode = TEXT_MODE_INVOCATION; g_emit_pos = 0;
    bb_emit_mode = EMIT_BINARY_WIRED;
    bb_emit_begin(buf, size);
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emitter_init_text(FILE *out, int mode)
{
    g_is_text = 1; g_emit_text_mode = mode; g_emit_pos = 0;
    bb_emit_mode = EMIT_TEXT;
    bb_emit_out  = out ? out : stdout;
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
int  emitter_end(void)        { return g_is_text ? g_emit_pos : bb_emit_end(); }
FILE *emitter_text_out(void)  { return g_is_text ? bb_emit_out : NULL; }
int   emitter_pos(void)       { return g_is_text ? g_emit_pos  : bb_emit_pos; }
void  emitter_init_macro_def(FILE *out) { emitter_init_text(out, TEXT_MODE_DEFINITION); }

static void ef_b1(uint8_t a)                                  { bb_emit_byte(a); }
static void ef_b2(uint8_t a, uint8_t b)                       { bb_emit_byte(a); bb_emit_byte(b); }
static void ef_b3(uint8_t a, uint8_t b, uint8_t c)            { bb_emit_byte(a); bb_emit_byte(b); bb_emit_byte(c); }
static void ef_b4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { bb_emit_byte(a); bb_emit_byte(b); bb_emit_byte(c); bb_emit_byte(d); }
static void ef_u32(uint32_t v) { bb_emit_u32(v); }
static void ef_u64(uint64_t v) { bb_emit_u64(v); }

static void ef_t3c(const char *mnem, const char *fmt, ...)
{
    char buf[256]; buf[0] = '\0';
    if (fmt) { va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap); }
    bb3c_format(bb_emit_out, "", mnem ? mnem : "", buf);
}
static void ef_t3c_jmp(const char *mnem, const char *target)
{ bb3c_emit_jmp(bb_emit_out, mnem ? mnem : "", target ? target : ""); }

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_reg64_imm64(uint8_t prefix, uint8_t reg, uint64_t val, const char *mnem)
{
    if (g_is_text) { ef_t3c("mov", "%s, 0x%llx", mnem, (unsigned long long)val); g_emit_pos += 10; }
    else           { ef_b2(prefix, reg); ef_u64(val); }
}
void emit_form_reg32_imm32(uint8_t op, uint32_t val, const char *mnem)
{
    if (g_is_text) { ef_t3c("mov", "%s, %u", mnem, val); g_emit_pos += 5; }
    else           { ef_b1(op); ef_u32(val); }
}
void emit_form_alu_eax_imm32(uint8_t op, uint32_t val, const char *mnem)
{
    if (g_is_text) { ef_t3c(mnem, "eax, %u", val); g_emit_pos += 5; }
    else           { ef_b1(op); ef_u32(val); }
}
void emit_form_alu_esi_imm8(uint8_t modrm, uint8_t val, const char *mnem)
{
    if (g_is_text) { ef_t3c(mnem, "esi, %u", (unsigned)val); g_emit_pos += 3; }
    else           { ef_b3(0x83, modrm, val); }
}
void emit_form_reg_reg2(uint8_t b0, uint8_t b1_, const char *text)
{
    if (g_is_text) { ef_t3c(NULL, "%s", text); g_emit_pos += 2; }
    else           { ef_b2(b0, b1_); }
}
void emit_form_reg_reg3(uint8_t b0, uint8_t b1_, uint8_t b2_, const char *text)
{
    if (g_is_text) { ef_t3c(NULL, "%s", text); g_emit_pos += 3; }
    else           { ef_b3(b0, b1_, b2_); }
}
void emit_form_mem2(uint8_t b0, uint8_t b1_, const char *text)
{
    if (g_is_text) { ef_t3c(NULL, "%s", text); g_emit_pos += 2; }
    else           { ef_b2(b0, b1_); }
}
void emit_form_mem3(uint8_t b0, uint8_t b1_, uint8_t b2_, const char *text)
{
    if (g_is_text) { ef_t3c(NULL, "%s", text); g_emit_pos += 3; }
    else           { ef_b3(b0, b1_, b2_); }
}
void emit_form_mem4(uint8_t b0, uint8_t b1_, uint8_t b2_, uint8_t b3_, const char *text)
{
    if (g_is_text) { ef_t3c(NULL, "%s", text); g_emit_pos += 4; }
    else           { ef_b4(b0, b1_, b2_, b3_); }
}
void emit_form_r13_disp8(uint8_t b0, uint8_t b1_, uint8_t b2_, uint8_t disp, const char *text_fmt)
{
    if (g_is_text) { ef_t3c(NULL, text_fmt, (unsigned)disp); g_emit_pos += 4; }
    else           { ef_b3(b0, b1_, b2_); ef_b1(disp); }
}
void emit_form_nullary1(uint8_t b0, const char *text)
{
    if (g_is_text) { ef_t3c(text, NULL); g_emit_pos += 1; }
    else           { ef_b1(b0); }
}
void emit_form_nullary2(uint8_t b0, uint8_t b1_, const char *text)
{
    if (g_is_text) { ef_t3c(text, NULL); g_emit_pos += 2; }
    else           { ef_b2(b0, b1_); }
}
void emit_form_nullary3(uint8_t b0, uint8_t b1_, uint8_t b2_, const char *text)
{
    if (g_is_text) { ef_t3c(text, NULL); g_emit_pos += 3; }
    else           { ef_b3(b0, b1_, b2_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_sym_lea_rcx(const char *sym, uint64_t addr)
{
    if (g_is_text) { ef_t3c("lea", "rcx, [rip + %s]", sym ? sym : "??"); g_emit_pos += 7; }
    else           { ef_b2(0x48,0xB9); ef_u64(addr); }
}
void emit_sym_lea_r10(const char *sym, uint64_t addr)
{
    if (g_is_text) { ef_t3c("lea", "r10, [rip + %s]", sym ? sym : "??"); g_emit_pos += 7; }
    else           { ef_b2(0x49,0xBA); ef_u64(addr); }
}
void emit_load_r10_delta_ptr(uint64_t addr)  { emit_sym_lea_r10("\xCE\x94", addr); }
void emit_load_delta(void)                    { emit_mov_eax_r10mem(); }
void emit_store_delta(void)                   { emit_mov_r10mem_eax(); }
void emit_load_sigma(uint64_t a)             { emit_sym_lea_rcx("\xCE\xA3", a);        emit_mov_rax_rcxmem(); }
void emit_load_siglen(uint64_t a)            { emit_sym_lea_rcx("\xCE\xA3""len", a);   emit_mov_eax_rcxmem(); }
void emit_sigma_plus_delta(uint64_t a)       { emit_load_sigma(a); emit_movsxd_rcx_r10mem(); emit_lea_rax_raxrcx(); }
void emit_cmp_eax_siglen(uint64_t a)         { emit_sym_lea_rcx("\xCE\xA3""len", a);   emit_cmp_eax_rcxmem(); }
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_label_define_bb(bb_label_t *lbl)
{
    if (g_is_text) {
        char buf[256]; snprintf(buf, sizeof(buf), "%s:", lbl->name);
        bb3c_format(bb_emit_out, buf, "", "");
    } else {
        bb_emit_mode_t s = bb_emit_mode; bb_emit_mode = EMIT_BINARY_WIRED;
        bb_label_define(lbl);
        bb_emit_mode = s;
    }
}
void emit_label_name(const char *name)
{
    if (!g_is_text) return;
    char buf[256]; snprintf(buf, sizeof(buf), "%s:", name ? name : "");
    bb3c_format(bb_emit_out, buf, "", "");
}
void emit_pc_label(int pc)
{
    if (!g_is_text) return;
    char buf[64]; snprintf(buf, sizeof(buf), ".L%d:", pc);
    bb3c_format(bb_emit_out, buf, "", "");
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_jmp_label(bb_label_t *target, jmp_kind_t kind)
{
    static const char    *mn[]    = {"jmp","je","jne","jl","jge","jg"};
    static const uint8_t  ops[6][2] = {{0xE9,0x00},{0x0F,0x84},{0x0F,0x85},{0x0F,0x8C},{0x0F,0x8D},{0x0F,0x8F}};
    int k = (int)kind < 6 ? (int)kind : 0;
    if (g_is_text) { ef_t3c_jmp(mn[k], target->name); g_emit_pos += 6; }
    else { if (k==0) ef_b1(0xE9); else ef_b2(ops[k][0], ops[k][1]); bb_emit_patch_rel32(target); }
}
void emit_section(const char *name)
{
    if (!g_is_text || !name) return;
    bb3c_flush_pending_cjmp_only();
    if (name[0]=='.' && (strcmp(name,".text")==0 || strcmp(name,".data")==0 ||
                          strcmp(name,".rodata")==0 || strcmp(name,".bss")==0))
        fprintf(bb_emit_out, "%s\n", name);
    else
        fprintf(bb_emit_out, ".section %s\n", name);
}
void emit_directive(const char *line)
{
    if (!g_is_text || !line) return;
    bb3c_flush_pending_cjmp_only();
    fprintf(bb_emit_out, "    %s\n", line);
}
void emit_global_sym(const char *name) { if (g_is_text) bb3c_format(bb_emit_out, "", ".global", name ? name : ""); }
void emit_banner(const char *text)
{
    if (!g_is_text) return;
    bb3c_flush_pending_cjmp_only();
    fputc('#', bb_emit_out); for (int i=1;i<120;i++) fputc('=',bb_emit_out); fputc('\n',bb_emit_out);
    fprintf(bb_emit_out, "    # %s\n", text ? text : "");
    fputc('#', bb_emit_out); for (int i=1;i<120;i++) fputc('=',bb_emit_out); fputc('\n',bb_emit_out);
}
void emit_minor_break(const char *text)
{
    if (!g_is_text) return;
    bb3c_flush_pending_cjmp_only();
    fputc('#', bb_emit_out); for (int i=1;i<120;i++) fputc('-',bb_emit_out); fputc('\n',bb_emit_out);
    if (text && *text) fprintf(bb_emit_out, "    # %s\n", text);
}
void emit_blank_line(void)
{
    if (!g_is_text) return;
    bb3c_flush_pending_cjmp_only();
    fputc('\n', bb_emit_out);
}
void emit_fprintf_raw(const char *fmt, ...)
{
    if (!g_is_text) return;
    bb3c_flush_pending_cjmp_only();
    va_list ap; va_start(ap, fmt); vfprintf(bb_emit_out, fmt, ap); va_end(ap);
}
void emit_data_quad(uint64_t val)
{
    if (g_is_text) {
        char buf[40]; snprintf(buf, sizeof(buf), "0x%llx", (unsigned long long)val);
        bb3c_format(bb_emit_out, "", ".quad", buf);
    } else { bb_emit_u64(val); }
}
void emit_data_quad_sym(const char *sym)
{
    if (g_is_text) bb3c_format(bb_emit_out, "", ".quad", sym ? sym : "0");
}
void emit_data_string(const char *bytes, size_t len)
{
    if (!bytes) return;
    if (g_is_text) {
        bb3c_flush_pending_cjmp_only();
        fputs("    .ascii \"", bb_emit_out);
        for (size_t i = 0; i < len; i++) {
            unsigned char c = (unsigned char)bytes[i];
            switch (c) {
            case '\\': fputs("\\\\", bb_emit_out); break;
            case '"' : fputs("\\\"", bb_emit_out); break;
            case '\n': fputs("\\n",  bb_emit_out); break;
            case '\t': fputs("\\t",  bb_emit_out); break;
            default: if (c>=0x20&&c<0x7f) fputc(c,bb_emit_out); else fprintf(bb_emit_out,"\\x%02x",c); break;
            }
        }
        fputs("\"\n", bb_emit_out);
    } else { for (size_t i=0;i<len;i++) bb_emit_byte((uint8_t)bytes[i]); }
}
void emit_data_long(int32_t val)
{
    if (g_is_text) {
        char buf[24]; snprintf(buf, sizeof(buf), "%d", (int)val);
        bb3c_format(bb_emit_out, "", ".long", buf);
    } else {
        uint32_t u = (uint32_t)val;
        bb_emit_byte((uint8_t)u); bb_emit_byte((uint8_t)(u>>8));
        bb_emit_byte((uint8_t)(u>>16)); bb_emit_byte((uint8_t)(u>>24));
    }
}
void emit_bb_zeta_rdi(uint64_t ptr, const char *sym)
{
    if (g_is_text) {
        char arg[128]; snprintf(arg, sizeof(arg), "rdi, [rip + %s]", sym ? sym : "0");
        bb3c_format(bb_emit_out, "", "lea", arg);
    } else { emit_mov_rdi_imm64(ptr); }
}
void emit_bb_dispatch_jne_jmp(bb_label_t *lbl_succ, bb_label_t *lbl_fail)
{
    if (g_is_text) {
        char buf[256]; int o = 0;
        o += snprintf(buf+o, sizeof(buf)-o, "rax, rax;");
        while (o < 27 && o < (int)sizeof(buf)-1) buf[o++] = ' '; buf[o] = '\0';
        snprintf(buf+o, sizeof(buf)-o, "jne %s; jmp %s",
                 lbl_succ ? lbl_succ->name : "?", lbl_fail ? lbl_fail->name : "?");
        bb3c_format(bb_emit_out, "", "test", buf);
    } else {
        emit_test_rax_rax();
        emit_jmp_label(lbl_succ, JMP_JNE);
        emit_jmp_label(lbl_fail, JMP_JMP);
    }
}
static const char *ef_greek_port(char port)
{
    switch (port) { case 'a': return "\xCE\xB1"; case 'b': return "\xCE\xB2";
                    case 'g': return "\xCE\xB3"; case 'o': return "\xCF\x89"; default: return "?"; }
}
void emit_bb_port_label(const char *pfx, char port)
{
    if (!g_is_text) return;
    char buf[256]; snprintf(buf, sizeof(buf), "%s_%s:", pfx ? pfx : "", ef_greek_port(port));
    bb3c_format(bb_emit_out, buf, "", "");
}
void emit_bb_port_jmp(const char *pfx, char port)
{
    if (!g_is_text) return;
    char tbuf[256]; snprintf(tbuf, sizeof(tbuf), "%s_%s", pfx ? pfx : "", ef_greek_port(port));
    bb3c_emit_jmp(bb_emit_out, "jmp", tbuf);
    g_emit_pos += 5;
}
void emit_macro_param_ref(const char *name)
{
    if (g_is_text && g_emit_text_mode == TEXT_MODE_DEFINITION)
        fprintf(bb_emit_out, "\\%s", name ? name : "?");
}
