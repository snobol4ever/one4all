#include "emit_core.h"
#include "emit_globals.h"
#include "emit_io.h"
#include "stage2.h"
#include "BB_templates/bb_templates.h"
#include "SM_templates/sm_templates.h"
#include "sm_jit_interp.h"
#include "emit_form.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
bb_emit_mode_t  bb_emit_mode = EMIT_BINARY_WIRED;
FILE           *bb_emit_out  = NULL;
int g_bb_emit_format  = 0;
int g_in_text_macro_body = 0;
void emit_mode_set(bb_emit_mode_t m, FILE *out)
{
    bb_emit_mode = m;
    bb_emit_out  = out;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
FILE *emit_outf(void) { return bb_emit_out ? bb_emit_out : stdout; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int emit_bb_is_format_mode(void) {
    return g_bb_emit_format &&
           (bb_emit_mode == EMIT_TEXT || bb_emit_mode == EMIT_TEXT_INLINE);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static char g_fmt_label[BB_LABEL_NAME_MAX + 4];
static char g_fmt_body[512];
static void fmt_label_save(bb_label_t *lbl) {
    if (lbl && lbl->name[0]) snprintf(g_fmt_label, sizeof(g_fmt_label), "%s:", lbl->name);
    else g_fmt_label[0] = '\0';
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void fmt_body_append(const char *instr, const char *operands) {
    char frag[128];
    if (operands && operands[0]) snprintf(frag, sizeof(frag), "%s %s", instr, operands);
    else                          snprintf(frag, sizeof(frag), "%s", instr);
    if (g_fmt_body[0]) { strncat(g_fmt_body, " ; ", sizeof(g_fmt_body) - strlen(g_fmt_body) - 1); }
    strncat(g_fmt_body, frag, sizeof(g_fmt_body) - strlen(g_fmt_body) - 1);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_label_define(bb_label_t *lbl)
{
    if (emit_bb_is_format_mode())
        fmt_label_save(lbl);
    else
        bb_label_define(lbl);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int bb_emit_overflow;
int bb_emit_end(void)
{
    if (bb_emit_overflow) {
        bb_patch_count = 0;  /* patches are invalid due to overflow — discard */
        return -1;
    }
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
        bb_emit_overflow = 1;
        return;
    }
    bb_patch_list[bb_patch_count].site  = bb_emit_pos;
    bb_patch_list[bb_patch_count].label = lbl;
    bb_patch_list[bb_patch_count].kind  = PATCH_REL8;
    bb_patch_count++;
    bb_emit_byte(0x00);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
        bb_emit_overflow = 1;  /* treat patch overflow same as byte overflow */
        return;
    }
    bb_patch_list[bb_patch_count].site  = bb_emit_pos;
    bb_patch_list[bb_patch_count].label = lbl;
    bb_patch_list[bb_patch_count].kind  = PATCH_REL32;
    bb_patch_count++;
    bb_emit_u32(0x00000000);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int bb_emit_overflow = 0;
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
        bb_emit_overflow = 1;
        return;  /* silently drop — caller checks emitter_end() > FLAT_BUF_MAX */
    }
    bb_emit_buf[bb_emit_pos++] = b;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void  bb_emit_u32(uint32_t v)  { bb_emit_byte((uint8_t)(v)); bb_emit_byte((uint8_t)(v>>8)); bb_emit_byte((uint8_t)(v>>16)); bb_emit_byte((uint8_t)(v>>24)); }
void  bb_emit_u64(uint64_t v)  { bb_emit_u32((uint32_t)(v)); bb_emit_u32((uint32_t)(v >> 32)); }
void  bb_emit_i32(int32_t v)   { uint32_t u; memcpy(&u, &v, 4); bb_emit_u32(u); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  g_is_text        = 0;
int  g_emit_text_mode = TEXT_MODE_INVOCATION;
int  g_emit_pos       = 0;
void emitter_init_binary(bb_buf_t buf, int size)
{
    g_is_text = 0; g_emit_text_mode = TEXT_MODE_INVOCATION; g_emit_pos = 0;
    bb_emit_overflow = 0;
    bb_emit_mode = EMIT_BINARY_WIRED;
    bb_emit_begin(buf, size);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emitter_init_text(FILE *out, int mode)
{
    g_is_text = 1; g_emit_text_mode = mode; g_emit_pos = 0;
    bb_emit_mode = EMIT_TEXT;
    bb_emit_out  = out ? out : stdout;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int  emitter_end(void)        { return g_is_text ? g_emit_pos : bb_emit_end(); }
void  emitter_init_macro_def(FILE *out) { emitter_init_text(out, TEXT_MODE_DEFINITION); }
static void  ef_b1 (uint8_t a)                                   { bb_emit_byte(a); }
static void  ef_b2 (uint8_t a, uint8_t b)                        { bb_emit_byte(a); bb_emit_byte(b); }
static void  ef_b3 (uint8_t a, uint8_t b, uint8_t c)             { bb_emit_byte(a); bb_emit_byte(b); bb_emit_byte(c); }
static void  ef_b4 (uint8_t a, uint8_t b, uint8_t c, uint8_t d)  { bb_emit_byte(a); bb_emit_byte(b); bb_emit_byte(c); bb_emit_byte(d); }
static void  ef_u32(uint32_t v)                                  { bb_emit_u32(v); }
static void  ef_u64(uint64_t v)                                  { bb_emit_u64(v); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void ef_t3c(const char *mnem, const char *fmt, ...)
{
    char buf[256]; buf[0] = '\0';
    if (fmt) { va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap); }
    bb3c_format(bb_emit_out, "", mnem ? mnem : "", buf);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void ef_t3c_jmp(const char *mnem, const char *target)
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
{ bb3c_emit_jmp(bb_emit_out, mnem ? mnem : "", target ? target : ""); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_reg32_imm32(uint8_t op, uint32_t val, const char *mnem)
{
    if (g_is_text) { ef_t3c("mov", "%s, %u", mnem, val); g_emit_pos += 5; }
    else           { ef_b1(op); ef_u32(val); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_reg_reg2(uint8_t b0, uint8_t b1_, const char *text)
{
    if (g_is_text) { ef_t3c(NULL, "%s", text); g_emit_pos += 2; }
    else           { ef_b2(b0, b1_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_reg_reg3(uint8_t b0, uint8_t b1_, uint8_t b2_, const char *text)
{
    if (g_is_text) { ef_t3c(NULL, "%s", text); g_emit_pos += 3; }
    else           { ef_b3(b0, b1_, b2_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_mem3(uint8_t b0, uint8_t b1_, uint8_t b2_, const char *text)
{
    if (g_is_text) { ef_t3c(NULL, "%s", text); g_emit_pos += 3; }
    else           { ef_b3(b0, b1_, b2_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_mem4(uint8_t b0, uint8_t b1_, uint8_t b2_, uint8_t b3_, const char *text)
{
    if (g_is_text) { ef_t3c(NULL, "%s", text); g_emit_pos += 4; }
    else           { ef_b4(b0, b1_, b2_, b3_); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_form_nullary1(uint8_t b0, const char *text)
{
    if (g_is_text) { ef_t3c(text, NULL); g_emit_pos += 1; }
    else           { ef_b1(b0); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_sym_lea_rcx(const char *sym, uint64_t addr)
{
    if (g_is_text) { ef_t3c("lea", "rcx, [rip + %s]", sym ? sym : "??"); g_emit_pos += 7; }
    else           { ef_b2(0x48,0xB9); ef_u64(addr); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_sym_lea_r10(const char *sym, uint64_t addr)
{
    if (g_is_text) { ef_t3c("lea", "r10, [rip + %s]", sym ? sym : "??"); g_emit_pos += 7; }
    else           { ef_b2(0x49,0xBA); ef_u64(addr); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void  emit_store_delta       (void)           { emit_mov_r10mem_eax(); }
void  emit_load_sigma        (uint64_t a)     { emit_sym_lea_rcx("\xCE\xA3", a);        emit_mov_rax_rcxmem(); }
void  emit_sigma_plus_delta  (uint64_t a)     { emit_load_sigma(a); emit_movsxd_rcx_r10mem(); emit_lea_rax_raxrcx(); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_jmp_label(bb_label_t *target, jmp_kind_t kind)
{
    static const char    *mn[]    = {"jmp","je","jne","jl","jge","jg"};
    static const uint8_t  ops[6][2] = {{0xE9,0x00},{0x0F,0x84},{0x0F,0x85},{0x0F,0x8C},{0x0F,0x8D},{0x0F,0x8F}};
    int k = (int)kind < 6 ? (int)kind : 0;
    if (g_is_text) { ef_t3c_jmp(mn[k], target->name); g_emit_pos += 6; }
    else { if (k==0) ef_b1(0xE9); else ef_b2(ops[k][0], ops[k][1]); bb_emit_patch_rel32(target); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* DAI-8 cluster 3: ef_greek_port deleted (static, zero callers after C2 byte-emit sweep). */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_label_define(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        FILE *f = bb_emit_out ? bb_emit_out : stdout;
        char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", lbl->name);
        bb3c_format(f, lbuf, "", "");
        return;
    }
    if (bb_emit_overflow) return;  /* overflow already occurred — skip patching */
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_label_initf(bb_label_t *lbl, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vsnprintf(lbl->name, BB_LABEL_NAME_MAX, fmt, ap);
    va_end(ap);
    lbl->offset = BB_LABEL_UNRESOLVED;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void  bb3c_flush_pending_cjmp_only(void)  { bb3c_flush_pending_cond_jmp(); }
void  bb3c_flush_pending          (void)  { bb3c_flush_pending_to(NULL); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int bb3c_is_cond_jmp(const char *mn)
{
    if (!mn) return 0;
    return (strcmp(mn, "je")  == 0) || (strcmp(mn, "jne") == 0) ||
           (strcmp(mn, "jl")  == 0) || (strcmp(mn, "jge") == 0) ||
           (strcmp(mn, "jg")  == 0) || (strcmp(mn, "jle") == 0) ||
           (strcmp(mn, "jbe") == 0);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_text_3col(FILE *out, const char *label, const char *action, const char *goto_) {
    bb3c_format(out, label, action, goto_);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_text_rawf(const char *fmt, ...) {
    if (bb_emit_mode != EMIT_TEXT) return;
    va_list ap; va_start(ap, fmt); vfprintf(emit_outf(), fmt, ap); va_end(ap);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void  emit_text_stno_banner(int stno, int lineno, const char *src_text)  { emit_banner_stno(stno, lineno, src_text); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_text_global(const char *name) {
    if (!IS_TEXT) return;
    bb3c_flush_pending_cjmp_only();
    fprintf(emit_outf(), "    .global %s\n", name ? name : "");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_seq_brokered_enter(void) {
    insn_push_rbp();
    insn_mov_rbp_rsp();
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_seq_mov_edi_i32(int val) {
    if (bb_emit_mode == EMIT_MACRO_DEF) {
        emit_text_3col(emit_outf(), "", "mov", "edi, \\kind");
        return;
    }
    insn_mov_edi_i32((uint32_t)val);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_seq_cmp_delta_i(int n, bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    insn_mov_eax_r10mem();
    insn_cmp_eax_i32((uint32_t)n);
    emit_jmp(lbl_fail, JMP_JNE);
    emit_jmp(lbl_succ, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_seq_cmp_siglen_delta(int n, uint64_t siglen_addr,
                               bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    if (IS_TEXT) emit_sym_lea_rcx("\xCE\xA3""len", siglen_addr);
    else         insn_mov_rcx_i64(siglen_addr);
    insn_mov_eax_rcxmem();
    insn_sub_eax_i32((uint32_t)n);
    insn_mov_ecx_eax();
    insn_mov_eax_r10mem();
    insn_cmp_eax_ecx();
    emit_jmp(lbl_fail, JMP_JNE);
    emit_jmp(lbl_succ, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_seq_sigma_delta_rdi(uint64_t sigma_addr, uint64_t siglen_addr) {
    (void)siglen_addr;
    if (IS_TEXT) emit_sym_lea_rcx("\xCE\xA3", sigma_addr);
    else         insn_mov_rcx_i64(sigma_addr);
    insn_mov_rax_rcxmem();
    insn_movsxd_rcx_r10mem();
    insn_lea_rax_rax_rcx();
    insn_mov_rdi_rax();
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_seq_bounds_len(int len, uint64_t siglen_addr, bb_label_t *lbl_fail) {
    insn_mov_eax_r10mem();
    insn_add_eax_i32((uint32_t)len);
    if (IS_TEXT) emit_sym_lea_rcx("\xCE\xA3""len", siglen_addr);
    else         insn_mov_rcx_i64(siglen_addr);
    insn_cmp_eax_rcxmem();
    emit_jmp(lbl_fail, JMP_JG);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_seq_noop_macro(const char *macro_name) {
    if (IS_TEXT) emit_text_3col(emit_outf(), "", macro_name, "");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
        emit_text_3col(emit_outf(), "", "sub",  "rsp, 8");
        { char a[80]; snprintf(a,sizeof(a),"rdi, [rip + %s]", fn_name?fn_name:"??");
          emit_text_3col(emit_outf(),"","lea",a); }
        { char a[16]; snprintf(a,sizeof(a),"esi, %d",port);
          emit_text_3col(emit_outf(),"","mov",a); }
        { char a[80]; snprintf(a,sizeof(a),"%s@PLT",fn_name?fn_name:"??");
          emit_text_3col(emit_outf(),"","call",a); }
        emit_text_3col(emit_outf(), "", "add",  "rsp, 8");
        emit_text_3col(emit_outf(), "", "pop", "r10");
        insn_cmp_al_i8(99);
        emit_jmp(lbl_succ, JMP_JNE);
        emit_jmp(lbl_fail, JMP_JMP);
        return;
    }
    insn_push_r10();
    insn_sub_rsp_i8(8);
    insn_mov_rdi_i64(zeta_ptr);
    insn_mov_esi_i32(port);
    insn_call_plt(fn_name, fn_fallback);
    insn_add_rsp_i8(8);
    insn_pop_r10();
    insn_cmp_al_i8(99);
    emit_jmp(lbl_succ, JMP_JNE);
    emit_jmp(lbl_fail, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
        emit_text_3col(emit_outf(),"","sub","rsp, 8");
        { char a[80]; snprintf(a,sizeof(a),"rdi, [rip + %s]",zeta_label?zeta_label:"??");
          emit_text_3col(emit_outf(),"","lea",a); }
        { char a[16]; snprintf(a,sizeof(a),"esi, %d",port);
          emit_text_3col(emit_outf(),"","mov",a); }
        { char a[80]; snprintf(a,sizeof(a),"%s@PLT",fn_name?fn_name:"??");
          emit_text_3col(emit_outf(),"","call",a); }
        emit_text_3col(emit_outf(),"","add","rsp, 8");
        insn_pop_r10();
        insn_cmp_al_i8(99);
        emit_jmp(lbl_succ, JMP_JNE);
        emit_jmp(lbl_fail, JMP_JMP);
        return;
    }
    (void)zeta_label;
    insn_push_r10();
    insn_sub_rsp_i8(8);
    insn_mov_rdi_i64(zeta_ptr);
    insn_mov_esi_i32(port);
    insn_call_plt(fn_name, fn_fallback);
    insn_add_rsp_i8(8);
    insn_pop_r10();
    insn_cmp_al_i8(99);
    emit_jmp(lbl_succ, JMP_JNE);
    emit_jmp(lbl_fail, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define B(b)       bb_emit_byte((uint8_t)(b))
#define U32(v)     bb_emit_u32((uint32_t)(v))
#define U64(v)     bb_emit_u64((uint64_t)(v))
#define T3C(mn,fmt,...)  bb3c_format(emit_outf(),"",mn,fmt)
static void t3 (const char * mn, const char * args)                     { bb3c_format(emit_outf(), "", mn, args); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void tf (const char * mn, const char * fmt, ...) { char buf[64]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap); bb3c_format(emit_outf(), "", mn, buf); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void tj (const char * mn, const char * target)                   { bb3c_emit_jmp(emit_outf(), mn, target); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_eax_i32(uint32_t v) { if (IS_TEXT) tf("mov","eax, %u",v); else { B(0xB8); U32(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rcx_i64(uint64_t v) { if (IS_TEXT) tf("mov","rcx, 0x%llx",(unsigned long long)v); else { B(0x48); B(0xB9); U64(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rdx_i64(uint64_t v) { if (IS_TEXT) tf("mov","rdx, 0x%llx",(unsigned long long)v); else { B(0x48); B(0xBA); U64(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rsi_i64(uint64_t v) { if (IS_TEXT) tf("mov","rsi, 0x%llx",(unsigned long long)v); else { B(0x48); B(0xBE); U64(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rdi_i64(uint64_t v) { if (IS_TEXT) tf("mov","rdi, 0x%llx",(unsigned long long)v); else { B(0x48); B(0xBF); U64(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_edi_i32(uint32_t v) { if (IS_TEXT) tf("mov","edi, %u",v); else { B(0xBF); U32(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_esi_i32(int v) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF)
            t3("mov","esi, \\n");
        else
            tf("mov","esi, %d",v); return;
    }
    B(0xBE); U32((uint32_t)v);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rbp_rsp(void) { if (IS_TEXT) t3("mov","rbp, rsp"); else { B(0x48); B(0x89); B(0xE5); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_ecx_eax(void) { if (IS_TEXT) t3("mov","ecx, eax"); else { B(0x89); B(0xC1); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rdi_rax(void) { if (IS_TEXT) t3("mov","rdi, rax"); else { B(0x48); B(0x89); B(0xC7); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_eax_r10mem(void) { if (IS_TEXT) t3("mov","eax, [r10]"); else { B(0x41); B(0x8B); B(0x02); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_eax_rcxmem(void) { if (IS_TEXT) t3("mov","eax, [rcx]"); else { B(0x8B); B(0x01); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_mov_rax_rcxmem(void) { if (IS_TEXT) t3("mov","rax, [rcx]"); else { B(0x48); B(0x8B); B(0x01); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_lea_rax_rax_rcx(void) { if (IS_TEXT) t3("lea","rax, [rax+rcx]"); else { B(0x48); B(0x8D); B(0x04); B(0x08); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_movsxd_rcx_r10mem(void) { if (IS_TEXT) t3("movsxd","rcx, dword ptr [r10]"); else { B(0x49); B(0x63); B(0x0A); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_cmp_al_i8(uint8_t v) { if (IS_TEXT) tf("cmp","al, %u",(unsigned)v); else { B(0x3C); B(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_cmp_eax_i32(uint32_t v) { if (IS_TEXT) tf("cmp","eax, %u",v); else { B(0x3D); U32(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_cmp_eax_ecx(void) { if (IS_TEXT) t3("cmp","eax, ecx"); else { B(0x39); B(0xC8); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_cmp_eax_rcxmem(void) { if (IS_TEXT) t3("cmp","eax, [rcx]"); else { B(0x3B); B(0x01); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_add_rsp_i8(uint8_t v) { if (IS_TEXT) tf("add","rsp, %u",(unsigned)v); else { B(0x48); B(0x83); B(0xC4); B(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_sub_rsp_i8(uint8_t v) { if (IS_TEXT) tf("sub","rsp, %u",(unsigned)v); else { B(0x48); B(0x83); B(0xEC); B(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_add_eax_i32(uint32_t v) { if (IS_TEXT) tf("add","eax, %u",v); else { B(0x05); U32(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_sub_eax_i32(uint32_t v) { if (IS_TEXT) tf("sub","eax, %u",v); else { B(0x2D); U32(v); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_push_rbp(void) { if (IS_TEXT) t3("push","rbp"); else { B(0x55); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_pop_rbp(void) { if (IS_TEXT) t3("pop","rbp"); else { B(0x5D); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_push_r10(void) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode())
            fmt_body_append("push","r10");
        else
            t3("push","r10"); return;
    }
    B(0x41); B(0x52);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_pop_r10(void) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode())
            fmt_body_append("pop","r10");
        else
            t3("pop","r10"); return;
    }
    B(0x41); B(0x5A);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_call_plt(const char *sym, uint64_t fn_fallback) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        char args[80]; snprintf(args,sizeof(args),"%s@PLT",sym?sym:"??");
        if (emit_bb_is_format_mode())
            fmt_body_append("call",args);
        else
            t3("call",args); return;
    }
    B(0x48); B(0xB8); U64(fn_fallback);
    B(0xFF); B(0xD0);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define INSN_JMP_R8(fn, op) \
    void fn(bb_label_t *t) { \
        if (IS_TEXT) { tj(#op, t->name); return; } \
        B(0x##op); bb_emit_patch_rel8(t); \
    }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define INSN_JCC_R8(fn, mn, op) \
    void fn(bb_label_t *t) { \
        if (IS_TEXT) { tj(mn, t->name); return; } \
        B(0x##op); bb_emit_patch_rel8(t); \
    }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define INSN_JCC_R32(fn, mn, op1, op2) \
    void fn(bb_label_t *t) { \
        if (IS_TEXT) { tj(mn, t->name); return; } \
        B(0x0F); B(0x##op1); bb_emit_patch_rel32(t); \
    }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_jmp_r32(bb_label_t *t) { if (IS_TEXT) tj("jmp", t->name); else { B(0xE9); bb_emit_patch_rel32(t); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_je_r32(bb_label_t *t) { if (IS_TEXT) tj("je", t->name); else { B(0x0F); B(0x84); bb_emit_patch_rel32(t); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_jne_r32(bb_label_t *t) { if (IS_TEXT) tj("jne", t->name); else { B(0x0F); B(0x85); bb_emit_patch_rel32(t); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_jl_r32(bb_label_t *t) { if (IS_TEXT) tj("jl", t->name); else { B(0x0F); B(0x8C); bb_emit_patch_rel32(t); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_jge_r32(bb_label_t *t) { if (IS_TEXT) tj("jge", t->name); else { B(0x0F); B(0x8D); bb_emit_patch_rel32(t); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void insn_jg_r32(bb_label_t *t) { if (IS_TEXT) tj("jg", t->name); else { B(0x0F); B(0x8F); bb_emit_patch_rel32(t); } }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
        else
            bb3c_format(emit_outf(), "", "push", "r10"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
        else
            bb3c_format(emit_outf(), "", "pop", "r10"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-3 JVM scalar helpers — promoted from static in emit_jvm.c; used by SM_templates. */
void jvm_push_int2(FILE * out, long v) {
    if (v == -1) { fprintf(out, "    iconst_m1\n"); return; }
    if (v >= 0 && v <= 5) { fprintf(out, "    iconst_%ld\n", v); return; }
    if (v >= -128 && v <= 127) { fprintf(out, "    bipush %ld\n", v); return; }
    if (v >= -32768 && v <= 32767) { fprintf(out, "    sipush %ld\n", v); return; }
    fprintf(out, "    ldc %ld\n", v);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void jvm_emit_ldc_string(FILE * out, const char * s) {
    fprintf(out, "    ldc \"");
    for (const char * p = s; *p; p++) {
        if      (*p == '"')  fprintf(out, "\\\"");
        else if (*p == '\\') fprintf(out, "\\\\");
        else if (*p == '\n') fprintf(out, "\\n");
        else if (*p == '\r') fprintf(out, "\\r");
        else if (*p == '\t') fprintf(out, "\\t");
        else                 fputc(*p, out);
    }
    fprintf(out, "\"\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2 helpers — shared across JVM/JS/.NET arms; removed from silos in EC-5. */
#include "BB.h"
#include "emit_ir.h"
void js_escape(FILE * out, const char * s) {
    fprintf(out, "\"");
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if      (c == '"')  fprintf(out, "\\\"");
        else if (c == '\\') fprintf(out, "\\\\");
        else if (c == '\n') fprintf(out, "\\n");
        else if (c == '\r') fprintf(out, "\\r");
        else if (c == '\t') fprintf(out, "\\t");
        else if (c < 0x20 || c > 0x7e) fprintf(out, "\\x%02x", c);
        else fprintf(out, "%c", c);
    }
    fprintf(out, "\"");
}
void jvm_class_hdr(FILE * out, const char * name) {
    fprintf(out, ".class public bb/bb_%s\n", name);
    fprintf(out, ".super bb/bb_box\n");
    fprintf(out, ".inner class public static final spec inner bb/bb_box$Spec outer bb/bb_box\n");
    fprintf(out, ".inner class public static final matchstate inner bb/bb_box$MatchState outer bb/bb_box\n");
}
void net_escape_ldstr(FILE * out, const char * s) {
    fprintf(out, "    ldstr      \"");
    if (!s) { fprintf(out, "\"\n"); return; }
    for (const unsigned char * p = (const unsigned char *)s; *p; p++) {
        if (*p == '"')       fprintf(out, "\\\"");
        else if (*p == '\\') fprintf(out, "\\\\");
        else if (*p < 0x20 || *p == 0x7f) fprintf(out, "\\u%04X", (unsigned)*p);
        else fputc(*p, out);
    }
    fprintf(out, "\"\n");
}
void net_class_hdr(FILE * out, int sid, int nid) {
    fprintf(out, ".class nested public auto ansi beforefieldinit pat_%d_%d\n", sid, nid);
    fprintf(out, "       extends [mscorlib]System.Object\n");
    fprintf(out, "       implements [boxes]Snobol4.Runtime.Boxes.IByrdBox\n{\n");
}
void net_alpha_hdr(FILE * out) {
    fprintf(out, "  .method public virtual instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec\n");
    fprintf(out, "          Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState ms) cil managed\n  {\n");
}
void net_beta_hdr(FILE * out) {
    fprintf(out, "  .method public virtual instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec\n");
    fprintf(out, "          Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState ms) cil managed\n  {\n");
}
void net_fail_ret(FILE * out) {
    fprintf(out, "    ldsfld     valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.Spec::Fail\n");
    fprintf(out, "    ret\n");
}
void net_cursor_load(FILE * out) {
    fprintf(out, "    ldarg.1\n");
    fprintf(out, "    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
}
void net_ms_length(FILE * out) {
    fprintf(out, "    callvirt   instance int32 [boxes]Snobol4.Runtime.Boxes.MatchState::get_Length()\n");
}
void net_spec_of(FILE * out) {
    fprintf(out, "    call       valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.Spec::Of(int32, int32)\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2 charset helpers — used by ANY, NOTANY, SPAN, BREAK. */
void jvm_init_ms_str(FILE * out, const char * name, const char * field) {
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Ljava/lang/String;)V\n");
    fprintf(out, "    .limit stack 3\n    .limit locals 3\n");
    fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
    fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_%s/%s Ljava/lang/String;\n", name, field);
    fprintf(out, "    return\n.end method\n");
}
void net_charset_class(FILE * out, int sid, int nid, const char * tag) {
    net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private string _chars\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(string chars) cil managed\n  {\n");
    fprintf(out, "    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    dup\n    brtrue     %s_%d_%d_NN\n    pop\n    ldstr      \"\"\n", tag, sid, nid);
    fprintf(out, "  %s_%d_%d_NN:\n    stfld      string pat_%d_%d::_chars\n    ret\n  }\n", tag, sid, nid, sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b helpers — used by ARB, ARBNO, CAT, ALT, LEN, POS/RPOS, TAB/RTAB, REM, FENCE, ABORT, ASSIGN_*. */
void jvm_init_ms_only(FILE * out, const char * name) {
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;)V\n    .limit stack 2\n    .limit locals 2\n    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n    return\n.end method\n");
    (void)name;
}
void jvm_init_ms_int(FILE * out, const char * name, const char * field) {
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;I)V\n    .limit stack 3\n    .limit locals 3\n    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n    aload_0\n    iload_2\n    putfield bb/bb_%s/%s I\n    aload_0\n    aconst_null\n    putfield bb/bb_%s/dyn Ljava/util/function/IntSupplier;\n    return\n.end method\n", name, field, name);
}
void jvm_val_helper(FILE * out, const char * name) {
    fprintf(out, ".method private val()I\n    .limit stack 2\n    .limit locals 1\n    aload_0\n    getfield bb/bb_%s/dyn Ljava/util/function/IntSupplier;\n    ifnull %s_val_static\n    aload_0\n    getfield bb/bb_%s/dyn Ljava/util/function/IntSupplier;\n    invokeinterface java/util/function/IntSupplier/getAsInt()I 1\n    ireturn\n%s_val_static:\n    aload_0\n    getfield bb/bb_%s/n I\n    ireturn\n.end method\n", name, name, name, name, name);
}
void net_push_i4(FILE * out, int v) {
    if (v >= 0 && v <= 8)          { fprintf(out, "    ldc.i4.%d\n", v); }
    else if (v == -1)               { fprintf(out, "    ldc.i4.m1\n"); }
    else if (v >= -128 && v <= 127) { fprintf(out, "    ldc.i4.s   %d\n", v); }
    else                            { fprintf(out, "    ldc.i4     %d\n", v); }
}
void net_ctor_none(FILE * out, int sid, int nid) {
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor() cil managed\n  {\n    .maxstack 1\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n    ret\n  }\n");
    (void)sid; (void)nid;
}
void net_spec_zw(FILE * out) {
    fprintf(out, "    call       valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.Spec::ZeroWidth(int32)\n");
}

/* emit_bb_node — one call per BB kind; each bb_* function handles all modes internally.
   EC-UNI-10(c): templates are parameterless and read g_emit.node / g_emit.out.  bb_capture
   keeps `int imm` as a per-call-site discriminator (two BB kinds map to it). */
int emit_bb_node(BB_t * nd, FILE * out) {
    if (!nd) return 1;
    g_emit.node = nd;
    g_emit.out  = out;
    g_emit.sid  = 0;
    g_emit.nid  = bb_node_id(nd);
    switch (nd->t) {
    case BB_PAT_LIT:         bb_lit();          return 0;
    case BB_PAT_ANY:         bb_any();          return 0;
    case BB_PAT_NOTANY:      bb_notany();       return 0;
    case BB_PAT_SPAN:        bb_span();         return 0;
    case BB_PAT_BREAK:       bb_break();        return 0;
    case BB_PAT_ARB:         bb_arb();          return 0;
    case BB_PAT_ARBNO:       bb_arbno();        return 0;
    case BB_PAT_CAT:         bb_cat();          return 0;
    case BB_PAT_ALT:         bb_alt();          return 0;
    case BB_PAT_LEN:         bb_len();          return 0;
    case BB_PAT_POS:         bb_pos();          return 0;
    case BB_PAT_TAB:         bb_tab();          return 0;
    case BB_PAT_REM:         bb_rem();          return 0;
    case BB_PAT_FENCE:       bb_fence();        return 0;
    case BB_PAT_ABORT:       bb_abort();        return 0;
    case BB_PAT_ASSIGN_IMM:  bb_capture(1);     return 0;
    case BB_PAT_ASSIGN_COND: bb_capture(0);     return 0;
    default:
        fprintf(out, "; [emit_bb_node: kind=%d unhandled]\n", (int)nd->t);
        return 1;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-6: WASM string table — deduplicate literal strings into (data ...) segments. */
#define WASM_STRTAB_MAX 4096
#define WASM_STR_DATA_BASE 0x100000
typedef struct { const char * s; int addr; int len; } WasmStrEntry;
static WasmStrEntry g_wasm_strtab[WASM_STRTAB_MAX];
static int g_wasm_strtab_n = 0;
static int g_wasm_str_next = WASM_STR_DATA_BASE;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void wasm_strtab_reset(void) { g_wasm_strtab_n = 0; g_wasm_str_next = WASM_STR_DATA_BASE; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int wasm_intern_str(const char * s) {
    int len = s ? (int)strlen(s) : 0;
    for (int i = 0; i < g_wasm_strtab_n; i++)
        if (g_wasm_strtab[i].len == len && (len == 0 || memcmp(g_wasm_strtab[i].s, s, len) == 0)) return g_wasm_strtab[i].addr;
    if (g_wasm_strtab_n >= WASM_STRTAB_MAX) return WASM_STR_DATA_BASE;
    int addr = g_wasm_str_next;
    g_wasm_strtab[g_wasm_strtab_n].s    = s;
    g_wasm_strtab[g_wasm_strtab_n].addr = addr;
    g_wasm_strtab[g_wasm_strtab_n].len  = len;
    g_wasm_strtab_n++;
    g_wasm_str_next += len + 1;
    return addr;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int wasm_intern_name(const char * s) {
    if (!s) return wasm_intern_str(s);
    int len = (int)strlen(s);
    static char buf[256];
    if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        buf[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : (char)c;
    }
    buf[len] = '\0';
    return wasm_intern_str(buf);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void wasm_emit_data_segments(FILE * out) {
    for (int i = 0; i < g_wasm_strtab_n; i++) {
        const char * s   = g_wasm_strtab[i].s;
        int          len = g_wasm_strtab[i].len;
        int          adr = g_wasm_strtab[i].addr;
        fprintf(out, "  (data (i32.const 0x%x) \"", adr);
        for (int j = 0; j < len; j++) {
            unsigned char c = (unsigned char)s[j];
            if (c == '"' || c == '\\') fprintf(out, "\\%02x", (unsigned)c);
            else if (c < 32 || c > 126) fprintf(out, "\\%02x", (unsigned)c);
            else fputc((int)c, out);
        }
        fprintf(out, "\\00\")  ;; len=%d\n", len);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define WASM_USERFNS_MAX 256
#define WASM_MAX_PARAMS  16
typedef struct { char name[128]; int entry_pc; int nparams; char params[WASM_MAX_PARAMS][128]; } WasmUserFn;
static WasmUserFn g_wasm_userfns[WASM_USERFNS_MAX];
static int        g_wasm_userfns_n = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void wasm_userfns_reset(void) { g_wasm_userfns_n = 0; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-UNI-13(b): exposed for SM_templates/sm_calls.c verbatim arm. */
WasmUserFn * wasm_userfn_find(const char * name) {
    if (!name) return NULL;
    for (int i = 0; i < g_wasm_userfns_n; i++) if (strcmp(g_wasm_userfns[i].name, name) == 0) return &g_wasm_userfns[i];
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int wasm_parse_define_signature(WasmUserFn * fn, const char * sig) {
    if (!sig) return 0;
    const char * lp = strchr(sig, '(');
    const char * rp = lp ? strchr(lp + 1, ')') : NULL;
    if (!lp) { snprintf(fn->name, sizeof(fn->name), "%s", sig); fn->nparams = 0; return 1; }
    int nlen = (int)(lp - sig); if (nlen >= (int)sizeof(fn->name)) nlen = (int)sizeof(fn->name) - 1;
    memcpy(fn->name, sig, (size_t)nlen); fn->name[nlen] = '\0';
    fn->nparams = 0;
    if (!rp || rp <= lp + 1) return 1;
    const char * s = lp + 1;
    while (s < rp && fn->nparams < WASM_MAX_PARAMS) {
        while (s < rp && (*s == ' ' || *s == '\t' || *s == ',')) s++;
        const char * ps = s;
        while (s < rp && *s != ',' && *s != ' ' && *s != '\t') s++;
        int plen = (int)(s - ps); if (plen <= 0) continue;
        if (plen >= (int)sizeof(fn->params[0])) plen = (int)sizeof(fn->params[0]) - 1;
        memcpy(fn->params[fn->nparams], ps, (size_t)plen); fn->params[fn->nparams][plen] = '\0';
        fn->nparams++;
    }
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void wasm_pre_scan_userfns(SM_sequence_t * sm) {
    for (int i = 0; i < sm->count && g_wasm_userfns_n < WASM_USERFNS_MAX; i++) {
        SM_t * ins = &sm->instrs[i];
        if (ins->op != SM_LABEL) continue;
        const char * lbl = ins->a[0].s;
        if (!lbl || !strstr(lbl, "(")) continue;
        WasmUserFn * fn = &g_wasm_userfns[g_wasm_userfns_n++];
        memset(fn, 0, sizeof(*fn));
        fn->entry_pc = i;
        wasm_parse_define_signature(fn, lbl);
        wasm_intern_name(fn->name);
        for (int k = 0; k < fn->nparams; k++) wasm_intern_name(fn->params[k]);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-UNI-14(a): emit_sm_dispatch — shared opcode→template dispatcher used by the WASM, JS, and
 * NET silo walkers.  Reads g_emit.instr->op and calls the matching sm_<name>() template.  Returns
 * 1 if the template emitted its own PC transfer (jump / return / suspend / call-with-jump),
 * 0 otherwise (caller continues with per-PC postamble: WASM `(local.set $pc i+1) (br $lp)`,
 * JS `_pc = i+1; continue;`, NET `stloc _pc; br NET_DISPATCH`).
 *
 * Backend-specific quirks NOT covered here — caller handles BEFORE invoking:
 *   - SM_LABEL: WASM/JS no-op (drop into dispatch returns 0); NET inlines frame_push +
 *     frame_save + param-bind for function-entry labels and must override.
 *   - SM_EXEC_STMT: WASM/JS call sm_exec_stmt() (covered here); NET inlines a fixed
 *     stack-cleanup + set_omega/set_sigma/set_delta/set_last_ok sequence and must override.
 *   - SM_PUSH_EXPRESSION: JS emits literal `rt.push_null();` inline; WASM/NET don't
 *     handle this opcode at all.
 *   - SM_INCR / SM_DECR: WASM treats as aliases for SM_ADD / SM_SUB respectively; JS/NET
 *     don't handle them.
 *   - SM_PUSH_NULL_NOFLIP: shared across all three — bundled with SM_PUSH_NULL.
 *
 * Per-instruction g_emit fields (g_emit.i, g_emit.n, g_emit.instr, sidecars) must be set by
 * the caller before invoking.  g_emit.prog and g_emit.srclines are set once at emit_program
 * entry; the per-instruction fields turn over inside the silo walkers' loops. */
int emit_sm_dispatch(void) {
    const SM_t *instr = g_emit.instr;
    switch (instr->op) {
        /* No-effect cases — uniform default-handled by WASM/JS, overridden by NET upstream. */
        case SM_LABEL:                                                                   return 0;
        /* Push/pop literals and variables. */
        case SM_PUSH_LIT_I:           sm_push_lit_i();                                   return 0;
        case SM_PUSH_LIT_S:
        case SM_PUSH_LIT_CS:          sm_push_lit_s();                                   return 0;
        case SM_PUSH_LIT_F:           sm_push_lit_f();                                   return 0;
        case SM_PUSH_NULL:
        case SM_PUSH_NULL_NOFLIP:     sm_push_null();                                    return 0;
        case SM_PUSH_VAR:             sm_push_var();                                     return 0;
        case SM_STORE_VAR:            sm_store_var();                                    return 0;
        case SM_VOID_POP:             sm_void_pop();                                     return 0;
        /* Arithmetic / coercion. */
        case SM_CONCAT:               sm_concat();                                       return 0;
        case SM_NEG:                  sm_neg();                                          return 0;
        case SM_COERCE_NUM:           sm_coerce_num();                                   return 0;
        case SM_EXP:                  sm_exp();                                          return 0;
        case SM_ADD:                  sm_add();                                          return 0;
        case SM_SUB:                  sm_sub();                                          return 0;
        case SM_MUL:                  sm_mul();                                          return 0;
        case SM_DIV:                  sm_div();                                          return 0;
        case SM_MOD:                  sm_mod();                                          return 0;
        case SM_LCOMP:                sm_lcomp();                                        return 0;
        case SM_ACOMP:                sm_acomp();                                        return 0;
        case SM_STNO:                 sm_stno();                                         return 0;
        /* Control flow — these CAN take control of PC; bubble template return value up. */
        case SM_HALT:                                                          return sm_halt();
        case SM_JUMP:                                                          return sm_jump();
        case SM_JUMP_S:                                                        return sm_jump_s();
        case SM_JUMP_F:                                                        return sm_jump_f();
        case SM_CALL_FN:                                                       return sm_call_fn();
        case SM_SUSPEND_VALUE:                                                 return sm_suspend_value();
        case SM_RETURN: case SM_RETURN_S:  case SM_RETURN_F:                   return sm_return();
        case SM_FRETURN: case SM_FRETURN_S: case SM_FRETURN_F:                 return sm_freturn();
        case SM_NRETURN: case SM_NRETURN_S: case SM_NRETURN_F:                 return sm_nreturn();
        /* DEFINE bracket. */
        case SM_DEFINE_ENTRY:         sm_define_entry();                                 return 0;
        case SM_DEFINE:               sm_define();                                       return 0;
        /* Patterns. */
        case SM_PAT_LIT:              sm_pat_lit();                                      return 0;
        case SM_PAT_ANY:              sm_pat_any_i();                                    return 0;
        case SM_PAT_NOTANY:           sm_pat_notany();                                   return 0;
        case SM_PAT_SPAN:             sm_pat_span();                                     return 0;
        case SM_PAT_BREAK:            sm_pat_break();                                    return 0;
        case SM_PAT_LEN:              sm_pat_len();                                      return 0;
        case SM_PAT_POS:              sm_pat_pos();                                      return 0;
        case SM_PAT_RPOS:             sm_pat_rpos();                                     return 0;
        case SM_PAT_TAB:              sm_pat_tab();                                      return 0;
        case SM_PAT_RTAB:             sm_pat_rtab();                                     return 0;
        case SM_PAT_ARB:              sm_pat_arb();                                      return 0;
        case SM_PAT_ARBNO:            sm_pat_arbno();                                    return 0;
        case SM_PAT_REM:              sm_pat_rem();                                      return 0;
        case SM_PAT_BAL:              sm_pat_bal();                                      return 0;
        case SM_PAT_FENCE0:           sm_pat_fence0();                                   return 0;
        case SM_PAT_FENCE1:           sm_pat_fence1();                                   return 0;
        case SM_PAT_FAIL:             sm_pat_fail();                                     return 0;
        case SM_PAT_SUCCEED:          sm_pat_succeed();                                  return 0;
        case SM_PAT_ABORT:            sm_pat_abort();                                    return 0;
        case SM_PAT_EPS:              sm_pat_eps();                                      return 0;
        case SM_PAT_CAT:              sm_pat_cat();                                      return 0;
        case SM_PAT_ALT:              sm_pat_alt();                                      return 0;
        case SM_PAT_DEREF:            sm_pat_deref();                                    return 0;
        case SM_PAT_REFNAME:          sm_pat_refname();                                  return 0;
        case SM_PAT_CAPTURE:          sm_pat_capture();                                  return 0;
        case SM_PAT_CAPTURE_FN:       sm_pat_capture_fn();                               return 0;
        case SM_PAT_CAPTURE_FN_ARGS:  sm_pat_capture_fn_args();                          return 0;
        case SM_PAT_USERCALL:         sm_pat_usercall();                                 return 0;
        case SM_PAT_USERCALL_ARGS:    sm_pat_usercall_args();                            return 0;
        /* EXEC_STMT — WASM/JS via template; NET caller must override before reaching here. */
        case SM_EXEC_STMT:            sm_exec_stmt();                                    return 0;
        default:                                                                         return 0;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-UNI-14(a): sm_op_is_dispatched — returns 1 if emit_sm_dispatch covers this opcode, 0 if it
 * silently falls through.  Used by silo walkers that want to annotate truly unhandled opcodes
 * differently from the dispatcher's no-effect cases (e.g., SM_LABEL). */
int sm_op_is_dispatched(SM_op_t op) {
    switch (op) {
        case SM_LABEL: case SM_PUSH_LIT_I: case SM_PUSH_LIT_S: case SM_PUSH_LIT_CS:
        case SM_PUSH_LIT_F: case SM_PUSH_NULL: case SM_PUSH_NULL_NOFLIP:
        case SM_PUSH_VAR: case SM_STORE_VAR: case SM_VOID_POP:
        case SM_CONCAT: case SM_NEG: case SM_COERCE_NUM: case SM_EXP:
        case SM_ADD: case SM_SUB: case SM_MUL: case SM_DIV: case SM_MOD:
        case SM_LCOMP: case SM_ACOMP: case SM_STNO:
        case SM_HALT: case SM_JUMP: case SM_JUMP_S: case SM_JUMP_F:
        case SM_CALL_FN: case SM_SUSPEND_VALUE:
        case SM_RETURN: case SM_RETURN_S: case SM_RETURN_F:
        case SM_FRETURN: case SM_FRETURN_S: case SM_FRETURN_F:
        case SM_NRETURN: case SM_NRETURN_S: case SM_NRETURN_F:
        case SM_DEFINE_ENTRY: case SM_DEFINE:
        case SM_PAT_LIT: case SM_PAT_ANY: case SM_PAT_NOTANY: case SM_PAT_SPAN:
        case SM_PAT_BREAK: case SM_PAT_LEN: case SM_PAT_POS: case SM_PAT_RPOS:
        case SM_PAT_TAB: case SM_PAT_RTAB: case SM_PAT_ARB: case SM_PAT_ARBNO:
        case SM_PAT_REM: case SM_PAT_BAL: case SM_PAT_FENCE0: case SM_PAT_FENCE1:
        case SM_PAT_FAIL: case SM_PAT_SUCCEED: case SM_PAT_ABORT: case SM_PAT_EPS:
        case SM_PAT_CAT: case SM_PAT_ALT: case SM_PAT_DEREF: case SM_PAT_REFNAME:
        case SM_PAT_CAPTURE: case SM_PAT_CAPTURE_FN: case SM_PAT_CAPTURE_FN_ARGS:
        case SM_PAT_USERCALL: case SM_PAT_USERCALL_ARGS:
        case SM_EXEC_STMT:
            return 1;
        default:
            return 0;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-WASM-SM: emit_wasm_from_sm — per-PC `(if pc==N)` wrapper, body via emit_sm_dispatch.
 * EC-UNI-14(a): the 50+ arm switch collapsed into emit_sm_dispatch.  WASM-only overrides
 * stay here: SM_INCR / SM_DECR alias to sm_add / sm_sub (WASM treats INCR/DECR as ADD/SUB
 * since the rt only exposes the two), and the `;; unhandled` annotation for unknown opcodes
 * (where the dispatcher silently returns 0). */
static int emit_wasm_from_sm(SM_sequence_t * sm, FILE * out) {
    if (!sm || !out || sm->count == 0) return 0;
    int n = sm->count;
    fprintf(out, "    (block $done\n");
    fprintf(out, "      (loop $lp\n");
    for (int i = 0; i < n; i++) {
        SM_t * ins = &sm->instrs[i];
        int has_jump = 0;
        g_emit.i = i; g_emit.n = n; g_emit.instr = ins;
        g_emit.in_body = 0; g_emit.in_my_method = NULL;
        g_emit.pc_to_fn = NULL; g_emit.fn_names = NULL; g_emit.fn_pcs = NULL; g_emit.fn_count = 0;
        fprintf(out, "        (if (i32.eq (local.get $pc) (i32.const %d)) (then\n", i);
        /* WASM-only overrides: INCR/DECR alias to ADD/SUB.  Note: this is a runtime-fidelity
         * gap (no SM_INCR/SM_DECR semantic — they'd be ADD/SUB with arg=1).  The legacy switch
         * had this behavior; keep it bit-for-bit until WASM rt grows real INCR/DECR. */
        if (ins->op == SM_INCR)        { sm_add(); }
        else if (ins->op == SM_DECR)   { sm_sub(); }
        else {
            has_jump = emit_sm_dispatch();
            /* Annotation for opcodes the dispatcher silently passes through with no template
             * (currently: only the no-effect SM_LABEL path, plus any future opcode lacking a
             * template).  Detect by checking the dispatcher's coverage explicitly. */
            if (ins->op != SM_LABEL && !sm_op_is_dispatched(ins->op))
                fprintf(out, "          ;; unhandled SM opcode %d\n", ins->op);
        }
        if (!has_jump) fprintf(out, "          (i32.const %d) (local.set $pc) (br $lp)\n", i + 1);
        fprintf(out, "        ))\n");
    }
    fprintf(out, "        (br $done)\n");
    fprintf(out, "      ) ;; end loop $lp\n");
    fprintf(out, "    ) ;; end block $done\n");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-4: unified emit_prologue / emit_epilogue — mode-dispatched, replaces per-silo static fns. */
int emit_prologue(BB_graph_t * cfg, FILE * out) {
    (void)cfg;
    if (IS_JVM) {
        fprintf(out, "; JVM Jasmin output — generated by scrip --jit-emit --target=jvm\n");
        fprintf(out, ".class public Prog\n");
        fprintf(out, ".super java/lang/Object\n");
        fprintf(out, ".method public static main([Ljava/lang/String;)V\n");
        fprintf(out, "    .limit stack 4\n");
        fprintf(out, "    .limit locals 2\n");
        fprintf(out, "    invokestatic rt/SnoRt/init()V\n");
        fprintf(out, "    invokestatic Prog/sno_body()V\n");
        fprintf(out, "    invokestatic rt/SnoRt/finalize_rt()I\n");
        fprintf(out, "    pop\n");
        fprintf(out, "    return\n");
        fprintf(out, ".end method\n");
        return 0;
    }
    if (IS_JS) {
        fprintf(out, "'use strict';\n");
        fprintf(out, "const rt = require('/home/claude/one4all/src/runtime/js/sno_runtime.js');\n");
        fprintf(out, "rt._init();\n");
        fprintf(out, "let _pc = 0;\n");
        fprintf(out, "loop: while (true) { switch (_pc) {\n");
        return 0;
    }
    if (IS_NET) {
        fprintf(out, "// .NET MSIL output -- generated by scrip --sm-emit --target=net\n");
        fprintf(out, ".assembly extern mscorlib {}\n");
        fprintf(out, ".assembly extern boxes {}\n");
        fprintf(out, ".assembly Prog {}\n");
        fprintf(out, ".module Prog.exe\n");
        fprintf(out, ".class public auto ansi beforefieldinit Prog extends [mscorlib]System.Object\n{\n");
        fprintf(out, "  .method public static void Main() cil managed\n  {\n");
        fprintf(out, "    .entrypoint\n    .maxstack 8\n");
        fprintf(out, "    call       void SnoRt::_init()\n");
        return 0;
    }
    if (IS_WASM) {
        fprintf(out, "(module\n");
        fprintf(out, "  ;; imports from sno_runtime\n");
        fprintf(out, "  (import \"sno\" \"memory\"         (memory 64))\n");
        fprintf(out, "  (import \"sno\" \"sno_init\"        (func $sno_init))\n");
        fprintf(out, "  (import \"sno\" \"sno_finalize\"    (func $sno_finalize))\n");
        fprintf(out, "  (import \"sno\" \"sno_push_int\"    (func $sno_push_int    (param i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_push_real\"   (func $sno_push_real   (param f64)))\n");
        fprintf(out, "  (import \"sno\" \"sno_push_str\"    (func $sno_push_str    (param i32 i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_push_null\"   (func $sno_push_null))\n");
        fprintf(out, "  (import \"sno\" \"sno_push_var\"    (func $sno_push_var    (param i32 i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_store_var\"   (func $sno_store_var   (param i32 i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_pop_void\"    (func $sno_pop_void))\n");
        fprintf(out, "  (import \"sno\" \"sno_concat\"      (func $sno_concat))\n");
        fprintf(out, "  (import \"sno\" \"sno_neg\"         (func $sno_neg))\n");
        fprintf(out, "  (import \"sno\" \"sno_coerce_num\"  (func $sno_coerce_num))\n");
        fprintf(out, "  (import \"sno\" \"sno_exp_op\"      (func $sno_exp_op))\n");
        fprintf(out, "  (import \"sno\" \"sno_arith\"       (func $sno_arith       (param i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_lcomp\"       (func $sno_lcomp       (param i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_acomp\"       (func $sno_acomp       (param i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_last_ok\"     (func $sno_last_ok     (result i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_set_stno\"    (func $sno_set_stno    (param i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_halt_tos\"    (func $sno_halt_tos))\n");
        fprintf(out, "  (import \"sno\" \"sno_call\"        (func $sno_call        (param i32 i32 i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_fn_return\"   (func $sno_fn_return   (param i32 i32) (result i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_call_frame_push\" (func $sno_call_frame_push (param i32 i32 i32) (result i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_call_frame_close\" (func $sno_call_frame_close))\n");
        fprintf(out, "  (import \"sno\" \"sno_save_var\"    (func $sno_save_var    (param i32 i32 i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_clear_var\"   (func $sno_clear_var   (param i32 i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_set_var_from_tos\" (func $sno_set_var_from_tos (param i32 i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_pop_to_null\" (func $sno_pop_to_null))\n");
        fprintf(out, "  ;; Pattern matching functions\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_lit\"     (func $sno_pat_lit     (param i32 i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_any\"     (func $sno_pat_any))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_notany\"  (func $sno_pat_notany))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_span\"    (func $sno_pat_span))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_break\"   (func $sno_pat_break))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_len\"     (func $sno_pat_len))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_pos\"     (func $sno_pat_pos))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_rpos\"    (func $sno_pat_rpos))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_tab\"     (func $sno_pat_tab))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_rtab\"    (func $sno_pat_rtab))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_rem\"     (func $sno_pat_rem))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_arb\"     (func $sno_pat_arb))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_arbno\"   (func $sno_pat_arbno))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_bal\"     (func $sno_pat_bal))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_fail\"    (func $sno_pat_fail))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_succeed\" (func $sno_pat_succeed))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_abort\"   (func $sno_pat_abort))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_fence\"   (func $sno_pat_fence))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_eps\"     (func $sno_pat_eps))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_cat\"     (func $sno_pat_cat))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_alt\"     (func $sno_pat_alt))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_deref\"   (func $sno_pat_deref))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_refname\" (func $sno_pat_refname (param i32 i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_pat_capture\" (func $sno_pat_capture (param i32 i32 i32)))\n");
        fprintf(out, "  (import \"sno\" \"sno_exec_stmt\"   (func $sno_exec_stmt   (param i32 i32 i32)))\n");
        return 0;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int emit_epilogue(BB_graph_t * cfg, FILE * out) {
    (void)cfg;
    if (IS_JVM) { return 0; }
    if (IS_JS) {
        fprintf(out, "default: break loop;\n");
        fprintf(out, "}} rt._finalize();\n");
        return 0;
    }
    if (IS_NET) {
        fprintf(out, "    call       void SnoRt::_finalize()\n");
        fprintf(out, "    ret\n  }\n}\n");
        return 0;
    }
    if (IS_WASM) {
        wasm_emit_data_segments(out);
        fprintf(out, ")\n");
        return 0;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-5: jvm_sanitize_name — moved from emit_jvm.c. */
/* EC-UNI-13(b): exposed for SM_templates/sm_calls.c verbatim arm. */
void jvm_sanitize_name(char * dst, size_t dsz, const char * src) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < dsz; i++) {
        char c = src[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') dst[j++] = c;
        else dst[j++] = '_';
    }
    if (j == 0 && j + 4 < dsz) { dst[j++] = 's'; dst[j++] = 'n'; dst[j++] = 'o'; }
    dst[j] = '\0';
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-5: net_parse_define_proto — moved from emit_net.c. */
static char ** net_parse_define_proto(const char * proto, char ** out_fname, int * out_n) {
    *out_fname = NULL; *out_n = 0;
    if (!proto) return NULL;
    const char * lp = strchr(proto, '(');
    const char * rp = lp ? strchr(lp, ')') : NULL;
    if (!lp) {
        size_t flen = strlen(proto); char * fn = (char *)malloc(flen + 1);
        memcpy(fn, proto, flen); fn[flen] = '\0'; *out_fname = fn; return NULL;
    }
    size_t flen = (size_t)(lp - proto); char * fn = (char *)malloc(flen + 1);
    memcpy(fn, proto, flen); fn[flen] = '\0'; *out_fname = fn;
    if (!rp || rp <= lp + 1) return NULL;
    int cap = 4, count = 0;
    char ** params = (char **)malloc((size_t)(cap + 1) * sizeof(char *));
    const char * s = lp + 1;
    while (s < rp) {
        while (s < rp && (*s == ' ' || *s == '\t')) s++;
        const char * pstart = s;
        while (s < rp && *s != ',' && *s != ' ' && *s != '\t') s++;
        size_t plen = (size_t)(s - pstart);
        if (plen > 0) {
            if (count >= cap) { cap *= 2; params = (char **)realloc(params, (size_t)(cap + 1) * sizeof(char *)); }
            char * p = (char *)malloc(plen + 1); memcpy(p, pstart, plen); p[plen] = '\0'; params[count++] = p;
        }
        while (s < rp && (*s == ' ' || *s == '\t')) s++;
        if (s < rp && *s == ',') s++;
    }
    params[count] = NULL; *out_n = count; return params;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-5: bb_node_id / bb_is_generator / bb_walk — moved from emit_ir.c. */
int bb_node_id(BB_t * nd) { return (int)((uintptr_t)nd % 100000u); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int bb_is_generator(BB_op_t k) {
    if (k >= BB_PAT_LIT   && k <= BB_PAT_CALLOUT)  return 1;
    if (k >= BB_PL_CHOICE && k <= BB_PL_CALL)      return 1;
    if (k >= BB_ICN_TO    && k <= BB_ICN_PROC_GEN) return 1;
    if (k == BB_SCAN || k == BB_ALTERNATE || k == BB_TO_BY ||
        k == BB_EVERY || k == BB_WHILE    || k == BB_LIMIT || k == BB_SUSPEND) return 1;
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define IR_WALK_MAX 4096
static int g_visited[IR_WALK_MAX];
static int g_vcount = 0;
static void bb_walk_rec(BB_t * nd, void (*visit)(BB_t *, void *), void * ctx) {
    if (!nd) return;
    int id = bb_node_id(nd);
    for (int i = 0; i < g_vcount; i++) if (g_visited[i] == id) return;
    if (g_vcount < IR_WALK_MAX) g_visited[g_vcount++] = id;
    visit(nd, ctx);
    bb_walk_rec(nd->α, visit, ctx); bb_walk_rec(nd->β, visit, ctx);
    bb_walk_rec(nd->γ, visit, ctx); bb_walk_rec(nd->ω, visit, ctx);
    if (nd->c) for (int i = 0; i < nd->n; i++) bb_walk_rec(nd->c[i], visit, ctx);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_walk(BB_graph_t * cfg, void (*visit)(BB_t *, void *), void * ctx) {
    if (!cfg || !cfg->entry) return;
    g_vcount = 0;
    bb_walk_rec(cfg->entry, visit, ctx);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-5: emit_jvm_from_sm — moved from emit_jvm.c (method-split SM walk). */
static void emit_jvm_one_instr(SM_sequence_t * sm, int i, int n, const char ** fn_names, const int * fn_pcs, int fn_count, int in_body, const char * in_my_method, FILE * out) {
    SM_t * instr = &sm->instrs[i];
    g_emit.i = i; g_emit.n = n; g_emit.instr = instr;
    g_emit.in_body = in_body; g_emit.in_my_method = in_my_method;
    g_emit.pc_to_fn = NULL; g_emit.fn_names = fn_names; g_emit.fn_pcs = fn_pcs; g_emit.fn_count = fn_count;
    switch (instr->op) {
    case SM_LABEL: break;
    case SM_STNO:  sm_stno(); break;
    case SM_PUSH_LIT_I: sm_push_lit_i(); break;
    case SM_PUSH_LIT_S: sm_push_lit_s(); break;
    case SM_PUSH_LIT_F: sm_push_lit_f(); break;
    case SM_PUSH_NULL: case SM_PUSH_NULL_NOFLIP: sm_push_null(); break;
    case SM_PUSH_VAR:  sm_push_var(); break;
    case SM_STORE_VAR: sm_store_var(); break;
    case SM_VOID_POP:  sm_void_pop(); break;
    case SM_CONCAT:    sm_concat(); break;
    case SM_NEG:       sm_neg(); break;
    case SM_COERCE_NUM: sm_coerce_num(); break;
    case SM_EXP:       sm_exp(); break;
    case SM_ADD:       sm_add(); break;
    case SM_SUB:       sm_sub(); break;
    case SM_MUL:       sm_mul(); break;
    case SM_DIV:       sm_div(); break;
    case SM_MOD:       sm_mod(); break;
    case SM_ACOMP:     sm_acomp(); break;
    case SM_LCOMP:     sm_lcomp(); break;
    case SM_JUMP:   { sm_jump();   break; }
    case SM_JUMP_S: { sm_jump_s(); break; }
    case SM_JUMP_F: { sm_jump_f(); break; }
    case SM_CALL_FN:                                 { sm_call_fn();       break; }
    case SM_SUSPEND_VALUE:                           { sm_suspend_value(); break; }
    case SM_RETURN:   case SM_RETURN_S:  case SM_RETURN_F:  { sm_return();  break; }
    case SM_FRETURN:  case SM_FRETURN_S: case SM_FRETURN_F: { sm_freturn(); break; }
    case SM_NRETURN:  case SM_NRETURN_S: case SM_NRETURN_F: { sm_nreturn(); break; }
    case SM_DEFINE_ENTRY: sm_define_entry(); break;
    case SM_DEFINE:       sm_define(); break;
    case SM_HALT: { sm_halt(); break; }
    case SM_PAT_LIT:             sm_pat_lit(); break;
    case SM_PAT_ANY:             sm_pat_any_i(); break;
    case SM_PAT_NOTANY:          sm_pat_notany(); break;
    case SM_PAT_SPAN:            sm_pat_span(); break;
    case SM_PAT_BREAK:           sm_pat_break(); break;
    case SM_PAT_LEN:             sm_pat_len(); break;
    case SM_PAT_POS:             sm_pat_pos(); break;
    case SM_PAT_RPOS:            sm_pat_rpos(); break;
    case SM_PAT_TAB:             sm_pat_tab(); break;
    case SM_PAT_RTAB:            sm_pat_rtab(); break;
    case SM_PAT_ARB:             sm_pat_arb(); break;
    case SM_PAT_ARBNO:           sm_pat_arbno(); break;
    case SM_PAT_REM:             sm_pat_rem(); break;
    case SM_PAT_BAL:             sm_pat_bal(); break;
    case SM_PAT_FENCE0:          sm_pat_fence0(); break;
    case SM_PAT_FENCE1:          sm_pat_fence1(); break;
    case SM_PAT_ABORT:           sm_pat_abort(); break;
    case SM_PAT_FAIL:            sm_pat_fail(); break;
    case SM_PAT_SUCCEED:         sm_pat_succeed(); break;
    case SM_PAT_EPS:             sm_pat_eps(); break;
    case SM_PAT_CAT:             sm_pat_cat(); break;
    case SM_PAT_ALT:             sm_pat_alt(); break;
    case SM_PAT_DEREF:           sm_pat_deref(); break;
    case SM_PAT_REFNAME:         sm_pat_refname(); break;
    case SM_PAT_CAPTURE:         sm_pat_capture(); break;
    case SM_PAT_CAPTURE_FN:      sm_pat_capture_fn(); break;
    case SM_PAT_CAPTURE_FN_ARGS: sm_pat_capture_fn_args(); break;
    case SM_PAT_USERCALL:        sm_pat_usercall(); break;
    case SM_PAT_USERCALL_ARGS:   sm_pat_usercall_args(); break;
    case SM_EXEC_STMT: {
        const char * sname = instr->a[0].s ? instr->a[0].s : "";
        int has_repl = (int)instr->a[1].i;
        jvm_emit_ldc_string(out, sname); jvm_push_int2(out, has_repl);
        fprintf(out, "    invokestatic rt/SnoRt/sno_exec_stmt(Ljava/lang/String;I)V\n"); break;
    }
    default: break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_jvm_sm_range(SM_sequence_t * sm, int lo, int hi, int n, const char ** fn_names, const int * fn_pcs, int fn_count, FILE * out) {
    char * in_my = (char *)calloc((size_t)n, 1);
    for (int i = lo; i < hi; i++) in_my[i] = 1;
    fprintf(out, "    ; \xe2\x94\x80\xe2\x94\x80 SM instructions %d..%d \xe2\x94\x80\xe2\x94\x80\n", lo, hi - 1);
    for (int i = lo; i < hi; i++) { fprintf(out, "sm_pc_%d:\n", i); emit_jvm_one_instr(sm, i, n, fn_names, fn_pcs, fn_count, 0, in_my, out); }
    fprintf(out, "sm_pc_fn_end:\n");
    free(in_my);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_jvm_from_sm(SM_sequence_t * sm, FILE * out) {
    if (!sm || !out) return 0;
    int n = sm->count; if (n == 0) return 0;
    int * fn_pcs = (int *)calloc((size_t)n, sizeof(int));
    int * fn_ends = (int *)calloc((size_t)n, sizeof(int));
    const char ** fn_names = (const char **)calloc((size_t)n, sizeof(const char *));
    int fn_count = 0;
    for (int i = 0; i < n; i++) {
        SM_t * ins = &sm->instrs[i];
        if (ins->op == SM_LABEL && ins->a[2].i && ins->a[0].s) { fn_pcs[fn_count] = i; fn_names[fn_count] = ins->a[0].s; fn_count++; }
    }
    int * group_ends = (int *)calloc((size_t)fn_count, sizeof(int));
    for (int k = 0; k < fn_count; k++) group_ends[k] = -1;
    for (int k = 0; k < fn_count; k++) {
        int p = fn_pcs[k];
        for (int j = p - 1; j >= 0; j--) {
            SM_t * pi = &sm->instrs[j];
            if (pi->op == SM_LABEL && pi->a[2].i) break;
            if (pi->op == SM_LABEL) break;
            if (pi->op == SM_HALT) break;
            if (pi->op == SM_JUMP) { int t = (int)pi->a[0].i; if (t > p && t <= n) group_ends[k] = t; break; }
        }
    }
    for (int k = 0; k < fn_count; k++) { if (group_ends[k] < 0) { if (k > 0) group_ends[k] = group_ends[k - 1]; else group_ends[k] = n; } }
    for (int k = 0; k < fn_count; k++) {
        int my_end = group_ends[k];
        if (k + 1 < fn_count && fn_pcs[k + 1] < my_end) my_end = fn_pcs[k + 1];
        if (my_end > n) my_end = n;
        fn_ends[k] = my_end;
    }
    free(group_ends);
    char * is_fn_body = (char *)calloc((size_t)n, 1);
    for (int k = 0; k < fn_count; k++) for (int j = fn_pcs[k]; j < fn_ends[k]; j++) is_fn_body[j] = 1;
    char * body_in_my = (char *)calloc((size_t)n, 1);
    for (int i = 0; i < n; i++) body_in_my[i] = is_fn_body[i] ? 0 : 1;
    fprintf(out, ".method public static sno_body()V\n    .limit stack 16\n    .limit locals 4\n");
    fprintf(out, "    ; \xe2\x94\x80\xe2\x94\x80 SM body (non-function PCs) \xe2\x94\x80\xe2\x94\x80\n");
    for (int i = 0; i < n; i++) { if (is_fn_body[i]) continue; fprintf(out, "sm_pc_%d:\n", i); emit_jvm_one_instr(sm, i, n, fn_names, fn_pcs, fn_count, 1, body_in_my, out); }
    fprintf(out, "sm_pc_body_end:\n    return\n.end method\n");
    free(body_in_my);
    for (int k = 0; k < fn_count; k++) {
        char mname[256]; jvm_sanitize_name(mname, sizeof mname, fn_names[k]);
        fprintf(out, ".method public static sno_fn_%s()V\n    .limit stack 16\n    .limit locals 4\n", mname);
        emit_jvm_sm_range(sm, fn_pcs[k], fn_ends[k], n, fn_names, fn_pcs, fn_count, out);
        fprintf(out, "    return\n.end method\n");
    }
    free(fn_pcs); free(fn_names); free(fn_ends); free(is_fn_body);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-5: emit_js_from_sm — moved from emit_js.c. */
/* EC-UNI-13(b): exposed for SM_templates/sm_calls.c verbatim arm. */
void js_escape_string(FILE * out, const char * s) {
    fprintf(out, "\"");
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if      (c == '"')  fprintf(out, "\\\"");
        else if (c == '\\') fprintf(out, "\\\\");
        else if (c == '\n') fprintf(out, "\\n");
        else if (c == '\r') fprintf(out, "\\r");
        else if (c == '\t') fprintf(out, "\\t");
        else if (c < 0x20 || c > 0x7e) fprintf(out, "\\x%02x", c);
        else fprintf(out, "%c", c);
    }
    fprintf(out, "\"");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_js_from_sm(SM_sequence_t * sm, FILE * out) {
    if (!sm || !out || sm->count == 0) return 0;
    for (int i = 0; i < sm->count; i++) {
        SM_t * instr = &sm->instrs[i];
        g_emit.i = i; g_emit.n = sm->count; g_emit.instr = instr;
        g_emit.in_body = 0; g_emit.in_my_method = NULL;
        g_emit.pc_to_fn = NULL; g_emit.fn_names = NULL; g_emit.fn_pcs = NULL; g_emit.fn_count = 0;
        fprintf(out, "case %d: ", i);
        int has_continue = 0;
        /* EC-UNI-14(a): JS-only overrides come first.  SM_PUSH_EXPRESSION emits a literal
         * `rt.push_null();` (no template handles this opcode in JS, since no JS frontend lowers
         * SM_PUSH_EXPRESSION today; the legacy switch had this stub).  All other opcodes go
         * through the shared dispatcher.
         * Side note: the legacy JS switch lacked arms for SM_PUSH_LIT_CS and SM_PAT_FENCE1
         * (others list them); the shared dispatcher now covers both.  No current JS-targeting
         * frontend emits either opcode, so dispatcher widening is observably safe. */
        if (instr->op == SM_PUSH_EXPRESSION) { fprintf(out, "rt.push_null(); "); }
        else                                 { has_continue = emit_sm_dispatch(); }
        if (!has_continue) fprintf(out, "_pc = %d; continue; ", i + 1);
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-5: emit_net_from_sm — moved from emit_net.c. */
static int emit_net_from_sm(SM_sequence_t * sm, FILE * out) {
    if (!sm || !out) return 0;
    int n = sm->count;
    int * fn_pcs = NULL; const char ** fn_names = NULL; char *** fn_params = NULL;
    int * fn_nparams = NULL; int fn_count = 0; int * pc_to_fn = NULL;
    if (n > 0) {
        fn_pcs = (int *)calloc((size_t)n, sizeof(int));
        fn_names = (const char **)calloc((size_t)n, sizeof(const char *));
        fn_params = (char ***)calloc((size_t)n, sizeof(char **));
        fn_nparams = (int *)calloc((size_t)n, sizeof(int));
        pc_to_fn = (int *)malloc((size_t)n * sizeof(int));
        for (int p = 0; p < n; p++) pc_to_fn[p] = -1;
        for (int i = 0; i < n; i++) {
            SM_t * ins = &sm->instrs[i];
            if (ins->op == SM_LABEL && ins->a[2].i && ins->a[0].s) { fn_pcs[fn_count] = i; fn_names[fn_count] = ins->a[0].s; fn_count++; }
        }
        for (int i = 1; i < n; i++) {
            SM_t * ins = &sm->instrs[i];
            if (ins->op != SM_CALL_FN && ins->op != SM_SUSPEND_VALUE) continue;
            if (!ins->a[0].s || strcmp(ins->a[0].s, "DEFINE") != 0) continue;
            SM_t * prev = &sm->instrs[i - 1];
            if (prev->op != SM_PUSH_LIT_S && prev->op != SM_PUSH_LIT_CS) continue;
            if (!prev->a[0].s) continue;
            char * fname = NULL; int npar = 0;
            char ** pars = net_parse_define_proto(prev->a[0].s, &fname, &npar);
            if (fname) {
                for (int k = 0; k < fn_count; k++) {
                    if (fn_names[k] && strcmp(fn_names[k], fname) == 0) { fn_params[k] = pars; fn_nparams[k] = npar; pars = NULL; break; }
                }
                free(fname);
                if (pars) { for (int q = 0; q < npar; q++) free(pars[q]); free(pars); }
            }
        }
        for (int k = 0; k < fn_count; k++) {
            int entry = fn_pcs[k], around_target = -1;
            for (int p = entry - 1; p >= 0; p--) {
                SM_t * pi = &sm->instrs[p];
                if (pi->op == SM_JUMP && (int)pi->a[0].i > entry) { around_target = (int)pi->a[0].i; break; }
            }
            int body_end = (around_target > 0) ? around_target - 1 : entry;
            for (int p = entry; p <= body_end && p < n; p++) if (pc_to_fn[p] < 0) pc_to_fn[p] = k;
        }
    }
    fprintf(out, "    .locals init (int32 _pc)\n    ldc.i4.0\n    stloc      _pc\n");
    fprintf(out, "  NET_DISPATCH:\n    ldloc      _pc\n    switch (");
    for (int i = 0; i < n; i++) { fprintf(out, "NET_L%d", i); if (i < n - 1) fprintf(out, ", "); }
    fprintf(out, ")\n    br         NET_DONE\n");
    for (int i = 0; i < n; i++) {
        SM_t * instr = &sm->instrs[i];
        g_emit.i = i; g_emit.n = n; g_emit.instr = instr;
        g_emit.in_body = 0; g_emit.in_my_method = NULL;
        g_emit.pc_to_fn = pc_to_fn; g_emit.fn_names = fn_names; g_emit.fn_pcs = fn_pcs; g_emit.fn_count = fn_count;
        fprintf(out, "  NET_L%d:\n", i);
        int has_continue = 0;
        /* EC-UNI-14(a): NET-only overrides come first.  Two opcodes inline code that the shared
         * dispatcher cannot emit:
         *   SM_LABEL: needs walker-local fn_params/fn_nparams arrays (NET-specific param-binding
         *     proto parsing happens upstream in emit_net_from_sm; g_emit doesn't carry them).
         *   SM_EXEC_STMT: emits a fixed NET-specific stack-cleanup + set_omega/set_sigma/
         *     set_delta/set_last_ok sequence — the sm_exec_stmt() template's NET arm is
         *     equivalent in behavior but differs in detail (per HQ note: NET preserved
         *     SM_EXEC_STMT unchanged across the EC-UNI-13(c) consolidation).
         * Both stay inline at EC-UNI-14(a).  EC-UNI-14 proper folds NET SM_LABEL handling into
         * sm_label() template (currently nonexistent) and reconciles SM_EXEC_STMT NET arm. */
        if (instr->op == SM_LABEL) {
            if (instr->a[2].i && instr->a[0].s) {
                int k = -1;
                for (int q = 0; q < fn_count; q++) if (fn_pcs[q] == i) { k = q; break; }
                fprintf(out, "    ldc.i4.1\n    call       void SnoRt::set_last_ok(bool)\n");
                fprintf(out, "    call       void SnoRt::frame_push()\n");
                if (k >= 0 && fn_names[k]) { net_escape_ldstr(out, fn_names[k]); fprintf(out, "    call       void SnoRt::frame_save(string)\n"); }
                if (k >= 0 && fn_params[k] && fn_nparams[k] > 0) {
                    for (int p = 0; p < fn_nparams[k]; p++) { net_escape_ldstr(out, fn_params[k][p]); fprintf(out, "    call       void SnoRt::frame_save(string)\n"); }
                    for (int p = fn_nparams[k] - 1; p >= 0; p--) { net_escape_ldstr(out, fn_params[k][p]); fprintf(out, "    call       void SnoRt::store_var(string)\n"); }
                }
            }
        }
        else if (instr->op == SM_EXEC_STMT) {
            int has_repl = (int)instr->a[1].i; const char * subj_name = instr->a[0].s ? instr->a[0].s : "";
            if (has_repl) fprintf(out, "    pop\n");
            fprintf(out, "    pop\n    pop\n");
            net_escape_ldstr(out, subj_name);
            fprintf(out, "    call       void SnoRt::push_var(string)\n    dup\n");
            fprintf(out, "    call       int32 [mscorlib]System.String::get_Length()\n");
            fprintf(out, "    call       void SnoRt::set_omega(int32)\n    call       void SnoRt::set_sigma(string)\n");
            fprintf(out, "    ldc.i4.0\n    call       void SnoRt::set_delta(int32)\n");
            fprintf(out, "    ldc.i4.0\n    call       void SnoRt::set_last_ok(bool)\n");
        }
        else {
            has_continue = emit_sm_dispatch();
        }
        if (!has_continue && i + 1 < n) { net_push_i4(out, i + 1); fprintf(out, "    stloc      _pc\n    br         NET_DISPATCH\n"); }
    }
    fprintf(out, "  NET_DONE:\n");
    if (fn_params) { for (int k = 0; k < fn_count; k++) { if (fn_params[k]) { for (int q = 0; q < fn_nparams[k]; q++) free(fn_params[k][q]); free(fn_params[k]); } } free(fn_params); }
    free(fn_nparams); free(pc_to_fn); free(fn_pcs); free(fn_names);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-5: emit_program — unified entry point replacing emit_jvm_program/emit_js_program/emit_net_program. */
int emit_program(const tree_t * ast_prog, FILE * out, bb_emit_mode_t mode) {
    if (!ast_prog || !out) return 1;
    stage2_t * s2 = sm_preamble(ast_prog);
    if (!s2) return 1;
    SM_sequence_t * sm = &s2->sm;
    bb_emit_mode_t saved_mode = bb_emit_mode;
    FILE *         saved_out  = bb_emit_out;
    /* EC-UNI-10(a): populate g_emit.  Pass-wide fields are set once here at entry; per-instruction
     * and per-BB-node fields are set by the dispatcher loops in EC-UNI-10(b)/(c).  Save/restore
     * the prior g_emit so nested emit_program invocations don't trample each other. */
    sm_emit_t saved_g_emit = g_emit;
    g_emit.backend   = (int)mode;
    g_emit.is_binary = (mode == EMIT_BINARY_WIRED || mode == EMIT_BINARY_BROKERED);
    g_emit.out       = out;
    g_emit.prog      = sm;
    emit_mode_set(mode, out);
    if (IS_WASM) {
        wasm_strtab_reset();
        wasm_userfns_reset();
        for (int i = 0; i < sm->count; i++) {
            SM_t * ins = &sm->instrs[i];
            if ((ins->op == SM_PUSH_LIT_S || ins->op == SM_PUSH_LIT_CS) && ins->a[0].s) wasm_intern_str(ins->a[0].s);
            else if ((ins->op == SM_PUSH_VAR || ins->op == SM_STORE_VAR || ins->op == SM_CALL_FN || ins->op == SM_SUSPEND_VALUE) && ins->a[0].s) wasm_intern_name(ins->a[0].s);
            else if (ins->op == SM_PAT_LIT && ins->a[0].s) wasm_intern_str(ins->a[0].s);
            else if ((ins->op == SM_PAT_REFNAME || ins->op == SM_PAT_CAPTURE) && ins->a[0].s) wasm_intern_name(ins->a[0].s);
        }
        wasm_intern_str("OUTPUT"); wasm_intern_str("INPUT");
        wasm_pre_scan_userfns(sm);
        emit_prologue(NULL, out);
        fprintf(out, "  (func $main (export \"main\")\n");
        fprintf(out, "    (local $pc i32)\n");
        fprintf(out, "    (local $tmp i32)\n");
        fprintf(out, "    (local $fr i32)\n");
        fprintf(out, "    (call $sno_init)\n");
        emit_wasm_from_sm(sm, out);
        fprintf(out, "    (call $sno_finalize)\n");
        fprintf(out, "  )\n");
        emit_epilogue(NULL, out);
        emit_mode_set(saved_mode, saved_out);
        /* EC-UNI-11: flush Layer-3 buffers to `out`.  No-op today (no template uses emit_text*/
        /* / emit_byte* yet); load-bearing once EC-UNI-12 sweeps fprintf→emit_textf etc. */
        emit_io_flush(out);
        g_emit = saved_g_emit;
        /* g_stage2 is global; no free */
        return 0;
    }
    emit_prologue(NULL, out);
    if (IS_JVM) emit_jvm_from_sm(sm, out);
    else if (IS_JS) {
        fprintf(out, "rt._register_label_pcs({");
        int first = 1;
        for (int i = 0; i < sm->count; i++) {
            SM_t * in = &sm->instrs[i];
            if (in->op == SM_LABEL && in->a[0].s && in->a[0].s[0]) {
                if (!first) fprintf(out, ",");
                js_escape_string(out, in->a[0].s);
                fprintf(out, ":%d", i);
                first = 0;
            }
        }
        fprintf(out, "});\n");
        emit_js_from_sm(sm, out);
    } else if (IS_NET) emit_net_from_sm(sm, out);
    emit_epilogue(NULL, out);
    emit_mode_set(saved_mode, saved_out);
    /* EC-UNI-11: flush Layer-3 buffers to `out`.  See WASM exit above for rationale. */
    emit_io_flush(out);
    g_emit = saved_g_emit;
    /* g_stage2 is global; no free */
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-UNI-14(a): the parameterless emit_sm_dispatch(void) defined above (just before
 * emit_wasm_from_sm) supersedes the EC-UNI-0 scaffold stub that used to live here.  The new
 * shape reads g_emit and is called from inside the WASM/JS/NET per-PC loops with the caller
 * responsible for backend-specific overrides (SM_LABEL on NET, SM_EXEC_STMT on NET,
 * SM_PUSH_EXPRESSION on JS, SM_INCR/SM_DECR on WASM). */
