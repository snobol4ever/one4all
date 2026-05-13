
#include "emit_core.h"
#include "emit_sm_binary.h"
#include "emit_form.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

bb_emit_mode_t  bb_emit_mode = EMIT_TEXT;
FILE           *bb_emit_out  = NULL;

int g_bb_emit_format  = 0;
int g_in_text_macro_body = 0;

void emit_mode_set(bb_emit_mode_t m, FILE *out)
{
    bb_emit_mode = m;
    bb_emit_out  = out;
}

FILE *emit_outf(void) { return bb_emit_out ? bb_emit_out : stdout; }

/*--------------------------------------------------------------------------------------------------------------------*/
int emit_bb_is_format_mode(void) {
    return g_bb_emit_format &&
           (bb_emit_mode == EMIT_TEXT || bb_emit_mode == EMIT_TEXT_INLINE);
}

static char g_fmt_label[BB_LABEL_NAME_MAX + 4];
static char g_fmt_body[512];

/*--------------------------------------------------------------------------------------------------------------------*/
static void fmt_label_save(bb_label_t *lbl) {
    if (lbl && lbl->name[0]) snprintf(g_fmt_label, sizeof(g_fmt_label), "%s:", lbl->name);
    else g_fmt_label[0] = '\0';
}

/*--------------------------------------------------------------------------------------------------------------------*/
void fmt_body_append(const char *instr, const char *operands) {
    char frag[128];
    if (operands && operands[0]) snprintf(frag, sizeof(frag), "%s %s", instr, operands);
    else                          snprintf(frag, sizeof(frag), "%s", instr);
    if (g_fmt_body[0]) { strncat(g_fmt_body, " ; ", sizeof(g_fmt_body) - strlen(g_fmt_body) - 1); }
    strncat(g_fmt_body, frag, sizeof(g_fmt_body) - strlen(g_fmt_body) - 1);
}

/*--------------------------------------------------------------------------------------------------------------------*/
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

void emit_jmp(bb_label_t *target, jmp_kind_t kind)
{
    static const char *const mn_tab[] = { "jmp", "je", "jne", "jl", "jge", "jg" };
    const char *mn = ((unsigned)kind < 6) ? mn_tab[kind] : "jmp";
    if (emit_bb_is_format_mode())
        fmt_flush_jmp(mn, target);
    else {
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
        }
        return;
    }
}

void emit_label_define(bb_label_t *lbl)
{
    if (emit_bb_is_format_mode())
        fmt_label_save(lbl);
    else {
        bb_label_define(lbl);
    }
}

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

#include <string.h>

bb_buf_t   bb_emit_buf   = NULL;
int        bb_emit_pos   = 0;
int        bb_emit_size  = 0;

bb_patch_t bb_patch_list[BB_PATCH_MAX];
int        bb_patch_count = 0;

void bb_emit_begin(bb_buf_t buf, int size)
{
    bb_emit_buf    = buf;
    bb_emit_pos    = 0;
    bb_emit_size   = size;
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

void bb_emit_u16(uint16_t v) { bb_emit_byte((uint8_t)(v)); bb_emit_byte((uint8_t)(v >> 8)); }
void bb_emit_u32(uint32_t v) { bb_emit_byte((uint8_t)(v)); bb_emit_byte((uint8_t)(v>>8)); bb_emit_byte((uint8_t)(v>>16)); bb_emit_byte((uint8_t)(v>>24)); }
void bb_emit_u64(uint64_t v) { bb_emit_u32((uint32_t)(v)); bb_emit_u32((uint32_t)(v >> 32)); }
void bb_emit_i8 (int8_t  v) { bb_emit_byte((uint8_t)v); }
void bb_emit_i32(int32_t v) { uint32_t u; memcpy(&u, &v, 4); bb_emit_u32(u); }

int  g_is_text        = 0;
int  g_emit_text_mode = TEXT_MODE_INVOCATION;
int  g_emit_pos       = 0;

void emitter_init_binary(bb_buf_t buf, int size)
{
    g_is_text = 0; g_emit_text_mode = TEXT_MODE_INVOCATION; g_emit_pos = 0;
    bb_emit_mode = EMIT_BINARY_WIRED;
    bb_emit_begin(buf, size);
}

void emitter_init_text(FILE *out, int mode)
{
    g_is_text = 1; g_emit_text_mode = mode; g_emit_pos = 0;
    bb_emit_mode = EMIT_TEXT;
    bb_emit_out  = out ? out : stdout;
}

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
    lbl->offset = bb_emit_pos;
    for (int i = 0; i < bb_patch_count; i++) {
        bb_patch_t *p = &bb_patch_list[i];
        if (p->label != lbl) continue;
        int target = lbl->offset;
        if (p->kind == PATCH_REL8) {
            int disp = target - (p->site + 1);
            if (disp < -128 || disp > 127) {
                fprintf(stderr, "bb_label_define: rel8 overflow for '%s': disp=%d\n",
                        lbl->name, disp);
                abort();
            }
            bb_emit_buf[p->site] = (uint8_t)(int8_t)disp;
        } else {
            int disp = target - (p->site + 4);
            uint32_t u;
            memcpy(&u, &disp, 4);
            bb_emit_buf[p->site + 0] = (uint8_t)(u      );
            bb_emit_buf[p->site + 1] = (uint8_t)(u >>  8);
            bb_emit_buf[p->site + 2] = (uint8_t)(u >> 16);
            bb_emit_buf[p->site + 3] = (uint8_t)(u >> 24);
        }
        bb_patch_list[i] = bb_patch_list[--bb_patch_count];
        i--;
    }
}

void emit_label_init(bb_label_t *lbl, const char *name)  { bb_label_init(lbl, name); }
void emit_label_initf(bb_label_t *lbl, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vsnprintf(lbl->name, BB_LABEL_NAME_MAX, fmt, ap);
    va_end(ap);
    lbl->offset = BB_LABEL_UNRESOLVED;
}

static char  g_bb3c_pending_label[256] = "";
static FILE *g_bb3c_pending_out          = NULL;

static char  g_bb3c_pending_cjmp_mn[16]      = "";
static char  g_bb3c_pending_cjmp_target[256] = "";
static FILE *g_bb3c_pending_cjmp_out         = NULL;

#define BB_COL3_WIDTH 27

static int bb3c_visual_width(const char *s)
{
    int w = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if ((*p & 0xC0) != 0x80) w++;
    }
    return w;
}

static int bb3c_pad_to_width(char *buf, size_t bufsz, const char *s, int target)
{
    int sw = bb3c_visual_width(s);
    int slen = (int)strlen(s ? s : "");
    if (slen >= (int)bufsz) slen = (int)bufsz - 1;
    int o = 0;
    if (s && slen > 0) { memcpy(buf + o, s, (size_t)slen); o += slen; }
    int pad = target - sw;
    while (pad-- > 0 && o < (int)bufsz - 1) buf[o++] = ' ';
    if (o >= (int)bufsz) o = (int)bufsz - 1;
    buf[o] = '\0';
    return o;
}

static void bb3c_write_line(FILE *out, const char *L, const char *A, const char *G)
{
    char buf[768];
    int o = 0;
    o += bb3c_pad_to_width(buf + o, sizeof(buf) - o, L ? L : "", 24);
    o += bb3c_pad_to_width(buf + o, sizeof(buf) - o, A ? A : "", 16);
    if (o < (int)sizeof(buf) - 1) buf[o++] = ' ';
    if (G && *G) {
        int gl = (int)strlen(G);
        if (gl > (int)sizeof(buf) - 1 - o) gl = (int)sizeof(buf) - 1 - o;
        memcpy(buf + o, G, (size_t)gl);
        o += gl;
    }
    while (o > 0 && (buf[o-1] == ' ' || buf[o-1] == '\t')) o--;
    buf[o] = '\0';
    fputs(buf, out);
    fputc('\n', out);
}

static void bb3c_flush_pending_cond_jmp(void)
{
    if (g_bb3c_pending_cjmp_mn[0] && g_bb3c_pending_cjmp_out) {
        bb3c_write_line(g_bb3c_pending_cjmp_out, "",
                        g_bb3c_pending_cjmp_mn,
                        g_bb3c_pending_cjmp_target);
        g_bb3c_pending_cjmp_mn[0]     = '\0';
        g_bb3c_pending_cjmp_target[0] = '\0';
        g_bb3c_pending_cjmp_out       = NULL;
    }
}

static void bb3c_flush_pending_to(FILE *target)
{
    bb3c_flush_pending_cond_jmp();
    if (g_bb3c_pending_label[0] && g_bb3c_pending_out) {
        bb3c_write_line(g_bb3c_pending_out, g_bb3c_pending_label, "", "");
        g_bb3c_pending_label[0] = '\0';
        g_bb3c_pending_out = NULL;
    }
    (void)target;
}

void bb3c_flush_pending_cjmp_only(void) { bb3c_flush_pending_cond_jmp(); }
void bb3c_flush_pending(void)           { bb3c_flush_pending_to(NULL); }

static int bb3c_is_cond_jmp(const char *mn)
{
    if (!mn) return 0;
    return (strcmp(mn, "je")  == 0) || (strcmp(mn, "jne") == 0) ||
           (strcmp(mn, "jl")  == 0) || (strcmp(mn, "jge") == 0) ||
           (strcmp(mn, "jg")  == 0) || (strcmp(mn, "jle") == 0) ||
           (strcmp(mn, "jbe") == 0);
}

void bb3c_emit_jmp(FILE *out, const char *mn, const char *target)
{
    const char *m = mn ? mn : "";
    const char *t = target ? target : "";
    if (bb3c_is_cond_jmp(m)) {
        if (g_bb3c_pending_cjmp_mn[0] && g_bb3c_pending_cjmp_out != out)
            bb3c_flush_pending_cond_jmp();
        if (g_bb3c_pending_cjmp_mn[0])
            bb3c_flush_pending_cond_jmp();
        snprintf(g_bb3c_pending_cjmp_mn,     sizeof(g_bb3c_pending_cjmp_mn),     "%s", m);
        snprintf(g_bb3c_pending_cjmp_target, sizeof(g_bb3c_pending_cjmp_target), "%s", t);
        g_bb3c_pending_cjmp_out = out;
        return;
    }
    if (g_bb3c_pending_cjmp_mn[0] && g_bb3c_pending_cjmp_out == out) {
        char rest[512];
        char col3[288];
        int n = snprintf(col3, sizeof(col3), "%s;", g_bb3c_pending_cjmp_target);
        if (n < 0) n = 0;
        int o = bb3c_pad_to_width(rest, sizeof(rest), col3, BB_COL3_WIDTH);
        snprintf(rest + o, sizeof(rest) - o, "jmp %s", t);
        char saved_mn[16];
        snprintf(saved_mn, sizeof(saved_mn), "%s", g_bb3c_pending_cjmp_mn);
        g_bb3c_pending_cjmp_mn[0]     = '\0';
        g_bb3c_pending_cjmp_target[0] = '\0';
        g_bb3c_pending_cjmp_out       = NULL;
        bb3c_format(out, "", saved_mn, rest);
        return;
    }
    char rest[512];
    int o = bb3c_pad_to_width(rest, sizeof(rest), "", BB_COL3_WIDTH);
    snprintf(rest + o, sizeof(rest) - o, "%s %s", m, t);
    bb3c_format(out, "", "", rest);
}

void bb3c_format(FILE *out, const char *label, const char *action, const char *goto_)
{
    if (g_bb3c_pending_cjmp_mn[0]) bb3c_flush_pending_cond_jmp();
    const char *L = label  ? label  : "";
    const char *A = action ? action : "";
    const char *G = goto_  ? goto_  : "";
    int label_only  = (*L) && !(*A) && !(*G);
    int has_content = (*A) || (*G);
    if (g_bb3c_pending_label[0] && g_bb3c_pending_out && g_bb3c_pending_out != out) {
        bb3c_write_line(g_bb3c_pending_out, g_bb3c_pending_label, "", "");
        g_bb3c_pending_label[0] = '\0';
        g_bb3c_pending_out = NULL;
    }
    if (label_only) {
        if (g_bb3c_pending_label[0]) bb3c_write_line(out, g_bb3c_pending_label, "", "");
        snprintf(g_bb3c_pending_label, sizeof(g_bb3c_pending_label), "%s", L);
        g_bb3c_pending_out = out;
        return;
    }
    if (has_content) {
        char fused_lbl[256];
        const char *eff_L = L;
        if (g_bb3c_pending_label[0]) {
            if (!*eff_L) {
                snprintf(fused_lbl, sizeof(fused_lbl), "%s", g_bb3c_pending_label);
                eff_L = fused_lbl;
            } else {
                bb3c_write_line(out, g_bb3c_pending_label, "", "");
            }
            g_bb3c_pending_label[0] = '\0';
            g_bb3c_pending_out = NULL;
        }
        bb3c_write_line(out, eff_L, A, G);
        return;
    }
}

void bb3c_text(const char *label, const char *action, const char *goto_)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    bb3c_format(f, label, action, goto_);
}

void bb_text(const char *fmt, ...)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
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
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fprintf(f, "\n");
}

void emit_comment(const char *text)
{
    FILE *f;
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        bb3c_flush_pending_cjmp_only();
        f = bb_emit_out ? bb_emit_out : stdout;
        fprintf(f, "    # %s\n", text ? text : "");
        return;
    }
}

void emit_bb_box_banner(const char *kind, const char *args)
{
    FILE *f;
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        bb3c_flush_pending_cjmp_only();
        f = bb_emit_out ? bb_emit_out : stdout;
        fputc('#', f);
        for (int i = 1; i < 120; i++) fputc('-', f);
        fputc('\n', f);
        fprintf(f, "    # BOX %s(%s)\n", kind ? kind : "?", args ? args : "");
        return;
    }
}

void emit_banner_stno(int stno, int lineno, const char *src_text)
{
#define STNO_RULE \
    "#=======================================================================================================================\n"
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        FILE *f = bb_emit_out ? bb_emit_out : stdout;
        bb3c_flush_pending();
        fputs(STNO_RULE, f);
        if (src_text && *src_text)
            fprintf(f, "# stmt %d  (line %d):  %s\n", stno, lineno, src_text);
        else if (lineno > 0)
            fprintf(f, "# stmt %d  (line %d)\n", stno, lineno);
        else
            fprintf(f, "# stmt %d\n", stno);
        fputs(STNO_RULE, f);
        return;
    }
    }
#undef STNO_RULE
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_text_3col(FILE *out, const char *label, const char *action, const char *goto_) {
    bb3c_format(out, label, action, goto_);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_text_jmp(FILE *out, const char *mn, const char *target) {
    bb3c_emit_jmp(out, mn, target);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_text_op(const char *label, const char *action, const char *goto_) {
    bb3c_text(label, action, goto_);
}
void emit_text_flush_cjmp(void) { bb3c_flush_pending_cjmp_only(); }
void emit_text_flush(void)      { bb3c_flush_pending(); }

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_text_rawf(const char *fmt, ...) {
    if (bb_emit_mode != EMIT_TEXT) return;
    va_list ap; va_start(ap, fmt); vfprintf(emit_outf(), fmt, ap); va_end(ap);
}
void emit_text_label(bb_label_t *lbl)      { bb_text_label(lbl); }

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_text_comment(const char *fmt, ...) {
    if (bb_emit_mode != EMIT_TEXT) return;
    fprintf(emit_outf(), "    # ");
    va_list ap; va_start(ap, fmt); vfprintf(emit_outf(), fmt, ap); va_end(ap);
    fputc('\n', emit_outf());
}
void emit_text_box_banner(const char *kind, const char *args) { emit_bb_box_banner(kind, args); }
void emit_text_stno_banner(int stno, int lineno, const char *src_text) { emit_banner_stno(stno, lineno, src_text); }

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_text_global(const char *name) {
    if (!IS_TEXT) return;
    bb3c_flush_pending_cjmp_only();
    fprintf(emit_outf(), "    .global %s\n", name ? name : "");
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_frame_enter(void) {
    insn_push_rbp();
    insn_mov_rbp_rsp();
    insn_sub_rsp_i8(8);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_frame_leave(void) {
    insn_mov_rsp_rbp();
    insn_pop_rbp();
    insn_ret();
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_brokered_enter(void) {
    insn_push_rbp();
    insn_mov_rbp_rsp();
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_brokered_leave(int result) {
    insn_mov_eax_i32((uint32_t)result);
    insn_pop_rbp();
    insn_ret();
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_lea_rdi_sym(const char *sym, uint64_t ptr) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF) {
            emit_text_3col(emit_outf(), "", "lea", "rdi, [rip + \\lbl]");
            return;
        }
        char args[80]; snprintf(args, sizeof(args), "rdi, [rip + %s]", sym ? sym : "??");
        emit_text_3col(emit_outf(), "", "lea", args);
        return;
    }
    insn_mov_rdi_i64(ptr);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_lea_rsi_sym(const char *sym, uint64_t ptr) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF) {
            emit_text_3col(emit_outf(), "", "lea", "rcx, [rip + \\namelist_lbl]");
            emit_text_3col(emit_outf(), "", "mov", "rsi, rcx");
            return;
        }
        char args[80]; snprintf(args, sizeof(args), "rcx, [rip + %s]", sym ? sym : "??");
        if (emit_bb_is_format_mode()) {
            fmt_body_append("lea", args);
            fmt_body_append("mov", "rsi, rcx");
            return;
        }
        emit_text_3col(emit_outf(), "", "lea", args);
        emit_text_3col(emit_outf(), "", "mov", "rsi, rcx");
        return;
    }
    insn_mov_rsi_i64(ptr);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_lea_rdx_sym(const char *sym, uint64_t ptr) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF) {
            emit_text_3col(emit_outf(), "", "lea", "rdx, [rip + \\namelist_lbl]");
            return;
        }
        char args[80]; snprintf(args, sizeof(args), "rdx, [rip + %s]", sym ? sym : "??");
        emit_text_3col(emit_outf(), "", "lea", args);
        return;
    }
    insn_mov_rdx_i64(ptr);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_movabs_rdi(uint64_t ptr) {
    insn_mov_rdi_i64(ptr);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_mov_edx_i32(int val) {
    if (bb_emit_mode == EMIT_MACRO_DEF) {
        emit_text_3col(emit_outf(), "", "mov", "edx, \\nargs");
        return;
    }
    insn_mov_edx_i32((uint32_t)val);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_mov_edi_i32(int val) {
    if (bb_emit_mode == EMIT_MACRO_DEF) {
        emit_text_3col(emit_outf(), "", "mov", "edi, \\kind");
        return;
    }
    insn_mov_edi_i32((uint32_t)val);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_inc_r13(uint8_t disp) {
    insn_inc_r13_disp8(disp);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_cmp_delta_i(int n, bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    insn_mov_eax_r10mem();
    insn_cmp_eax_i32((uint32_t)n);
    emit_jmp(lbl_fail, JMP_JNE);
    emit_jmp(lbl_succ, JMP_JMP);
}

void emit_seq_cmp_siglen_delta(int n, uint64_t siglen_addr,
                               bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    insn_mov_rcx_i64(siglen_addr);
    insn_mov_eax_rcxmem();
    insn_sub_eax_i32((uint32_t)n);
    insn_mov_ecx_eax();
    insn_mov_eax_r10mem();
    insn_cmp_eax_ecx();
    emit_jmp(lbl_fail, JMP_JNE);
    emit_jmp(lbl_succ, JMP_JMP);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_sigma_delta_rdi(uint64_t sigma_addr, uint64_t siglen_addr) {
    (void)siglen_addr;
    insn_mov_rcx_i64(sigma_addr);
    insn_mov_rax_rcxmem();
    insn_movsxd_rcx_r10mem();
    insn_lea_rax_rax_rcx();
    insn_mov_rdi_rax();
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_bounds_len(int len, uint64_t siglen_addr, bb_label_t *lbl_fail) {
    insn_mov_eax_r10mem();
    insn_add_eax_i32((uint32_t)len);
    insn_mov_rcx_i64(siglen_addr);
    insn_cmp_eax_rcxmem();
    emit_jmp(lbl_fail, JMP_JG);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_jz_retskip(int pc) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF) {
            emit_text_3col(emit_outf(), "", "jz", ".Lretskip_\\pc\\()");
            return;
        }
        char args[40]; snprintf(args, sizeof(args), ".Lretskip_%d", pc);
        emit_text_3col(emit_outf(), "", "jz", args);
        return;
    }
    insn_nop();
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_retskip_label(int pc) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF) {
            fprintf(emit_outf(), ".Lretskip_\\pc\\():\n");
            return;
        }
        fprintf(emit_outf(), ".Lretskip_%d:\n", pc);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_zeta_rdi(uint64_t ptr, const char *sym) {
    if (IS_TEXT) {
        char args[128]; snprintf(args, sizeof(args), "rdi, [rip + %s]", sym ? sym : "0");
        emit_text_3col(emit_outf(), "", "lea", args);
        return;
    }
    insn_mov_rdi_i64(ptr);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_dispatch_jne_jmp(bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    insn_test_rax_rax();
    emit_jmp(lbl_succ, JMP_JNE);
    emit_jmp(lbl_fail, JMP_JMP);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_call_tgt(const char *sym_or_param) {
    if (!IS_TEXT) return;
    if (bb_emit_mode == EMIT_MACRO_DEF) {
        emit_text_3col(emit_outf(), "", "call", "\\tgt");
        return;
    }
    emit_text_3col(emit_outf(), "", "call", sym_or_param ? sym_or_param : "??");
}

/*--------------------------------------------------------------------------------------------------------------------*/
void emit_seq_noop_macro(const char *macro_name) {
    if (IS_TEXT) emit_text_3col(emit_outf(), "", macro_name, "");
}

void emit_seq_port_call(uint64_t zeta_ptr, const char *fn_name, uint64_t fn_fallback,
                        int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode()) {
            char frag[128]; snprintf(frag, sizeof(frag), "call %s@PLT", fn_name ? fn_name : "??");
            fmt_body_append(frag, "");
            char jne[128]; snprintf(jne, sizeof(jne), "jne %s", lbl_succ->name);
            fmt_body_append(jne, "");
            emit_jmp(lbl_fail, JMP_JMP);
            return;
        }
        emit_text_3col(emit_outf(), "", "push", "r10");
        { char a[80]; snprintf(a,sizeof(a),"rdi, [rip + %s]", fn_name?fn_name:"??");
          emit_text_3col(emit_outf(),"","lea",a); }
        { char a[16]; snprintf(a,sizeof(a),"esi, %d",port);
          emit_text_3col(emit_outf(),"","mov",a); }
        { char a[80]; snprintf(a,sizeof(a),"%s@PLT",fn_name?fn_name:"??");
          emit_text_3col(emit_outf(),"","call",a); }
        emit_text_3col(emit_outf(), "", "pop", "r10");
        insn_test_rax_rax();
        emit_jmp(lbl_succ, JMP_JNE);
        emit_jmp(lbl_fail, JMP_JMP);
        return;
    }
    insn_push_r12();
    insn_mov_rdi_i64(zeta_ptr);
    insn_mov_esi_i32(port);
    insn_call_plt(fn_name, fn_fallback);
    insn_pop_r12();
    insn_test_rax_rax();
    emit_jmp(lbl_succ, JMP_JNE);
    emit_jmp(lbl_fail, JMP_JMP);
}

void emit_seq_port_call_rip(uint64_t zeta_ptr, const char *zeta_label,
                             const char *fn_name, uint64_t fn_fallback,
                             int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode()) {
            char frag[128]; snprintf(frag,sizeof(frag),"call %s@PLT",fn_name?fn_name:"??");
            fmt_body_append(frag,"");
            char jne[128]; snprintf(jne,sizeof(jne),"jne %s",lbl_succ->name);
            fmt_body_append(jne,"");
            emit_jmp(lbl_fail, JMP_JMP);
            return;
        }
        insn_push_r10();
        { char a[80]; snprintf(a,sizeof(a),"rdi, [rip + %s]",zeta_label?zeta_label:"??");
          emit_text_3col(emit_outf(),"","lea",a); }
        { char a[16]; snprintf(a,sizeof(a),"esi, %d",port);
          emit_text_3col(emit_outf(),"","mov",a); }
        { char a[80]; snprintf(a,sizeof(a),"%s@PLT",fn_name?fn_name:"??");
          emit_text_3col(emit_outf(),"","call",a); }
        insn_pop_r10();
        insn_test_rax_rax();
        emit_jmp(lbl_succ, JMP_JNE);
        emit_jmp(lbl_fail, JMP_JMP);
        return;
    }
    (void)zeta_label;
    insn_push_r10();
    insn_mov_rdi_i64(zeta_ptr);
    insn_mov_esi_i32(port);
    insn_call_plt(fn_name, fn_fallback);
    insn_pop_r10();
    insn_test_rax_rax();
    emit_jmp(lbl_succ, JMP_JNE);
    emit_jmp(lbl_fail, JMP_JMP);
}

#define B(b)       bb_emit_byte((uint8_t)(b))
#define U32(v)     bb_emit_u32((uint32_t)(v))
#define U64(v)     bb_emit_u64((uint64_t)(v))

#define T3C(mn,fmt,...)  bb3c_format(emit_outf(),"",mn,fmt)

/*--------------------------------------------------------------------------------------------------------------------*/
static void t3(const char *mn, const char *args) {
    bb3c_format(emit_outf(), "", mn, args);
}

/*--------------------------------------------------------------------------------------------------------------------*/
static void tf(const char *mn, const char *fmt, ...) {
    char buf[64]; va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap);
    bb3c_format(emit_outf(), "", mn, buf);
}

/*--------------------------------------------------------------------------------------------------------------------*/
static void tj(const char *mn, const char *target) {
    bb3c_emit_jmp(emit_outf(), mn, target);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_eax_i32(uint32_t v) {
    if (IS_TEXT)
        tf("mov","eax, %u",v);
    else {
        B(0xB8); U32(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rax_i64(uint64_t v) {
    if (IS_TEXT)
        tf("mov","rax, 0x%llx",(unsigned long long)v);
    else {
        B(0x48); B(0xB8); U64(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rcx_i64(uint64_t v) {
    if (IS_TEXT)
        tf("mov","rcx, 0x%llx",(unsigned long long)v);
    else {
        B(0x48); B(0xB9); U64(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rdx_i64(uint64_t v) {
    if (IS_TEXT)
        tf("mov","rdx, 0x%llx",(unsigned long long)v);
    else {
        B(0x48); B(0xBA); U64(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rsi_i64(uint64_t v) {
    if (IS_TEXT)
        tf("mov","rsi, 0x%llx",(unsigned long long)v);
    else {
        B(0x48); B(0xBE); U64(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rdi_i64(uint64_t v) {
    if (IS_TEXT)
        tf("mov","rdi, 0x%llx",(unsigned long long)v);
    else {
        B(0x48); B(0xBF); U64(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_edx_i32(uint32_t v) {
    if (IS_TEXT)
        tf("mov","edx, %u",v);
    else {
        B(0xBA); U32(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_edi_i32(uint32_t v) {
    if (IS_TEXT)
        tf("mov","edi, %u",v);
    else {
        B(0xBF); U32(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_esi_i32(int v) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF)
            t3("mov","esi, \\n");
        else {
            tf("mov","esi, %d",v); return;
        }
    }
    B(0xBE); U32((uint32_t)v);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rbp_rsp(void) {
    if (IS_TEXT)
        t3("mov","rbp, rsp");
    else {
        B(0x48); B(0x89); B(0xE5);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rsp_rbp(void) {
    if (IS_TEXT)
        t3("mov","rsp, rbp");
    else {
        B(0x48); B(0x89); B(0xEC);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_ecx_eax(void) {
    if (IS_TEXT)
        t3("mov","ecx, eax");
    else {
        B(0x89); B(0xC1);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rdi_rax(void) {
    if (IS_TEXT)
        t3("mov","rdi, rax");
    else {
        B(0x48); B(0x89); B(0xC7);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_eax_r10mem(void) {
    if (IS_TEXT)
        t3("mov","eax, [r10]");
    else {
        B(0x41); B(0x8B); B(0x02);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_eax_rcxmem(void) {
    if (IS_TEXT)
        t3("mov","eax, [rcx]");
    else {
        B(0x8B); B(0x01);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rax_rcxmem(void) {
    if (IS_TEXT)
        t3("mov","rax, [rcx]");
    else {
        B(0x48); B(0x8B); B(0x01);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_lea_rcx_rip_sym(const char *sym, uint64_t addr) {
    if (IS_TEXT)
        tf("lea","rcx, [rip + %s]", sym ? sym : "??");
    else {
        B(0x48); B(0xB9); U64(addr);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_lea_r10_rip_sym(const char *sym, uint64_t addr) {
    if (IS_TEXT)
        tf("lea","r10, [rip + %s]", sym ? sym : "??");
    else {
        B(0x49); B(0xBA); U64(addr);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_lea_rax_rax_rcx(void) {
    if (IS_TEXT)
        t3("lea","rax, [rax+rcx]");
    else {
        B(0x48); B(0x8D); B(0x04); B(0x08);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_movzx_eax_rdi_off8(uint8_t off) {
    if (IS_TEXT)
        tf("movzx","eax, byte [rdi + %u]",(unsigned)off);
    else {
        B(0x0F); B(0xB6); B(0x47); B(off);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_movsxd_rcx_r10mem(void) {
    if (IS_TEXT)
        t3("movsxd","rcx, dword [r10]");
    else {
        B(0x49); B(0x63); B(0x0A);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_cmp_esi_i8(uint8_t v) {
    if (IS_TEXT)
        tf("cmp","esi, %u",(unsigned)v);
    else {
        B(0x83); B(0xFE); B(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_cmp_esi_i32(uint32_t v) {
    if (IS_TEXT)
        tf("cmp","esi, %u",v);
    else {
        B(0x81); B(0xFE); U32(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_cmp_al_i8(uint8_t v) {
    if (IS_TEXT)
        tf("cmp","al, %u",(unsigned)v);
    else {
        B(0x3C); B(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_cmp_eax_i32(uint32_t v) {
    if (IS_TEXT)
        tf("cmp","eax, %u",v);
    else {
        B(0x3D); U32(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_cmp_eax_ecx(void) {
    if (IS_TEXT)
        t3("cmp","eax, ecx");
    else {
        B(0x39); B(0xC8);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_cmp_eax_rcxmem(void) {
    if (IS_TEXT)
        t3("cmp","eax, [rcx]");
    else {
        B(0x3B); B(0x01);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_add_rsp_i8(uint8_t v) {
    if (IS_TEXT)
        tf("add","rsp, %u",(unsigned)v);
    else {
        B(0x48); B(0x83); B(0xC4); B(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_sub_rsp_i8(uint8_t v) {
    if (IS_TEXT)
        tf("sub","rsp, %u",(unsigned)v);
    else {
        B(0x48); B(0x83); B(0xEC); B(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_add_eax_i32(uint32_t v) {
    if (IS_TEXT)
        tf("add","eax, %u",v);
    else {
        B(0x05); U32(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_sub_eax_i32(uint32_t v) {
    if (IS_TEXT)
        tf("sub","eax, %u",v);
    else {
        B(0x2D); U32(v);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_add_delta_i(int v) {
    if (IS_TEXT) {
        char a[32]; snprintf(a,sizeof(a),"eax, %d",v);
        if (emit_bb_is_format_mode()) {
            fmt_body_append("mov","eax, [r10]");
            fmt_body_append("add",a);
            fmt_body_append("mov","[r10], eax");
            return;
        }
        t3("mov","eax, [r10]"); t3("add",a); t3("mov","[r10], eax"); return;
    }
    B(0x41); B(0x8B); B(0x02);
    B(0x05); U32((uint32_t)v);
    B(0x41); B(0x89); B(0x02);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_sub_delta_i(int v) {
    if (IS_TEXT) {
        char a[32]; snprintf(a,sizeof(a),"eax, %d",v);
        if (emit_bb_is_format_mode()) {
            fmt_body_append("mov","eax, [r10]");
            fmt_body_append("sub",a);
            fmt_body_append("mov","[r10], eax");
            return;
        }
        t3("mov","eax, [r10]"); t3("sub",a); t3("mov","[r10], eax"); return;
    }
    B(0x41); B(0x8B); B(0x02);
    B(0x2D); U32((uint32_t)v);
    B(0x41); B(0x89); B(0x02);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_test_rax_rax(void) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode())
            fmt_body_append("test","rax, rax");
        else {
            t3("test","rax, rax"); return;
        }
    }
    B(0x48); B(0x85); B(0xC0);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_test_eax_eax(void) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode())
            fmt_body_append("test","eax, eax");
        else {
            t3("test","eax, eax"); return;
        }
    }
    B(0x85); B(0xC0);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_xor_eax_eax(void) {
    if (IS_TEXT)
        t3("xor","eax, eax");
    else {
        B(0x31); B(0xC0);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_inc_r13_disp8(uint8_t disp) {
    if (IS_TEXT)
        tf("inc","dword ptr [r13 + %u]",(unsigned)disp);
    else {
        B(0x41); B(0xFF); B(0x45); B(disp);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_push_rbp(void) {
    if (IS_TEXT)
        t3("push","rbp");
    else
        B(0x55);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_pop_rbp(void) {
    if (IS_TEXT)
        t3("pop","rbp");
    else
        B(0x5D);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_push_r10(void) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode())
            fmt_body_append("push","r10");
        else {
            t3("push","r10"); return;
        }
    }
    B(0x41); B(0x52);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_pop_r10(void) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode())
            fmt_body_append("pop","r10");
        else {
            t3("pop","r10"); return;
        }
    }
    B(0x41); B(0x5A);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_push_r12(void) {
    if (IS_TEXT)
        t3("push","r12");
    else {
        B(0x41); B(0x54);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_pop_r12(void) {
    if (IS_TEXT)
        t3("pop","r12");
    else {
        B(0x41); B(0x5C);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_ret(void) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        t3("ret",""); return;
    }
    B(0xC3);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_nop(void) {
    if (IS_TEXT)
        t3("nop","");
    else
        B(0x90);
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_call_rax(void) {
    if (IS_TEXT)
        t3("call","rax");
    else {
        B(0xFF); B(0xD0);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_call_plt(const char *sym, uint64_t fn_fallback) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        char args[80]; snprintf(args,sizeof(args),"%s@PLT",sym?sym:"??");
        if (emit_bb_is_format_mode())
            fmt_body_append("call",args);
        else {
            t3("call",args); return;
        }
    }
    B(0x48); B(0xB8); U64(fn_fallback);
    B(0xFF); B(0xD0);
}

#define INSN_JMP_R8(fn, op) \
    void fn(bb_label_t *t) { \
        if (IS_TEXT) { tj(#op, t->name); return; } \
        B(0x##op); bb_emit_patch_rel8(t); \
    }
#define INSN_JCC_R8(fn, mn, op) \
    void fn(bb_label_t *t) { \
        if (IS_TEXT) { tj(mn, t->name); return; } \
        B(0x##op); bb_emit_patch_rel8(t); \
    }
#define INSN_JCC_R32(fn, mn, op1, op2) \
    void fn(bb_label_t *t) { \
        if (IS_TEXT) { tj(mn, t->name); return; } \
        B(0x0F); B(0x##op1); bb_emit_patch_rel32(t); \
    }

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_jmp_r8(bb_label_t *t) {
    if (IS_TEXT)
        tj("jmp", t->name);
    else {
        B(0xEB); bb_emit_patch_rel8(t);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_jmp_r32(bb_label_t *t) {
    if (IS_TEXT)
        tj("jmp", t->name);
    else {
        B(0xE9); bb_emit_patch_rel32(t);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_je_r8(bb_label_t *t) {
    if (IS_TEXT)
        tj("je", t->name);
    else {
        B(0x74); bb_emit_patch_rel8(t);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_je_r32(bb_label_t *t) {
    if (IS_TEXT)
        tj("je", t->name);
    else {
        B(0x0F); B(0x84); bb_emit_patch_rel32(t);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_jne_r8(bb_label_t *t) {
    if (IS_TEXT)
        tj("jne", t->name);
    else {
        B(0x75); bb_emit_patch_rel8(t);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_jne_r32(bb_label_t *t) {
    if (IS_TEXT)
        tj("jne", t->name);
    else {
        B(0x0F); B(0x85); bb_emit_patch_rel32(t);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_jl_r8(bb_label_t *t) {
    if (IS_TEXT)
        tj("jl", t->name);
    else {
        B(0x7C); bb_emit_patch_rel8(t);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_jl_r32(bb_label_t *t) {
    if (IS_TEXT)
        tj("jl", t->name);
    else {
        B(0x0F); B(0x8C); bb_emit_patch_rel32(t);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_jge_r8(bb_label_t *t) {
    if (IS_TEXT)
        tj("jge", t->name);
    else {
        B(0x7D); bb_emit_patch_rel8(t);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_jge_r32(bb_label_t *t) {
    if (IS_TEXT)
        tj("jge", t->name);
    else {
        B(0x0F); B(0x8D); bb_emit_patch_rel32(t);
    }
}

/*--------------------------------------------------------------------------------------------------------------------*/
void insn_jg_r32(bb_label_t *t) {
    if (IS_TEXT)
        tj("jg", t->name);
    else {
        B(0x0F); B(0x8F); bb_emit_patch_rel32(t);
    }
}

void bb_insn_mov_eax_imm32(uint32_t imm)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("mov", "eax, %u", imm);
    else {
        bb_emit_byte(MOV_EAX_IMM32); bb_emit_u32(imm);
    }
}

void bb_insn_mov_rax_imm64(uint64_t imm)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("mov", "rax, 0x%llx", (unsigned long long)imm);
    else {
        bb_emit_byte(REX_W); bb_emit_byte(MOV_EAX_IMM32); bb_emit_u64(imm);
    }
}

void bb_insn_ret(void)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("ret", NULL);
    else
        bb_emit_byte(RET);
}

void bb_insn_nop(void)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("nop", NULL);
    else
        bb_emit_byte(NOP);
}

void bb_insn_call_rax(void)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("call", "rax");
    else {
        bb_emit_byte(INC_CALL_FF); bb_emit_byte(MODRM_CALL_RAX);
    }
}

void bb_insn_jmp_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_jmp("jmp", target->name);
    else {
        bb_emit_byte(JMP_REL8); bb_emit_patch_rel8(target);
    }
}

void bb_insn_jmp_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_jmp("jmp", target->name);
    else {
        bb_emit_byte(JMP_REL32); bb_emit_patch_rel32(target);
    }
}

void bb_insn_jl_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_jmp("jl", target->name);
    else {
        bb_emit_byte(JL_REL8); bb_emit_patch_rel8(target);
    }
}

void bb_insn_jge_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_jmp("jge", target->name);
    else {
        bb_emit_byte(JGE_REL8); bb_emit_patch_rel8(target);
    }
}

void bb_insn_je_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_jmp("je", target->name);
    else {
        bb_emit_byte(JE_REL8); bb_emit_patch_rel8(target);
    }
}

void bb_insn_jne_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_jmp("jne", target->name);
    else {
        bb_emit_byte(JNE_REL8); bb_emit_patch_rel8(target);
    }
}

void bb_insn_jne_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_jmp("jne", target->name);
    else {
        bb_emit_byte(ESC); bb_emit_byte(TEST_RM_R); bb_emit_patch_rel32(target);
    }
}

void bb_insn_je_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_jmp("je", target->name);
    else {
        bb_emit_byte(ESC); bb_emit_byte(JE_REL32_X); bb_emit_patch_rel32(target);
    }
}

void bb_insn_jl_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_jmp("jl", target->name);
    else {
        bb_emit_byte(ESC); bb_emit_byte(JL_REL32_X); bb_emit_patch_rel32(target);
    }
}

void bb_insn_jge_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_jmp("jge", target->name);
    else {
        bb_emit_byte(ESC); bb_emit_byte(LEA); bb_emit_patch_rel32(target);
    }
}

void bb_insn_jg_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_jmp("jg", target->name);
    else {
        bb_emit_byte(ESC); bb_emit_byte(JG_REL32_X); bb_emit_patch_rel32(target);
    }
}

void bb_insn_cmp_esi_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("cmp", "esi, %u", (unsigned)imm);
    else {
        bb_emit_byte(CMP_RM_IMM8); bb_emit_byte(MODRM_CMP_ESI); bb_emit_byte(imm);
    }
}

void bb_insn_cmp_esi_imm32(uint32_t imm)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("cmp", "esi, %u", imm);
    else {
        bb_emit_byte(CMP_RM_IMM32); bb_emit_byte(MODRM_CMP_ESI); bb_emit_u32(imm);
    }
}

void bb_insn_movzx_eax_rdi_off8(uint8_t off)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("movzx", "eax, byte [rdi + %u]", (unsigned)off);
    else {
        bb_emit_byte(ESC); bb_emit_byte(MOVZX_R_RM8); bb_emit_byte(MODRM_EAX_EDI7); bb_emit_byte(off);
    }
}

void bb_insn_cmp_al_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("cmp", "al, %u", (unsigned)imm);
    else {
        bb_emit_byte(CMP_AL_IMM8); bb_emit_byte(imm);
    }
}

void bb_insn_xor_eax_eax(void)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("xor", "eax, eax");
    else {
        bb_emit_byte(XOR_RM_R); bb_emit_byte(MODRM_EAX_EAX);
    }
}

void bb_insn_push_rbp(void)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("push", "rbp");
    else
        bb_emit_byte(PUSH_RBP);
}

void bb_insn_pop_rbp(void)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("pop", "rbp");
    else
        bb_emit_byte(POP_RBP);
}

void bb_insn_mov_rbp_rsp(void)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("mov", "rbp, rsp");
    else {
        bb_emit_byte(REX_W); bb_emit_byte(MOV_RM_R); bb_emit_byte(MODRM_RBP_RSP);
    }
}

void bb_insn_sub_rsp_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("sub", "rsp, %u", (unsigned)imm);
    else {
        bb_emit_byte(REX_W); bb_emit_byte(CMP_RM_IMM8); bb_emit_byte(MODRM_RSP_RBP); bb_emit_byte(imm);
    }
}

void bb_insn_add_rsp_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT)
        bb3c_op("add", "rsp, %u", (unsigned)imm);
    else {
        bb_emit_byte(REX_W); bb_emit_byte(CMP_RM_IMM8); bb_emit_byte(MODRM_CMP_RSP); bb_emit_byte(imm);
    }
}

void emit_ret(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(RET); return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", "ret", ""); return;
    }
}

void emit_push_r10(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_B); bb_emit_byte(REX_B_PUSH_R10); return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode())
            fmt_body_append("push", "r10");
        else {
            bb3c_format(emit_outf(), "", "push", "r10"); return;
        }
    }
}

void emit_pop_r10(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_B); bb_emit_byte(REX_B_POP_R10); return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode())
            fmt_body_append("pop", "r10");
        else {
            bb3c_format(emit_outf(), "", "pop", "r10"); return;
        }
    }
}

void emit_test_rax_rax(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_W); bb_emit_byte(TEST_RM_R); bb_emit_byte(MODRM_EAX_EAX); return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", "test", "rax, rax"); return;
    }
}

void emit_test_eax_eax(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(TEST_RM_R); bb_emit_byte(MODRM_EAX_EAX); return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode())
            fmt_body_append("test", "eax, eax");
        else {
            bb3c_format(emit_outf(), "", "test", "eax, eax"); return;
        }
    }
}

void emit_mov_rdi_imm64(uint64_t val)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED: {
        bb_emit_byte(REX_W); bb_emit_byte(MOV_EDI_IMM32);
        bb_emit_byte((uint8_t)(val      )); bb_emit_byte((uint8_t)(val >>  8));
        bb_emit_byte((uint8_t)(val >> 16)); bb_emit_byte((uint8_t)(val >> 24));
        bb_emit_byte((uint8_t)(val >> 32)); bb_emit_byte((uint8_t)(val >> 40));
        bb_emit_byte((uint8_t)(val >> 48)); bb_emit_byte((uint8_t)(val >> 56));
        return;
    }
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        char args[64];
        snprintf(args, sizeof(args), "rdi, 0x%llx", (unsigned long long)val);
        bb3c_format(emit_outf(), "", "mov", args); return;
    }
    }
}

void emit_call_sym_plt(const char *sym, uint64_t fn_fallback)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED: {
        bb_emit_byte(REX_W); bb_emit_byte(MOV_EAX_IMM32);
        bb_emit_byte((uint8_t)(fn_fallback      )); bb_emit_byte((uint8_t)(fn_fallback >>  8));
        bb_emit_byte((uint8_t)(fn_fallback >> 16)); bb_emit_byte((uint8_t)(fn_fallback >> 24));
        bb_emit_byte((uint8_t)(fn_fallback >> 32)); bb_emit_byte((uint8_t)(fn_fallback >> 40));
        bb_emit_byte((uint8_t)(fn_fallback >> 48)); bb_emit_byte((uint8_t)(fn_fallback >> 56));
        bb_emit_byte(INC_CALL_FF); bb_emit_byte(MODRM_CALL_RAX);
        return;
    }
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        char args[80];
        snprintf(args, sizeof(args), "%s@PLT", sym ? sym : "??sym??");
        if (emit_bb_is_format_mode())
            fmt_body_append("call", args);
        else {
            bb3c_format(emit_outf(), "", "call", args); return;
        }
    }
    }
}

void emit_mov_esi_imm32(int val)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED: {
        uint32_t u = (uint32_t)val;
        bb_emit_byte(MOV_ESI_IMM32);
        bb_emit_byte((uint8_t)(u      )); bb_emit_byte((uint8_t)(u >>  8));
        bb_emit_byte((uint8_t)(u >> 16)); bb_emit_byte((uint8_t)(u >> 24));
        return;
    }
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;
        char args[32]; snprintf(args, sizeof(args), "esi, %d", val);
        bb3c_format(emit_outf(), "", "mov", args); return;
    }
    case EMIT_MACRO_DEF:
        bb3c_format(emit_outf(), "", "mov", "esi, \\n"); return;
    }
}

void emit_add_delta_imm(int v)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_B); bb_emit_byte(MOV_R_RM); bb_emit_byte(MODRM_R10_INDIR);
        bb_emit_byte(ADD_EAX_IMM32);
        bb_emit_byte((uint8_t)((uint32_t)v      )); bb_emit_byte((uint8_t)((uint32_t)v >>  8));
        bb_emit_byte((uint8_t)((uint32_t)v >> 16)); bb_emit_byte((uint8_t)((uint32_t)v >> 24));
        bb_emit_byte(REX_B); bb_emit_byte(MOV_RM_R); bb_emit_byte(MODRM_R10_INDIR);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[32]; snprintf(args, sizeof(args), "eax, %d", v);
        if (emit_bb_is_format_mode()) {
            fmt_body_append("mov", "eax, [r10]");
            fmt_body_append("add", args);
            fmt_body_append("mov", "[r10], eax");
            return;
        }
        FILE *f = emit_outf();
        bb3c_format(f, "", "mov", "eax, [r10]");
        bb3c_format(f, "", "add", args);
        bb3c_format(f, "", "mov", "[r10], eax");
        return;
    }
    }
}

void emit_sub_delta_imm(int v)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_B); bb_emit_byte(MOV_R_RM); bb_emit_byte(MODRM_R10_INDIR);
        bb_emit_byte(SUB_EAX_IMM32);
        bb_emit_byte((uint8_t)((uint32_t)v      )); bb_emit_byte((uint8_t)((uint32_t)v >>  8));
        bb_emit_byte((uint8_t)((uint32_t)v >> 16)); bb_emit_byte((uint8_t)((uint32_t)v >> 24));
        bb_emit_byte(REX_B); bb_emit_byte(MOV_RM_R); bb_emit_byte(MODRM_R10_INDIR);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[32]; snprintf(args, sizeof(args), "eax, %d", v);
        if (emit_bb_is_format_mode()) {
            fmt_body_append("mov", "eax, [r10]");
            fmt_body_append("sub", args);
            fmt_body_append("mov", "[r10], eax");
            return;
        }
        FILE *f = emit_outf();
        bb3c_format(f, "", "mov", "eax, [r10]");
        bb3c_format(f, "", "sub", args);
        bb3c_format(f, "", "mov", "[r10], eax");
        return;
    }
    }
}

void bb_insn_mov_rsp_rbp(void)          
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_W); bb_emit_byte(MOV_RM_R); bb_emit_byte(MODRM_RSP_RBP); return;
    default: bb3c_format(emit_outf(), "", "mov", "rsp, rbp"); return;
    }
}

void bb_insn_mov_rdx_imm64(uint64_t v)  
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_W); bb_emit_byte(MOV_EDX_IMM32);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        bb_emit_byte((uint8_t)(v >> 32)); bb_emit_byte((uint8_t)(v >> 40));
        bb_emit_byte((uint8_t)(v >> 48)); bb_emit_byte((uint8_t)(v >> 56));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"rdx, 0x%llx",(unsigned long long)v);
               bb3c_format(emit_outf(),"","mov",a); return; }
    }
}

void bb_insn_mov_edx_imm32(uint32_t v)  
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(MOV_EDX_IMM32);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"edx, %u",v);
               bb3c_format(emit_outf(),"","mov",a); return; }
    }
}

void bb_insn_mov_edi_imm32(uint32_t v)  
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(MOV_EDI_IMM32);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"edi, %u",v);
               bb3c_format(emit_outf(),"","mov",a); return; }
    }
}

void bb_insn_mov_rsi_imm64(uint64_t v)  
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_W); bb_emit_byte(MOV_ESI_IMM32);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        bb_emit_byte((uint8_t)(v >> 32)); bb_emit_byte((uint8_t)(v >> 40));
        bb_emit_byte((uint8_t)(v >> 48)); bb_emit_byte((uint8_t)(v >> 56));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"rsi, 0x%llx",(unsigned long long)v);
               bb3c_format(emit_outf(),"","mov",a); return; }
    }
}

void bb_insn_push_r12(void)             
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_B); bb_emit_byte(REX_B_PUSH_R12); return;
    default: bb3c_format(emit_outf(), "", "push", "r12"); return;
    }
}

void bb_insn_pop_r12(void)              
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_B); bb_emit_byte(REX_B_POP_R12); return;
    default: bb3c_format(emit_outf(), "", "pop", "r12"); return;
    }
}

void bb_insn_inc_r13_disp8(uint8_t disp) 
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_B); bb_emit_byte(INC_CALL_FF);
        bb_emit_byte(MODRM_INC_R13D); bb_emit_byte(disp);
        return;
    default: { char a[40]; snprintf(a,sizeof(a),"dword ptr [r13 + %u]",(unsigned)disp);
               bb3c_format(emit_outf(),"","inc",a); return; }
    }
}

void bb_insn_mov_rcx_imm64(uint64_t v)  
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_W); bb_emit_byte(MOV_ECX_IMM32);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        bb_emit_byte((uint8_t)(v >> 32)); bb_emit_byte((uint8_t)(v >> 40));
        bb_emit_byte((uint8_t)(v >> 48)); bb_emit_byte((uint8_t)(v >> 56));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"rcx, 0x%llx",(unsigned long long)v);
               bb3c_format(emit_outf(),"","mov",a); return; }
    }
}

void bb_insn_mov_eax_r10mem(void)        
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_B); bb_emit_byte(MOV_R_RM); bb_emit_byte(MODRM_R10_INDIR); return;
    default: bb3c_format(emit_outf(), "", "mov", "eax, [r10]"); return;
    }
}

void bb_insn_cmp_eax_imm32(uint32_t v)  
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(CMP_EAX_IMM32);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"eax, %u",v);
               bb3c_format(emit_outf(),"","cmp",a); return; }
    }
}

void bb_insn_mov_eax_mem_rcx(void)       
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(MOV_R_RM); bb_emit_byte(MODRM_RCX_INDIR); return;
    default: bb3c_format(emit_outf(), "", "mov", "eax, [rcx]"); return;
    }
}

void bb_insn_sub_eax_imm32(uint32_t v)  
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(SUB_EAX_IMM32);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"eax, %u",v);
               bb3c_format(emit_outf(),"","sub",a); return; }
    }
}

void bb_insn_mov_ecx_eax(void)           
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(MOV_RM_R); bb_emit_byte(MODRM_ECX_EAX); return;
    default: bb3c_format(emit_outf(), "", "mov", "ecx, eax"); return;
    }
}

void bb_insn_cmp_eax_ecx(void)           
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(CMP_RM_R); bb_emit_byte(MODRM_EAX_ECX); return;
    default: bb3c_format(emit_outf(), "", "cmp", "eax, ecx"); return;
    }
}

void bb_insn_mov_rax_mem_rcx(void)       
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_W); bb_emit_byte(MOV_R_RM); bb_emit_byte(MODRM_RCX_INDIR); return;
    default: bb3c_format(emit_outf(), "", "mov", "rax, [rcx]"); return;
    }
}

void bb_insn_movsxd_rcx_r10mem(void)     
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_WB); bb_emit_byte(MOVSXD_R_RM); bb_emit_byte(MODRM_RCX_R10); return;
    default: bb3c_format(emit_outf(), "", "movsxd", "rcx, dword [r10]"); return;
    }
}

void bb_insn_lea_rax_rax_rcx(void)       
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_W); bb_emit_byte(LEA);
        bb_emit_byte(MODRM_RAX_RAXRCX); bb_emit_byte(SIB_RAX_RCX); return;
    default: bb3c_format(emit_outf(), "", "lea", "rax, [rax+rcx]"); return;
    }
}

void bb_insn_mov_rdi_rax(void)           
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(REX_W); bb_emit_byte(MOV_RM_R); bb_emit_byte(MODRM_RDI_RAX); return;
    default: bb3c_format(emit_outf(), "", "mov", "rdi, rax"); return;
    }
}

void bb_insn_add_eax_imm32(uint32_t v)  
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(ADD_EAX_IMM32);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"eax, %u",v);
               bb3c_format(emit_outf(),"","add",a); return; }
    }
}

void bb_insn_cmp_eax_mem_rcx(void)       
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(CMP_R_RM); bb_emit_byte(MODRM_RCX_INDIR); return;
    default: bb3c_format(emit_outf(), "", "cmp", "eax, [rcx]"); return;
    }
}
