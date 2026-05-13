#include "emit.h"
#include "emit_buf.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

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
int  emitter_end(void)   { return g_is_text ? g_emit_pos : bb_emit_end(); }
FILE *emitter_text_out(void) { return g_is_text ? bb_emit_out : NULL; }
int   emitter_pos     (void) { return g_is_text ? g_emit_pos  : bb_emit_pos; }

static void b1(uint8_t a)                                       { bb_emit_byte(a); }
static void b2(uint8_t a, uint8_t b)                            { bb_emit_byte(a); bb_emit_byte(b); }
static void b3(uint8_t a, uint8_t b, uint8_t c)                 { bb_emit_byte(a); bb_emit_byte(b); bb_emit_byte(c); }
static void b4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)      { bb_emit_byte(a); bb_emit_byte(b); bb_emit_byte(c); bb_emit_byte(d); }
static void u32(uint32_t v) { bb_emit_u32(v); }
static void u64(uint64_t v) { bb_emit_u64(v); }

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void t3c(const char *mnem, const char *fmt, ...)
{
    char buf[256]; buf[0] = '\0';
    if (fmt) { va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap); }
    bb3c_format(bb_emit_out, "", mnem ? mnem : "", buf);
}

static void t3c_jmp(const char *mnem, const char *target)
{ bb3c_emit_jmp(bb_emit_out, mnem ? mnem : "", target ? target : ""); }

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_reg64_imm64(uint8_t prefix, uint8_t reg, uint64_t val, const char *mnem)
{
    if (g_is_text) { t3c("mov", "%s, 0x%llx", mnem, (unsigned long long)val); g_emit_pos += 10; }
    else           { b2(prefix, reg); u64(val); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_reg32_imm32(uint8_t op, uint32_t val, const char *mnem)
{
    if (g_is_text) { t3c("mov", "%s, %u", mnem, val); g_emit_pos += 5; }
    else           { b1(op); u32(val); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_alu_eax_imm32(uint8_t op, uint32_t val, const char *mnem)
{
    if (g_is_text) { t3c(mnem, "eax, %u", val); g_emit_pos += 5; }
    else           { b1(op); u32(val); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_alu_esi_imm8(uint8_t modrm, uint8_t val, const char *mnem)
{
    if (g_is_text) { t3c(mnem, "esi, %u", (unsigned)val); g_emit_pos += 3; }
    else           { b3(0x83, modrm, val); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_reg_reg2(uint8_t b0, uint8_t b1_, const char *text)
{
    if (g_is_text) { t3c(NULL, "%s", text); g_emit_pos += 2; }
    else           { b2(b0, b1_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_reg_reg3(uint8_t b0, uint8_t b1_, uint8_t b2_, const char *text)
{
    if (g_is_text) { t3c(NULL, "%s", text); g_emit_pos += 3; }
    else           { b3(b0, b1_, b2_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_mem2(uint8_t b0, uint8_t b1_, const char *text)
{
    if (g_is_text) { t3c(NULL, "%s", text); g_emit_pos += 2; }
    else           { b2(b0, b1_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_mem3(uint8_t b0, uint8_t b1_, uint8_t b2_, const char *text)
{
    if (g_is_text) { t3c(NULL, "%s", text); g_emit_pos += 3; }
    else           { b3(b0, b1_, b2_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_mem4(uint8_t b0, uint8_t b1_, uint8_t b2_, uint8_t b3_, const char *text)
{
    if (g_is_text) { t3c(NULL, "%s", text); g_emit_pos += 4; }
    else           { b4(b0, b1_, b2_, b3_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_r13_disp8(uint8_t b0, uint8_t b1_, uint8_t b2_, uint8_t disp, const char *text_fmt)
{
    if (g_is_text) { t3c(NULL, text_fmt, (unsigned)disp); g_emit_pos += 4; }
    else           { b3(b0, b1_, b2_); b1(disp); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_nullary1(uint8_t b0, const char *text)
{
    if (g_is_text) { t3c(text, NULL); g_emit_pos += 1; }
    else           { b1(b0); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_nullary2(uint8_t b0, uint8_t b1_, const char *text)
{
    if (g_is_text) { t3c(text, NULL); g_emit_pos += 2; }
    else           { b2(b0, b1_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_nullary3(uint8_t b0, uint8_t b1_, uint8_t b2_, const char *text)
{
    if (g_is_text) { t3c(text, NULL); g_emit_pos += 3; }
    else           { b3(b0, b1_, b2_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_sym_lea_rcx(const char *sym, uint64_t addr)
{
    if (g_is_text) { t3c("lea", "rcx, [rip + %s]", sym ? sym : "??"); g_emit_pos += 7; }
    else           { b2(0x48,0xB9); u64(addr); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_sym_lea_r10(const char *sym, uint64_t addr)
{
    if (g_is_text) { t3c("lea", "r10, [rip + %s]", sym ? sym : "??"); g_emit_pos += 7; }
    else           { b2(0x49,0xBA); u64(addr); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_label_name(const char *name)
{
    if (!g_is_text) return;
    char buf[256]; snprintf(buf, sizeof(buf), "%s:", name ? name : "");
    bb3c_format(bb_emit_out, buf, "", "");
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
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
    if (g_is_text) { t3c_jmp(mn[k], target->name); g_emit_pos += 6; }
    else { if (k==0) b1(0xE9); else b2(ops[k][0], ops[k][1]); bb_emit_patch_rel32(target); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_directive(const char *line)
{
    if (!g_is_text || !line) return;
    bb3c_flush_pending_cjmp_only();
    fprintf(bb_emit_out, "    %s\n", line);
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_global_sym(const char *name) { if (g_is_text) bb3c_format(bb_emit_out, "", ".global", name ? name : ""); }
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_banner(const char *text)
{
    if (!g_is_text) return;
    bb3c_flush_pending_cjmp_only();
    fputc('#', bb_emit_out); for (int i=1;i<120;i++) fputc('=',bb_emit_out); fputc('\n',bb_emit_out);
    fprintf(bb_emit_out, "    # %s\n", text ? text : "");
    fputc('#', bb_emit_out); for (int i=1;i<120;i++) fputc('=',bb_emit_out); fputc('\n',bb_emit_out);
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_minor_break(const char *text)
{
    if (!g_is_text) return;
    bb3c_flush_pending_cjmp_only();
    fputc('#', bb_emit_out); for (int i=1;i<120;i++) fputc('-',bb_emit_out); fputc('\n',bb_emit_out);
    if (text && *text) fprintf(bb_emit_out, "    # %s\n", text);
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_blank_line(void)
{
    if (!g_is_text) return;
    bb3c_flush_pending_cjmp_only();
    fputc('\n', bb_emit_out);
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_fprintf_raw(const char *fmt, ...)
{
    if (!g_is_text) return;
    bb3c_flush_pending_cjmp_only();
    va_list ap; va_start(ap, fmt); vfprintf(bb_emit_out, fmt, ap); va_end(ap);
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_data_quad(uint64_t val)
{
    if (g_is_text) {
        char buf[40]; snprintf(buf, sizeof(buf), "0x%llx", (unsigned long long)val);
        bb3c_format(bb_emit_out, "", ".quad", buf);
    } else { bb_emit_u64(val); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_data_quad_sym(const char *sym)
{
    if (g_is_text) bb3c_format(bb_emit_out, "", ".quad", sym ? sym : "0");
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_zeta_rdi(uint64_t ptr, const char *sym)
{
    if (g_is_text) {
        char arg[128]; snprintf(arg, sizeof(arg), "rdi, [rip + %s]", sym ? sym : "0");
        bb3c_format(bb_emit_out, "", "lea", arg);
    } else { emit_mov_rdi_imm64(ptr); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static const char *greek_port(char port)
{
    switch (port) { case 'a': return "\xCE\xB1"; case 'b': return "\xCE\xB2";
                    case 'g': return "\xCE\xB3"; case 'o': return "\xCF\x89"; default: return "?"; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_port_label(const char *pfx, char port)
{
    if (!g_is_text) return;
    char buf[256]; snprintf(buf, sizeof(buf), "%s_%s:", pfx ? pfx : "", greek_port(port));
    bb3c_format(bb_emit_out, buf, "", "");
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_port_jmp(const char *pfx, char port)
{
    if (!g_is_text) return;
    char tbuf[256]; snprintf(tbuf, sizeof(tbuf), "%s_%s", pfx ? pfx : "", greek_port(port));
    bb3c_emit_jmp(bb_emit_out, "jmp", tbuf);
    g_emit_pos += 5;
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int g_macro_suppress = 0;

void emit_macro_param_ref(const char *name)
{
    if (g_is_text && g_emit_text_mode == TEXT_MODE_DEFINITION)
        fprintf(bb_emit_out, "\\%s", name ? name : "?");
}
