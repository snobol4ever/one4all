/* emit_text.h — RW-1: TEXT-mode 3-col formatter and banner helpers.
 *
 * Replaces emit_text3c.h. New names for all public symbols.
 * Old emit_text3c.h still compiled alongside — no callers changed in RW-1.
 */

#ifndef EMIT_TEXT_H
#define EMIT_TEXT_H

#include "emit_defs.h"
#include <stdio.h>

/* ---- 3-col core ------------------------------------------------------- */
/* emit_text_3col: write one L/A/G line; handles pending-label fusion.     */
void emit_text_3col    (FILE *out, const char *label, const char *action, const char *goto_);

/* emit_text_jmp: write a 3-col jmp line; pairs cond+uncond on one line.   */
void emit_text_jmp     (FILE *out, const char *mn, const char *target);

/* emit_text_op: guard (TEXT mode only) then emit_text_3col.               */
void emit_text_op      (const char *label, const char *action, const char *goto_);

/* ---- Flush helpers ----------------------------------------------------- */
void emit_text_flush_cjmp (void);   /* flush pending conditional jmp only  */
void emit_text_flush      (void);   /* flush pending cjmp + pending label   */

/* ---- Raw output -------------------------------------------------------- */
void emit_text_rawf    (const char *fmt, ...);   /* vfprintf to out (TEXT only) */
void emit_text_label   (bb_label_t *lbl);        /* TEXT: emit label; BIN: define */
void emit_text_comment (const char *fmt, ...);   /* TEXT: # comment line        */

/* ---- Banners ----------------------------------------------------------- */
void emit_text_box_banner (const char *kind, const char *args);  /* #--- BOX … */
void emit_text_stno_banner(int stno, int lineno, const char *src_text); /* #=== stmt */

#endif /* EMIT_TEXT_H */
