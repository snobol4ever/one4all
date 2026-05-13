/* emit_mode.c — L2: emit mode globals, macro begin/end, format-port helpers.
 *
 * Owns the global emit-mode state and the functions that inspect or change it.
 * Template functions (L4) and compound BB helpers (L3) read bb_emit_mode to
 * select TEXT vs BINARY vs MACRO_DEF output paths.
 */

#include "emit_mode.h"
#include "emit_defs.h"
#include "emit_buf.h"
#include "emit_text3c.h"
#include "emit_label.h"

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
        case JMP_JMP: bb_insn_jmp_rel32(target);  return;
        case JMP_JE:  bb_insn_je_rel32(target);   return;
        case JMP_JNE: bb_insn_jne_rel32(target);  return;
        case JMP_JL:  bb_insn_jl_rel32(target);   return;
        case JMP_JGE: bb_insn_jge_rel32(target);  return;
        case JMP_JG:  bb_insn_jg_rel32(target);   return;
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
