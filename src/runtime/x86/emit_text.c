/* emit_text.c — RW-6: TEXT-mode helpers (emit_text3c.c absorbed).
 *
 * All bb3c_* internals live here now. emit_text_* are new-name wrappers.
 * Old emit_text3c.c deleted at RW-6.
 */

#include "emit_text.h"
#include "emit_text3c.h"
#include "emit_mode.h"
#include "emit_label.h"
#include "insn.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/*========================================================================*/
/* bb3c internals (was emit_text3c.c)                                     */
/*========================================================================*/

static char  g_bb3c_pending_label[256] = "";
static FILE *g_bb3c_pending_out          = NULL;

static char  g_bb3c_pending_cjmp_mn[16]      = "";
static char  g_bb3c_pending_cjmp_target[256] = "";
static FILE *g_bb3c_pending_cjmp_out         = NULL;

#define BB_COL3_WIDTH 27

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
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

/*========================================================================*/
/* emit_text_* new-name API                                               */
/*========================================================================*/

void emit_text_3col(FILE *out, const char *label, const char *action, const char *goto_) {
    bb3c_format(out, label, action, goto_);
}
void emit_text_jmp(FILE *out, const char *mn, const char *target) {
    bb3c_emit_jmp(out, mn, target);
}
void emit_text_op(const char *label, const char *action, const char *goto_) {
    bb3c_text(label, action, goto_);
}
void emit_text_flush_cjmp(void) { bb3c_flush_pending_cjmp_only(); }
void emit_text_flush(void)      { bb3c_flush_pending(); }

void emit_text_rawf(const char *fmt, ...) {
    if (bb_emit_mode != EMIT_TEXT) return;
    va_list ap; va_start(ap, fmt); vfprintf(emit_outf(), fmt, ap); va_end(ap);
}
void emit_text_label(bb_label_t *lbl)      { bb_text_label(lbl); }
void emit_text_comment(const char *fmt, ...) {
    if (bb_emit_mode != EMIT_TEXT) return;
    fprintf(emit_outf(), "    # ");
    va_list ap; va_start(ap, fmt); vfprintf(emit_outf(), fmt, ap); va_end(ap);
    fputc('\n', emit_outf());
}
void emit_text_box_banner(const char *kind, const char *args) { emit_bb_box_banner(kind, args); }
void emit_text_stno_banner(int stno, int lineno, const char *src_text) { emit_banner_stno(stno, lineno, src_text); }
void emit_text_global(const char *name) {
    if (!IS_TEXT) return;
    bb3c_flush_pending_cjmp_only();
    fprintf(emit_outf(), "    .global %s\n", name ? name : "");
}
