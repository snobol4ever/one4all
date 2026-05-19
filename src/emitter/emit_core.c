#include "emit_core.h"
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
/* EC-2 helpers — shared across JVM/JS/.NET arms; removed from silos in EC-5. */
#include "IR.h"
#include "emit_ir.h"
static void ec_js_escape(FILE * out, const char * s) {
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
static void ec_jvm_class_hdr(FILE * out, const char * name) {
    fprintf(out, ".class public bb/bb_%s\n", name);
    fprintf(out, ".super bb/bb_box\n");
    fprintf(out, ".inner class public static final spec inner bb/bb_box$Spec outer bb/bb_box\n");
    fprintf(out, ".inner class public static final matchstate inner bb/bb_box$MatchState outer bb/bb_box\n");
}
static void ec_net_escape_ldstr(FILE * out, const char * s) {
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
static void ec_net_class_hdr(FILE * out, int sid, int nid) {
    fprintf(out, ".class nested public auto ansi beforefieldinit pat_%d_%d\n", sid, nid);
    fprintf(out, "       extends [mscorlib]System.Object\n");
    fprintf(out, "       implements [boxes]Snobol4.Runtime.Boxes.IByrdBox\n{\n");
}
static void ec_net_alpha_hdr(FILE * out) {
    fprintf(out, "  .method public virtual instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec\n");
    fprintf(out, "          Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState ms) cil managed\n  {\n");
}
static void ec_net_beta_hdr(FILE * out) {
    fprintf(out, "  .method public virtual instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec\n");
    fprintf(out, "          Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState ms) cil managed\n  {\n");
}
static void ec_net_fail_ret(FILE * out) {
    fprintf(out, "    ldsfld     valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.Spec::Fail\n");
    fprintf(out, "    ret\n");
}
static void ec_net_cursor_load(FILE * out) {
    fprintf(out, "    ldarg.1\n");
    fprintf(out, "    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
}
static void ec_net_ms_length(FILE * out) {
    fprintf(out, "    callvirt   instance int32 [boxes]Snobol4.Runtime.Boxes.MatchState::get_Length()\n");
}
static void ec_net_spec_of(FILE * out) {
    fprintf(out, "    call       valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.Spec::Of(int32, int32)\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2: IR_PAT_LIT — unified template function (JVM / JS / .NET arms; x86 arms in emit_bb.c). */
/* EC-2b: IR_PAT_LIT — one function, all modes. */
static void ec_bb_lit(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_flat_node/emit_bb_xchr — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "lit_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "lit");
        fprintf(out, ".field private final lit Ljava/lang/String;\n");
        fprintf(out, ".field private final len I\n");
        fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Ljava/lang/String;)V\n");
        fprintf(out, "    .limit stack 3\n    .limit locals 3\n");
        fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_lit/lit Ljava/lang/String;\n");
        fprintf(out, "    aload_0\n    aload_2\n    invokevirtual java/lang/String/length()I\n    putfield bb/bb_lit/len I\n");
        fprintf(out, "    return\n.end method\n");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
        fprintf(out, "    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n    iadd\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n");
        fprintf(out, "    if_icmpgt %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/lit Ljava/lang/String;\n    iconst_0\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n");
        fprintf(out, "    invokevirtual java/lang/String/regionMatches(ILjava/lang/String;II)Z\n");
        fprintf(out, "    ifeq %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    getfield bb/bb_lit/len I\n");
        fprintf(out, "    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
        fprintf(out, "    .limit stack 4\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { const lit = ", nd->ival, nid);
        ec_js_escape(out, nd->sval);
        fprintf(out, "; const len = lit.length; let self = { succ: null, fail: null,\n");
        fprintf(out, "alpha() { if (ms.delta + len > ms.omega || ms.sigma.slice(ms.delta, ms.delta + len) !== lit) { self.fail.alpha(); return; } ms.delta += len; self.succ.alpha(); },\n");
        fprintf(out, "beta() { ms.delta -= len; self.fail.alpha(); }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * lit = nd->sval ? nd->sval : "";
        ec_net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private string _lit\n  .field private int32  _len\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(string lit) cil managed\n  {\n");
        fprintf(out, "    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    dup\n    brtrue     LIT_%d_%d_NN\n    pop\n    ldstr      \"\"\n", sid, nid);
        fprintf(out, "  LIT_%d_%d_NN:\n", sid, nid);
        fprintf(out, "    stfld      string pat_%d_%d::_lit\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.0\n");
        fprintf(out, "    ldfld      string pat_%d_%d::_lit\n", sid, nid);
        fprintf(out, "    callvirt   instance int32 [mscorlib]System.String::get_Length()\n");
        fprintf(out, "    stfld      int32 pat_%d_%d::_len\n    ret\n  }\n", sid, nid);
        ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        ec_net_cursor_load(out);
        fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n    add\n", sid, nid);
        fprintf(out, "    ldarg.1\n"); ec_net_ms_length(out);
        fprintf(out, "    bgt        LIT_%d_%d_A_FAIL\n", sid, nid);
        fprintf(out, "    ldarg.1\n");
        ec_net_cursor_load(out);
        fprintf(out, "    ldfld      string pat_%d_%d::_lit\n", sid, nid);
        fprintf(out, "    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::MatchesAt(int32, string)\n");
        fprintf(out, "    brfalse    LIT_%d_%d_A_FAIL\n", sid, nid);
        ec_net_cursor_load(out);
        fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n", sid, nid);
        ec_net_spec_of(out);
        fprintf(out, "    stloc.0\n");
        fprintf(out, "    ldarg.1\n    ldarg.1\n");
        ec_net_cursor_load(out);
        fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n    add\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        fprintf(out, "    ldloc.0\n    ret\n");
        fprintf(out, "  LIT_%d_%d_A_FAIL:\n", sid, nid);
        ec_net_fail_ret(out);
        fprintf(out, "  }\n");
        ec_net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n");
        fprintf(out, "    ldarg.1\n    ldarg.1\n");
        ec_net_cursor_load(out);
        fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n    sub\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        ec_net_fail_ret(out);
        fprintf(out, "  }\n}\n");
        ec_net_escape_ldstr(out, lit);
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2 charset helpers — used by ANY, NOTANY, SPAN, BREAK. */
static void ec_jvm_init_ms_str(FILE * out, const char * name, const char * field) {
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Ljava/lang/String;)V\n");
    fprintf(out, "    .limit stack 3\n    .limit locals 3\n");
    fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
    fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_%s/%s Ljava/lang/String;\n", name, field);
    fprintf(out, "    return\n.end method\n");
}
static void ec_net_charset_class(FILE * out, int sid, int nid, const char * tag) {
    ec_net_class_hdr(out, sid, nid);
    fprintf(out, "  .field private string _chars\n");
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor(string chars) cil managed\n  {\n");
    fprintf(out, "    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ldarg.0\n    ldarg.1\n    dup\n    brtrue     %s_%d_%d_NN\n    pop\n    ldstr      \"\"\n", tag, sid, nid);
    fprintf(out, "  %s_%d_%d_NN:\n    stfld      string pat_%d_%d::_chars\n    ret\n  }\n", tag, sid, nid, sid, nid);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b helpers — used by ARB, ARBNO, CAT, ALT, LEN, POS/RPOS, TAB/RTAB, REM, FENCE, ABORT, ASSIGN_*. */
static void ec_jvm_init_ms_only(FILE * out, const char * name) {
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;)V\n    .limit stack 2\n    .limit locals 2\n    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n    return\n.end method\n");
    (void)name;
}
static void ec_jvm_init_ms_int(FILE * out, const char * name, const char * field) {
    fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;I)V\n    .limit stack 3\n    .limit locals 3\n    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n    aload_0\n    iload_2\n    putfield bb/bb_%s/%s I\n    aload_0\n    aconst_null\n    putfield bb/bb_%s/dyn Ljava/util/function/IntSupplier;\n    return\n.end method\n", name, field, name);
}
static void ec_jvm_val_helper(FILE * out, const char * name) {
    fprintf(out, ".method private val()I\n    .limit stack 2\n    .limit locals 1\n    aload_0\n    getfield bb/bb_%s/dyn Ljava/util/function/IntSupplier;\n    ifnull %s_val_static\n    aload_0\n    getfield bb/bb_%s/dyn Ljava/util/function/IntSupplier;\n    invokeinterface java/util/function/IntSupplier/getAsInt()I 1\n    ireturn\n%s_val_static:\n    aload_0\n    getfield bb/bb_%s/n I\n    ireturn\n.end method\n", name, name, name, name, name);
}
static void ec_net_push_i4(FILE * out, int v) {
    if (v >= 0 && v <= 8)          { fprintf(out, "    ldc.i4.%d\n", v); }
    else if (v == -1)               { fprintf(out, "    ldc.i4.m1\n"); }
    else if (v >= -128 && v <= 127) { fprintf(out, "    ldc.i4.s   %d\n", v); }
    else                            { fprintf(out, "    ldc.i4     %d\n", v); }
}
static void ec_net_ctor_none(FILE * out, int sid, int nid) {
    fprintf(out, "  .method public specialname rtspecialname instance void .ctor() cil managed\n  {\n    .maxstack 1\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n    ret\n  }\n");
    (void)sid; (void)nid;
}
static void ec_net_spec_zw(FILE * out) {
    fprintf(out, "    call       valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.Spec::ZeroWidth(int32)\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_ANY — one function, all modes. */
static void ec_bb_any(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_charset/bb_any — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "any_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "any"); fprintf(out, ".field private final chars Ljava/lang/String;\n"); ec_jvm_init_ms_str(out, "any", "chars");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpge %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_any/chars Ljava/lang/String;\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    invokevirtual java/lang/String/charAt(I)C\n    invokevirtual java/lang/String/indexOf(I)I\n    iflt %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    iconst_1\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid); ec_js_escape(out, nd->sval);
        fprintf(out, "; let self = { succ: null, fail: null,\nalpha() { if (ms.delta >= ms.omega || chars.indexOf(ms.sigma[ms.delta]) < 0) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + 1); ms.delta++; self.succ.alpha(); return r; },\nbeta() { ms.delta--; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * chars = nd->sval ? nd->sval : "";
        ec_net_charset_class(out, sid, nid, "ANY"); ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldfld      string pat_%d_%d::_chars\n    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n    brfalse    ANY_%d_%d_A_FAIL\n", sid, nid, sid, nid);
        ec_net_cursor_load(out); fprintf(out, "    ldc.i4.1\n"); ec_net_spec_of(out); fprintf(out, "    stloc.0\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldc.i4.1\n    add\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  ANY_%d_%d_A_FAIL:\n", sid, nid);
        ec_net_fail_ret(out); fprintf(out, "  }\n"); ec_net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldc.i4.1\n    sub\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        ec_net_escape_ldstr(out, chars); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_NOTANY — one function, all modes. */
static void ec_bb_notany(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_charset/bb_notany — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "notany_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "notany"); fprintf(out, ".field private final chars Ljava/lang/String;\n"); ec_jvm_init_ms_str(out, "notany", "chars");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpge %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_notany/chars Ljava/lang/String;\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    invokevirtual java/lang/String/charAt(I)C\n    invokevirtual java/lang/String/indexOf(I)I\n    ifge %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    iconst_1\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid); ec_js_escape(out, nd->sval);
        fprintf(out, "; let self = { succ: null, fail: null,\nalpha() { if (ms.delta >= ms.omega || chars.indexOf(ms.sigma[ms.delta]) >= 0) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + 1); ms.delta++; self.succ.alpha(); return r; },\nbeta() { ms.delta--; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * chars = nd->sval ? nd->sval : "";
        ec_net_charset_class(out, sid, nid, "NOTANY"); ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldfld      string pat_%d_%d::_chars\n    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n    brtrue     NOTANY_%d_%d_A_FAIL\n", sid, nid, sid, nid);
        ec_net_cursor_load(out); fprintf(out, "    ldc.i4.1\n"); ec_net_spec_of(out); fprintf(out, "    stloc.0\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldc.i4.1\n    add\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  NOTANY_%d_%d_A_FAIL:\n", sid, nid);
        ec_net_fail_ret(out); fprintf(out, "  }\n"); ec_net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldc.i4.1\n    sub\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        ec_net_escape_ldstr(out, chars); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_SPAN — one function, all modes. */
static void ec_bb_span(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_charset/bb_span — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "span_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "span"); fprintf(out, ".field private final chars Ljava/lang/String;\n.field private matched_len I\n"); ec_jvm_init_ms_str(out, "span", "chars");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 3\n");
        fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_span/matched_len I\n%s_loop:\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_span/matched_len I\n    iadd\n    istore_1\n");
        fprintf(out, "    iload_1\n    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpge %s_done\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_span/chars Ljava/lang/String;\n    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n    iload_1\n    invokevirtual java/lang/String/charAt(I)C\n");
        fprintf(out, "    invokevirtual java/lang/String/indexOf(I)I\n    iflt %s_done\n    aload_0\n    dup\n    getfield bb/bb_span/matched_len I\n    iconst_1\n    iadd\n    putfield bb/bb_span/matched_len I\n    goto %s_loop\n", tag, tag);
        fprintf(out, "%s_done:\n    aload_0\n    getfield bb/bb_span/matched_len I\n    ifle %s_omega\n", tag, tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_span/matched_len I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_2\n    aload_0\n    getfield bb/bb_span/matched_len I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_span/matched_len I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid); ec_js_escape(out, nd->sval);
        fprintf(out, "; let delta = 0; let self = { succ: null, fail: null,\nalpha() { delta = 0; while (ms.delta + delta < ms.omega && chars.indexOf(ms.sigma[ms.delta + delta]) >= 0) delta++; if (delta <= 0) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\nbeta() { ms.delta -= delta; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * chars = nd->sval ? nd->sval : "";
        ec_net_class_hdr(out, sid, nid); fprintf(out, "  .field private string _chars\n  .field private int32  _count\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(string chars) cil managed\n  {\n    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    dup\n    brtrue     SP_%d_%d_NN\n    pop\n    ldstr      \"\"\n  SP_%d_%d_NN:\n    stfld      string pat_%d_%d::_chars\n    ret\n  }\n", sid, nid, sid, nid, sid, nid);
        ec_net_alpha_hdr(out); fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_count\n  SP_%d_%d_LOOP:\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.1\n"); ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    ldfld      string pat_%d_%d::_chars\n    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n    brfalse    SP_%d_%d_DONE\n", sid, nid, sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_count\n    br         SP_%d_%d_LOOP\n  SP_%d_%d_DONE:\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.0\n    ble        SP_%d_%d_FAIL\n", sid, nid, sid, nid, sid, nid, sid, nid, sid, nid, sid, nid);
        ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n", sid, nid); ec_net_spec_of(out); fprintf(out, "    stloc.0\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  SP_%d_%d_FAIL:\n", sid, nid, sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n");
        ec_net_beta_hdr(out); fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    sub\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n", sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        ec_net_escape_ldstr(out, chars); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_BREAK — one function, all modes. */
static void ec_bb_break(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_charset/bb_brk — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "brk_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "brk"); fprintf(out, ".field private final chars Ljava/lang/String;\n.field private matched_len I\n"); ec_jvm_init_ms_str(out, "brk", "chars");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 3\n    aload_0\n    iconst_0\n    putfield bb/bb_brk/matched_len I\n%s_loop:\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_brk/matched_len I\n    iadd\n    istore_1\n");
        fprintf(out, "    iload_1\n    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpge %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_brk/chars Ljava/lang/String;\n    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n    iload_1\n    invokevirtual java/lang/String/charAt(I)C\n");
        fprintf(out, "    invokevirtual java/lang/String/indexOf(I)I\n    ifge %s_found\n    aload_0\n    dup\n    getfield bb/bb_brk/matched_len I\n    iconst_1\n    iadd\n    putfield bb/bb_brk/matched_len I\n    goto %s_loop\n%s_found:\n", tag, tag, tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_brk/matched_len I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_2\n    aload_0\n    getfield bb/bb_brk/matched_len I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_brk/matched_len I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid); ec_js_escape(out, nd->sval);
        fprintf(out, "; let delta = 0; let self = { succ: null, fail: null,\nalpha() { delta = 0; while (ms.delta + delta < ms.omega && chars.indexOf(ms.sigma[ms.delta + delta]) < 0) delta++; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\nbeta() { ms.delta -= delta; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * chars = nd->sval ? nd->sval : "";
        ec_net_class_hdr(out, sid, nid); fprintf(out, "  .field private string _chars\n  .field private int32  _count\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(string chars) cil managed\n  {\n    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    dup\n    brtrue     BRK_%d_%d_NN\n    pop\n    ldstr      \"\"\n  BRK_%d_%d_NN:\n    stfld      string pat_%d_%d::_chars\n    ret\n  }\n", sid, nid, sid, nid, sid, nid);
        ec_net_alpha_hdr(out); fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_count\n  BRK_%d_%d_LOOP:\n", sid, nid, sid, nid);
        ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    ldarg.1\n", sid, nid); ec_net_ms_length(out);
        fprintf(out, "    bge        BRK_%d_%d_EOS\n    ldarg.1\n", sid, nid); ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    ldarg.0\n    ldfld      string pat_%d_%d::_chars\n", sid, nid, sid, nid);
        fprintf(out, "    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n    brtrue     BRK_%d_%d_FOUND\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_count\n    br         BRK_%d_%d_LOOP\n  BRK_%d_%d_EOS:\n", sid, nid, sid, nid, sid, nid, sid, nid); ec_net_fail_ret(out);
        fprintf(out, "  BRK_%d_%d_FOUND:\n", sid, nid); ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n", sid, nid); ec_net_spec_of(out); fprintf(out, "    stloc.0\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  }\n", sid, nid);
        ec_net_beta_hdr(out); fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    sub\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n", sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        ec_net_escape_ldstr(out, chars); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_ARB — one function, all modes. */
static void ec_bb_arb(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_xfarb — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "arb_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "arb");
        fprintf(out, ".field private arb_count I\n.field private arb_start I\n");
        ec_jvm_init_ms_only(out, "arb");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_arb/arb_count I\n");
        fprintf(out, "    aload_0\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_arb/arb_start I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_arb/arb_count I\n    iconst_1\n    iadd\n    putfield bb/bb_arb/arb_count I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arb/arb_start I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    iadd\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpgt %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_arb/arb_start I\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    areturn\n%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { let delta = 0; let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { delta = ms.omega - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
        fprintf(out, "beta() { if (delta <= 0) { self.fail.alpha(); return; } delta--; ms.delta--; const r = ms.sigma.slice(ms.delta, ms.delta + delta + 1); return r; }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    if (IS_NET) {
        ec_net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private int32 _count\n  .field private int32 _start\n");
        ec_net_ctor_none(out, sid, nid);
        ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 2\n");
        fprintf(out, "    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_count\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stfld      int32 pat_%d_%d::_start\n", sid, nid);
        ec_net_cursor_load(out); ec_net_spec_zw(out); fprintf(out, "    ret\n  }\n");
        ec_net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n");
        fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_count\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
        fprintf(out, "    ldarg.1\n"); ec_net_ms_length(out);
        fprintf(out, "    bgt        ARB_%d_%d_FAIL\n", sid, nid);
        fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n", sid, nid);
        ec_net_spec_of(out); fprintf(out, "    ret\n");
        fprintf(out, "  ARB_%d_%d_FAIL:\n", sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_ARBNO — one function, all modes. */
static void ec_bb_arbno(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_xarbn — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "arbno_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "arbno");
        fprintf(out, ".field private static final MAX_DEPTH I = 64\n");
        fprintf(out, ".field private final body Lbb/bb_box;\n");
        fprintf(out, ".field private final frame_start [I\n.field private final frame_match_st [I\n.field private final frame_match_ln [I\n");
        fprintf(out, ".field private depth I\n");
        fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;)V\n    .limit stack 4\n    .limit locals 3\n");
        fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_arbno/body Lbb/bb_box;\n");
        fprintf(out, "    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_start [I\n");
        fprintf(out, "    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_match_st [I\n");
        fprintf(out, "    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_match_ln [I\n");
        fprintf(out, "    return\n.end method\n");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_arbno/depth I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    iconst_0\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    iconst_0\n    iconst_0\n    iastore\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    iconst_0\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
        fprintf(out, "    aload_0\n    invokevirtual bb/bb_arbno/tryBody()Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/depth I\n    ifle %s_beta_omega\n", tag);
        fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    isub\n    putfield bb/bb_arbno/depth I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_beta_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method private tryBody()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 4\n%s_tryBody_loop:\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/body Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_tryBody_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    if_icmpne %s_tryBody_advance\n", tag);
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_tryBody_advance:\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    istore_2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    iadd\n    bipush 64\n    if_icmpge %s_tryBody_full\n", tag);
        fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    iadd\n    putfield bb/bb_arbno/depth I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iload_2\n    iastore\n");
        fprintf(out, "    istore 3\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iload 3\n    iastore\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
        fprintf(out, "    goto %s_tryBody_loop\n%s_tryBody_full:\n", tag, tag);
        fprintf(out, "    new bb/bb_box$Spec\n    dup_x1\n    swap\n    iload_2\n    swap\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_tryBody_omega:\n", tag);
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { const stack = []; let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { stack.length = 0; stack.push({ start: ms.delta }); while (true) { const frame = stack[stack.length - 1]; const br = self.body.alpha();\n");
        fprintf(out, "if (br === null) { return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
        fprintf(out, "if (ms.delta === frame.start) { return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
        fprintf(out, "stack.push({ start: ms.delta }); } },\n");
        fprintf(out, "beta() { if (stack.length <= 1) { self.fail.alpha(); return; } stack.pop(); const frame = stack[stack.length - 1]; ms.delta = frame.start; return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    if (IS_NET) {
        ec_net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _body\n");
        fprintf(out, "  .field private int32[] _matchStart\n  .field private int32[] _matchLen\n");
        fprintf(out, "  .field private int32[] _startStack\n  .field private int32   _depth\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox body) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_body\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_startStack\n    ret\n  }\n", sid, nid);
        ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 4\n    .locals init (int32 V_startHere, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_br)\n");
        fprintf(out, "    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldc.i4.0\n", sid, nid); ec_net_cursor_load(out); fprintf(out, "    stelem.i4\n");
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n    ldc.i4.0\n    ldc.i4.0\n    stelem.i4\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n    ldc.i4.0\n", sid, nid); ec_net_cursor_load(out); fprintf(out, "    stelem.i4\n");
        fprintf(out, "  ARBNO_%d_%d_LOOP:\n", sid, nid); ec_net_cursor_load(out); fprintf(out, "    stloc.0\n");
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_body\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.1\n    ldloca.s   V_br\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brtrue     ARBNO_%d_%d_STOP\n", sid, nid); ec_net_cursor_load(out); fprintf(out, "    ldloc.0\n    beq        ARBNO_%d_%d_STOP\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4     63\n    bge        ARBNO_%d_%d_STOP\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldc.i4.0\n    ldelem.i4\n    stelem.i4\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    sub\n    ldelem.i4\n", sid, nid);
        fprintf(out, "    ldloca.s   V_br\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n    stelem.i4\n");
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        ec_net_cursor_load(out); fprintf(out, "    stelem.i4\n    br         ARBNO_%d_%d_LOOP\n", sid, nid);
        fprintf(out, "  ARBNO_%d_%d_STOP:\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        ec_net_spec_of(out); fprintf(out, "    ret\n  }\n");
        ec_net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.0\n    ble        ARBNO_%d_%d_BFAIL\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    sub\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        ec_net_spec_of(out); fprintf(out, "    ret\n  ARBNO_%d_%d_BFAIL:\n", sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox)\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_ALT — one function, all modes. */
static void ec_bb_alt(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_flat_alt — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "alt_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "alt");
        fprintf(out, ".field private final children [Lbb/bb_box;\n.field private final n I\n.field private current I\n.field private position I\n");
        fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;[Lbb/bb_box;)V\n    .limit stack 3\n    .limit locals 3\n");
        fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_alt/children [Lbb/bb_box;\n");
        fprintf(out, "    aload_0\n    aload_2\n    arraylength\n    putfield bb/bb_alt/n I\n    return\n.end method\n");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 3\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    aload_0\n    getfield bb/bb_alt/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_alt/position I\n");
        fprintf(out, "    aload_0\n    iconst_1\n    putfield bb/bb_alt/current I\n");
        fprintf(out, "    aload_0\n    invokevirtual bb/bb_alt/tryAlpha()Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_alt/children [Lbb/bb_box;\n    aload_0\n    getfield bb/bb_alt/current I\n    iconst_1\n    isub\n    aaload\n");
        fprintf(out, "    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_beta_omega\n", tag);
        fprintf(out, "    aload_1\n    areturn\n%s_beta_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method private tryAlpha()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 2\n%s_try_loop:\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_alt/current I\n    aload_0\n    getfield bb/bb_alt/n I\n    if_icmpgt %s_try_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_alt/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_alt/position I\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_alt/children [Lbb/bb_box;\n    aload_0\n    getfield bb/bb_alt/current I\n    iconst_1\n    isub\n    aaload\n");
        fprintf(out, "    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_try_next\n    aload_1\n    areturn\n", tag);
        fprintf(out, "%s_try_next:\n    aload_0\n    dup\n    getfield bb/bb_alt/current I\n    iconst_1\n    iadd\n    putfield bb/bb_alt/current I\n    goto %s_try_loop\n", tag, tag);
        fprintf(out, "%s_try_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { const children = self.children || []; let idx = 0; let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { idx = 0; while (idx < children.length) { const r = children[idx].alpha(); if (r !== null) { self.succ.alpha(); return r; } idx++; } self.fail.alpha(); return null; },\n");
        fprintf(out, "beta() { idx--; if (idx >= 0 && idx < children.length) { const r = children[idx].beta(); if (r !== null) { return r; } return self.beta(); } self.fail.alpha(); return null; }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    if (IS_NET) {
        ec_net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] _children\n  .field private int32 _idx\n  .field private int32 _savedPos\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] children) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ret\n  }\n", sid, nid);
        ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        fprintf(out, "    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
        fprintf(out, "  ALT_%d_%d_LOOP:\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ldlen\n    conv.i4\n", sid, nid);
        fprintf(out, "    bge        ALT_%d_%d_FAIL\n", sid, nid);
        fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldelem.ref\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.0\n    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid, sid, nid);
        fprintf(out, "    ldloca.s   V_r\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brtrue     ALT_%d_%d_LOOP\n    ldloc.0\n    ret\n  ALT_%d_%d_FAIL:\n", sid, nid, sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n");
        ec_net_beta_hdr(out);
        fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n  ALT_%d_%d_BLOOP:\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ldlen\n    conv.i4\n", sid, nid);
        fprintf(out, "    bge        ALT_%d_%d_BFAIL\n", sid, nid);
        fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldelem.ref\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.0\n    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid, sid, nid);
        fprintf(out, "    ldloca.s   V_r\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brtrue     ALT_%d_%d_BLOOP\n    ldloc.0\n    ret\n  ALT_%d_%d_BFAIL:\n", sid, nid, sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox[])\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_LEN — one function, all modes. */
static void ec_bb_len(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_xlnth — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "len_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "len");
        fprintf(out, ".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n");
        ec_jvm_init_ms_int(out, "len", "n"); ec_jvm_val_helper(out, "len");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    iadd\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpgt %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        int64_t n = nd->ival;
        fprintf(out, "function make_pat_%d_%d(ms) { const n = %ld; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        fprintf(out, "alpha() { if (ms.delta + n > ms.omega) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + n); ms.delta += n; self.succ.alpha(); return r; },\n");
        fprintf(out, "beta() { ms.delta -= n; self.fail.alpha(); }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    if (IS_NET) {
        int n = (int)nd->ival;
        ec_net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private int32 _n\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
        ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    add\n", sid, nid);
        fprintf(out, "    ldarg.1\n"); ec_net_ms_length(out); fprintf(out, "    bgt        LEN_%d_%d_FAIL\n", sid, nid);
        ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid);
        ec_net_spec_of(out); fprintf(out, "    stloc.0\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    add\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n");
        fprintf(out, "  LEN_%d_%d_FAIL:\n", sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n");
        ec_net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        ec_net_push_i4(out, n); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_POS / IR_PAT_RPOS — one function, all modes. nd->ival2 != 0 means RPOS. */
static void ec_bb_pos(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0; int rpos = (nd->ival2 != 0);
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_xposi/xrpsi — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        const char * name = rpos ? "rpos" : "pos"; char tag[32]; snprintf(tag, sizeof tag, "%s_%d_%d", name, sid, nid);
        ec_jvm_class_hdr(out, name);
        fprintf(out, ".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n");
        ec_jvm_init_ms_int(out, name, "n"); ec_jvm_val_helper(out, name);
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        if (rpos) {
            fprintf(out, "    aload_0\n    getfield bb/bb_rpos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_rpos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    aload_0\n    invokevirtual bb/bb_rpos/val()I\n    isub\n");
            fprintf(out, "    if_icmpne %s_omega\n", tag);
        } else {
            fprintf(out, "    aload_0\n    getfield bb/bb_pos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_pos/val()I\n    if_icmpne %s_omega\n", tag);
        }
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_%s/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n", name);
        fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        int64_t n = nd->ival;
        fprintf(out, "function make_pat_%d_%d(ms) { const n = %ld; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        if (rpos)
            fprintf(out, "alpha() { if (ms.delta !== ms.omega - n) { self.fail.alpha(); return; } self.succ.alpha(); return ''; },\n");
        else
            fprintf(out, "alpha() { if (ms.delta !== n) { self.fail.alpha(); return; } self.succ.alpha(); return ''; },\n");
        fprintf(out, "beta() { self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        int n = (int)nd->ival; const char * lbl = rpos ? "RPOS" : "POS";
        ec_net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private int32 _n\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
        ec_net_alpha_hdr(out); fprintf(out, "    .maxstack 3\n"); ec_net_cursor_load(out);
        if (rpos) { fprintf(out, "    ldarg.1\n"); ec_net_ms_length(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n", sid, nid); }
        else { fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid); }
        fprintf(out, "    bne.un     %s_%d_%d_FAIL\n", lbl, sid, nid);
        ec_net_cursor_load(out); ec_net_spec_zw(out); fprintf(out, "    ret\n");
        fprintf(out, "  %s_%d_%d_FAIL:\n", lbl, sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n");
        ec_net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        ec_net_push_i4(out, n); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_TAB / IR_PAT_RTAB — one function, all modes. nd->ival2 != 0 means RTAB. */
static void ec_bb_tab(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0; int rtab = (nd->ival2 != 0);
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_xtb/xrtb — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        const char * name = rtab ? "rtab" : "tab"; char tag[32]; snprintf(tag, sizeof tag, "%s_%d_%d", name, sid, nid);
        ec_jvm_class_hdr(out, name);
        fprintf(out, ".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n.field private advance I\n");
        ec_jvm_init_ms_int(out, name, "n"); ec_jvm_val_helper(out, name);
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals %d\n", rtab ? 4 : 3);
        if (rtab) {
            fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    aload_0\n    invokevirtual bb/bb_rtab/val()I\n    isub\n    istore_1\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iload_1\n    if_icmpgt %s_omega\n", tag);
            fprintf(out, "    iload_1\n    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    isub\n    istore_2\n");
            fprintf(out, "    aload_0\n    iload_2\n    putfield bb/bb_rtab/advance I\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_3\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    iload_1\n    putfield bb/bb_box$MatchState/delta I\n");
            fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_3\n    iload_2\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
            fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
            fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_rtab/advance I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
            fprintf(out, "    aconst_null\n    areturn\n.end method\n");
        } else {
            fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_tab/val()I\n    if_icmpgt %s_omega\n", tag);
            fprintf(out, "    aload_0\n    invokevirtual bb/bb_tab/val()I\n    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    isub\n    istore_1\n");
            fprintf(out, "    aload_0\n    iload_1\n    putfield bb/bb_tab/advance I\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_2\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    aload_0\n    invokevirtual bb/bb_tab/val()I\n    putfield bb/bb_box$MatchState/delta I\n");
            fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_2\n    iload_1\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
            fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
            fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_tab/advance I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
            fprintf(out, "    aconst_null\n    areturn\n.end method\n");
        }
        return;
    }
    if (IS_JS) {
        int64_t n = nd->ival;
        fprintf(out, "function make_pat_%d_%d(ms) { const n = %ld; let delta = 0; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        if (rtab)
            fprintf(out, "alpha() { const tgt = ms.omega - n; if (ms.delta > tgt) { self.fail.alpha(); return; } delta = tgt - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
        else
            fprintf(out, "alpha() { if (ms.delta > n || ms.delta > ms.omega) { self.fail.alpha(); return; } delta = n - ms.delta; if (ms.delta + delta > ms.omega) delta = ms.omega - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
        fprintf(out, "beta() { ms.delta -= delta; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        int n = (int)nd->ival; const char * lbl = rtab ? "RTAB" : "TAB";
        ec_net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private int32 _n\n  .field private int32 _advance\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
        ec_net_alpha_hdr(out);
        if (rtab) {
            fprintf(out, "    .maxstack 4\n    .locals init (int32 V_target, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
            fprintf(out, "    ldarg.1\n"); ec_net_ms_length(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n    stloc.0\n", sid, nid);
            ec_net_cursor_load(out); fprintf(out, "    ldloc.0\n    bgt        %s_%d_%d_FAIL\n", lbl, sid, nid);
            fprintf(out, "    ldarg.0\n    ldloc.0\n"); ec_net_cursor_load(out); fprintf(out, "    sub\n    stfld      int32 pat_%d_%d::_advance\n", sid, nid);
            ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n", sid, nid);
            ec_net_spec_of(out); fprintf(out, "    stloc.1\n    ldarg.1\n    ldloc.0\n");
            fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.1\n    ret\n");
        } else {
            fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
            ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    bgt        %s_%d_%d_FAIL\n", sid, nid, lbl, sid, nid);
            fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid); ec_net_cursor_load(out);
            fprintf(out, "    sub\n    stfld      int32 pat_%d_%d::_advance\n", sid, nid);
            ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n", sid, nid);
            ec_net_spec_of(out); fprintf(out, "    stloc.0\n    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid);
            fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n");
        }
        fprintf(out, "  %s_%d_%d_FAIL:\n", lbl, sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n");
        ec_net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n    sub\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        ec_net_push_i4(out, n); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_REM — one function, all modes. */
static void ec_bb_rem(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0; (void)sid;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_xstar — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        ec_jvm_class_hdr(out, "rem"); ec_jvm_init_ms_only(out, "rem");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/omega I\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iload_1\n    isub\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { const r = ms.sigma.slice(ms.delta, ms.omega); ms.delta = ms.omega; self.succ.alpha(); return r; },\n");
        fprintf(out, "beta() { self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        ec_net_class_hdr(out, sid, nid); ec_net_ctor_none(out, sid, nid);
        ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        ec_net_cursor_load(out); fprintf(out, "    ldarg.1\n"); ec_net_ms_length(out);
        ec_net_spec_of(out); fprintf(out, "    stloc.0\n    ldarg.1\n    ldarg.1\n"); ec_net_ms_length(out);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  }\n");
        ec_net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_CAT — one function, all modes. */
static void ec_bb_cat(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_flat_xcat — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "seq_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "seq");
        fprintf(out, ".field private final left Lbb/bb_box;\n.field private final right Lbb/bb_box;\n");
        fprintf(out, ".field private matched_start I\n.field private matched_len I\n");
        fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;Lbb/bb_box;)V\n    .limit stack 3\n    .limit locals 4\n");
        fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_seq/left Lbb/bb_box;\n");
        fprintf(out, "    aload_0\n    aload_3\n    putfield bb/bb_seq/right Lbb/bb_box;\n    return\n.end method\n");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    aload_0\n    getfield bb/bb_seq/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_seq/matched_start I\n");
        fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_seq/matched_len I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_seq/left Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_omega\n", tag);
        fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    putfield bb/bb_seq/matched_len I\n");
        fprintf(out, "    aload_0\n    invokevirtual bb/bb_seq/rightAlpha()Lbb/bb_box$Spec;\n    areturn\n");
        fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_seq/right Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_beta_right_omega\n", tag);
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_seq/matched_start I\n    aload_0\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_beta_right_omega:\n    aload_0\n    invokevirtual bb/bb_seq/leftBeta()Lbb/bb_box$Spec;\n    areturn\n.end method\n", tag);
        fprintf(out, ".method private rightAlpha()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_seq/right Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_rA_omega\n", tag);
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_seq/matched_start I\n    aload_0\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_rA_omega:\n    aload_0\n    invokevirtual bb/bb_seq/leftBeta()Lbb/bb_box$Spec;\n    areturn\n.end method\n", tag);
        fprintf(out, ".method private leftBeta()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_seq/left Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_lB_omega\n", tag);
        fprintf(out, "    aload_0\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    putfield bb/bb_seq/matched_len I\n");
        fprintf(out, "    aload_0\n    invokevirtual bb/bb_seq/rightAlpha()Lbb/bb_box$Spec;\n    areturn\n");
        fprintf(out, "%s_lB_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { const lr = self.left.alpha(); if (lr === null) { self.fail.alpha(); return; }\n");
        fprintf(out, "let rr = self.right.alpha(); while (rr === null) { const lr2 = self.left.beta(); if (lr2 === null) { self.fail.alpha(); return; } rr = self.right.alpha(); }\n");
        fprintf(out, "self.succ.alpha(); return rr; },\n");
        fprintf(out, "beta() { let rr = self.right.beta(); while (rr === null) { const lr = self.left.beta(); if (lr === null) { self.fail.alpha(); return; } rr = self.right.alpha(); }\n");
        fprintf(out, "return rr; }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    if (IS_NET) {
        ec_net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _left\n");
        fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _right\n");
        fprintf(out, "  .field private int32 _mStart\n  .field private int32 _mLen\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox left, class [boxes]Snobol4.Runtime.Boxes.IByrdBox right) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.2\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ret\n  }\n", sid, nid);
        ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_lr, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_rr)\n");
        fprintf(out, "    ldarg.0\n"); ec_net_cursor_load(out); fprintf(out, "    stfld      int32 pat_%d_%d::_mStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.0\n    ldloca.s   V_lr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brtrue     CAT_%d_%d_FAIL\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldloca.s   V_lr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    stfld      int32 pat_%d_%d::_mLen\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.1\n    ldloca.s   V_rr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brtrue     CAT_%d_%d_FAIL\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mLen\n", sid, nid);
        fprintf(out, "    ldloca.s   V_rr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n");
        ec_net_spec_of(out); fprintf(out, "    ret\n  CAT_%d_%d_FAIL:\n", sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n");
        ec_net_beta_hdr(out);
        fprintf(out, "    .maxstack 2\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_rr)\n");
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.0\n    ldloca.s   V_rr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brfalse    CAT_%d_%d_BNOK\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    ret\n  CAT_%d_%d_BNOK:\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mLen\n", sid, nid);
        fprintf(out, "    ldloca.s   V_rr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n");
        ec_net_spec_of(out); fprintf(out, "    ret\n  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox, class [boxes]Snobol4.Runtime.Boxes.IByrdBox)\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_FENCE — one function, all modes. x86 text/binary go via emit_flat_node/emit_bb.c. */
static void ec_bb_fence(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_JVM) {
        ec_jvm_class_hdr(out, "fence"); ec_jvm_init_ms_only(out, "fence");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_fence/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
        (void)sid; (void)nid; return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { self.succ.alpha(); return ''; },\nbeta() { self.fail.alpha(); return null; }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        ec_net_class_hdr(out, sid, nid); ec_net_ctor_none(out, sid, nid);
        ec_net_alpha_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_cursor_load(out); ec_net_spec_zw(out); fprintf(out, "    ret\n  }\n");
        ec_net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
        return;
    }
    /* IS_TEXT / IS_BIN: x86 path via emit_flat_node → emit_bb_xfnce — not wired here yet (EC-3+). */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_ABORT — one function, all modes. */
static void ec_bb_abort(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_JVM) {
        ec_jvm_class_hdr(out, "abort");
        fprintf(out, ".inner class public static final abort_exception inner bb/bb_abort$AbortException outer bb/bb_abort\n");
        ec_jvm_init_ms_only(out, "abort");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 2\n    .limit locals 1\n    new bb/bb_abort$AbortException\n    dup\n    invokespecial bb/bb_abort$AbortException/<init>()V\n    athrow\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 2\n    .limit locals 1\n    new bb/bb_abort$AbortException\n    dup\n    invokespecial bb/bb_abort$AbortException/<init>()V\n    athrow\n.end method\n");
        (void)sid; (void)nid; return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { self.fail.alpha(); return null; },\nbeta() { self.fail.alpha(); return null; }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        ec_net_class_hdr(out, sid, nid); ec_net_ctor_none(out, sid, nid);
        ec_net_alpha_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_fail_ret(out); fprintf(out, "  }\n");
        ec_net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
        return;
    }
    /* IS_TEXT / IS_BIN: x86 path — not wired here yet (EC-3+). */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-2b: IR_PAT_ASSIGN_IMM / IR_PAT_ASSIGN_COND — one function, all modes. imm=1 for IMM. */
static void ec_bb_capture(IR_t * nd, FILE * out, int imm) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_xfnme/xnme — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        (void)imm;
        ec_jvm_class_hdr(out, "capture");
        fprintf(out, ".inner interface public static abstract var_setter inner bb/bb_capture$VarSetter outer bb/bb_capture\n");
        fprintf(out, ".field private final child Lbb/bb_box;\n.field private final varname Ljava/lang/String;\n");
        fprintf(out, ".field private final immediate Z\n.field private final setter Lbb/bb_capture$VarSetter;\n");
        fprintf(out, ".field private pending_start I\n.field private pending_len I\n.field private has_pending Z\n");
        fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;Ljava/lang/String;ZLbb/bb_capture$VarSetter;)V\n    .limit stack 3\n    .limit locals 6\n");
        fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_capture/child Lbb/bb_box;\n");
        fprintf(out, "    aload_0\n    aload_3\n    putfield bb/bb_capture/varname Ljava/lang/String;\n");
        fprintf(out, "    aload_0\n    iload 4\n    putfield bb/bb_capture/immediate Z\n");
        fprintf(out, "    aload_0\n    aload 5\n    putfield bb/bb_capture/setter Lbb/bb_capture$VarSetter;\n    return\n.end method\n");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 3\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_capture/child Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n");
        fprintf(out, "    aload_0\n    aload_1\n    invokevirtual bb/bb_capture/runChild(Lbb/bb_box$Spec;)Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 3\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_capture/child Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n");
        fprintf(out, "    aload_0\n    aload_1\n    invokevirtual bb/bb_capture/runChild(Lbb/bb_box$Spec;)Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        (void)sid; (void)nid; return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { const varname = ", nd->ival, nid);
        ec_js_escape(out, nd->sval);
        fprintf(out, "; let self = { succ: null, fail: null,\n");
        fprintf(out, "alpha() { const cr = self.child.alpha(); if (cr === null) { self.fail.alpha(); return; } ms._do_capture(cr, varname, %d); self.succ.alpha(); return cr; },\n", imm);
        fprintf(out, "beta() { const cr = self.child.beta(); if (cr === null) { self.fail.alpha(); return; } ms._do_capture(cr, varname, %d); return cr; }\n", imm);
        fprintf(out, "}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * varname = nd->sval ? nd->sval : "";
        ec_net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _child\n  .field private string _varname\n  .field private bool _immediate\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox child, string varname, bool imm) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.2\n    dup\n    brtrue     CAP_%d_%d_NN\n    pop\n    ldstr      \"\"\n  CAP_%d_%d_NN:\n    stfld      string pat_%d_%d::_varname\n", sid, nid, sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.3\n    stfld      bool pat_%d_%d::_immediate\n    ret\n  }\n", sid, nid);
        ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_cr, int32 V_start, int32 V_len, string V_matched)\n");
        fprintf(out, "    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stloc.1\n");
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.0\n    ldloca.s   V_cr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n    brtrue     CAPC_%d_%d_FAIL\n", sid, nid);
        fprintf(out, "    ldloca.s   V_cr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Start\n    stloc.1\n");
        fprintf(out, "    ldloca.s   V_cr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    stloc.2\n");
        fprintf(out, "    ldarg.1\n    callvirt   instance string [boxes]Snobol4.Runtime.Boxes.MatchState::get_Subject()\n");
        fprintf(out, "    ldloc.1\n    ldloc.2\n    callvirt   instance string [mscorlib]System.String::Substring(int32, int32)\n    stloc.3\n");
        fprintf(out, "    ldarg.0\n    ldfld      string pat_%d_%d::_varname\n", sid, nid);
        fprintf(out, "    ldstr      \"OUTPUT\"\n    call       bool [mscorlib]System.String::op_Equality(string, string)\n");
        fprintf(out, "    brfalse    CAPC_%d_%d_NOTOUT\n    ldloc.3\n    call       void [mscorlib]System.Console::WriteLine(string)\n", sid, nid);
        fprintf(out, "    br         CAPC_%d_%d_DONE\n  CAPC_%d_%d_NOTOUT:\n  CAPC_%d_%d_DONE:\n    ldloc.0\n    ret\n", sid, nid, sid, nid, sid, nid);
        fprintf(out, "  CAPC_%d_%d_FAIL:\n", sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n");
        ec_net_beta_hdr(out);
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n    ret\n  }\n}\n");
        ec_net_escape_ldstr(out, varname); ec_net_push_i4(out, imm);
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox, string, bool)\n", sid, nid);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* emit_bb_node — one call per BB kind; each ec_bb_* function handles all modes internally.
   EC-2b COMPLETE: 18 kinds, each with IS_TEXT/IS_BIN/IS_JVM/IS_JS/IS_NET arms in one function. */
int emit_bb_node(IR_t * nd, FILE * out) {
    if (!nd) return 1;
    switch (nd->t) {
    case IR_PAT_LIT:         ec_bb_lit(nd, out);         return 0;
    case IR_PAT_ANY:         ec_bb_any(nd, out);         return 0;
    case IR_PAT_NOTANY:      ec_bb_notany(nd, out);      return 0;
    case IR_PAT_SPAN:        ec_bb_span(nd, out);        return 0;
    case IR_PAT_BREAK:       ec_bb_break(nd, out);       return 0;
    case IR_PAT_ARB:         ec_bb_arb(nd, out);         return 0;
    case IR_PAT_ARBNO:       ec_bb_arbno(nd, out);       return 0;
    case IR_PAT_CAT:         ec_bb_cat(nd, out);         return 0;
    case IR_PAT_ALT:         ec_bb_alt(nd, out);         return 0;
    case IR_PAT_LEN:         ec_bb_len(nd, out);         return 0;
    case IR_PAT_POS:         ec_bb_pos(nd, out);         return 0;
    case IR_PAT_TAB:         ec_bb_tab(nd, out);         return 0;
    case IR_PAT_REM:         ec_bb_rem(nd, out);         return 0;
    case IR_PAT_FENCE:       ec_bb_fence(nd, out);       return 0;
    case IR_PAT_ABORT:       ec_bb_abort(nd, out);       return 0;
    case IR_PAT_ASSIGN_IMM:  ec_bb_capture(nd, out, 1); return 0;
    case IR_PAT_ASSIGN_COND: ec_bb_capture(nd, out, 0); return 0;
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
