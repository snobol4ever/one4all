/* emit_text.c — RW-1: TEXT-mode helpers with new names.
 *
 * Thin wrappers over emit_text3c.c internals. No logic duplicated.
 * Old emit_text3c.c still compiled alongside — no callers changed in RW-1.
 */

#include "emit_text.h"
#include "emit_text3c.h"
#include "emit_mode.h"
#include "insn.h"
#include "emit_mode.h"
#include "emit_label.h"
#include <stdio.h>
#include <stdarg.h>

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
void emit_text_label(bb_label_t *lbl) { bb_text_label(lbl); }
void emit_text_comment(const char *fmt, ...) {
    if (bb_emit_mode != EMIT_TEXT) return;
    fprintf(emit_outf(), "    # ");
    va_list ap; va_start(ap, fmt); vfprintf(emit_outf(), fmt, ap); va_end(ap);
    fputc('\n', emit_outf());
}
void emit_text_box_banner(const char *kind, const char *args) {
    emit_bb_box_banner(kind, args);
}
void emit_text_stno_banner(int stno, int lineno, const char *src_text) {
    emit_banner_stno(stno, lineno, src_text);
}
void emit_text_global(const char *name) {
    if (!IS_TEXT) return;
    bb3c_flush_pending_cjmp_only();
    fprintf(emit_outf(), "    .global %s\n", name ? name : "");
}
