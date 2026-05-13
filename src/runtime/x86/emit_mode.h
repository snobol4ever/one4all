/* emit_mode.h — L2: emit mode globals, macro begin/end, format-port helpers.
 *
 * Owns: bb_emit_mode, bb_emit_out, g_bb_emit_format, g_in_text_macro_body,
 *       emit_mode_set, emit_outf, emit_bb_is_format_mode, fmt_body_append,
 *       emit_bb_format_port, emit_pad_to_blob_size,
 *       emit_macro_begin, emit_macro_end,
 *       bb3c_op, bb3c_jmp.
 *
 * Called by: emit_bb_gen.c (L3 compound helpers), emit_sm_op.c / emit_bb_box.c (L4 templates).
 * Calls:     emit_buf.h (L0), emit_text3c.h (L2), emit_label.h (L2).
 */

#ifndef EMIT_MODE_H
#define EMIT_MODE_H

#include "emit_defs.h"
#include <stdio.h>

/* ---- globals ---------------------------------------------------------- */
extern bb_emit_mode_t  bb_emit_mode;
extern FILE           *bb_emit_out;
extern int             g_bb_emit_format;
extern int             g_in_text_macro_body;

/* ---- mode lifecycle --------------------------------------------------- */
void emit_mode_set(bb_emit_mode_t m, FILE *out);
FILE *emit_outf(void);

/* ---- format-port helpers ---------------------------------------------- */
int  emit_bb_is_format_mode(void);
void fmt_body_append(const char *instr, const char *operands);
void emit_bb_format_port(bb_label_t *lbl_entry, const char *macro_name, const char *args);

/* ---- padding ---------------------------------------------------------- */
void emit_pad_to_blob_size(void);

/* ---- macro begin/end -------------------------------------------------- */
void emit_macro_begin(const char *name, const char *const *params, int nparams);
void emit_macro_end(void);

/* ---- TEXT-mode convenience wrappers ----------------------------------- */
void bb3c_op (const char *mn, const char *fmt, ...);
void bb3c_jmp(const char *mn, const char *target);

/* ---- jump + label-define (use fmt helpers when in format mode) -------- */
void emit_jmp          (bb_label_t *target, jmp_kind_t kind);
void emit_label_define (bb_label_t *lbl);

#endif /* EMIT_MODE_H */
