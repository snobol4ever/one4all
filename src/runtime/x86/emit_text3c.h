#ifndef EMIT_TEXT3C_H
#define EMIT_TEXT3C_H

#include "emit_defs.h"
#include <stdio.h>
#include <stdarg.h>

/* 3-column GAS line formatter */
void bb3c_format(FILE *out, const char *label, const char *action, const char *goto_);
void bb3c_text  (const char *label, const char *action, const char *goto_);
void bb3c_emit_jmp(FILE *out, const char *mn, const char *target);
void bb3c_flush_pending(void);
void bb3c_flush_pending_cjmp_only(void);

/* Text-mode raw output helpers */
void bb_text        (const char *fmt, ...);
void bb_text_label  (bb_label_t *lbl);
void bb_text_comment(const char *fmt, ...);

/* Banner and annotation emitters */
void emit_comment      (const char *text);
void emit_bb_box_banner(const char *kind, const char *args);
void emit_banner_stno  (int stno, int lineno, const char *src_text);

#endif /* EMIT_TEXT3C_H */
