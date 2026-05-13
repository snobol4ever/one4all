/* emit_label.h — RW-1: label lifecycle, new names.
 *
 * Consolidates label symbols currently split across emit_label.c and
 * emit_form.c. New names alongside old — no callers changed in RW-1.
 */

#ifndef EMIT_LABEL_NEW_H
#define EMIT_LABEL_NEW_H

/* Avoid collision with old emit_label.h guard */
#include "emit_defs.h"
#include <stdarg.h>

/* ---- Label init -------------------------------------------------------- */
void emit_label_init (bb_label_t *lbl, const char *name);
void emit_label_initf(bb_label_t *lbl, const char *fmt, ...);

/* ---- Label define (binary: resolve patches; text: emit "name:") -------- */
/* Note: emit_label_define already exists in emit_mode.c with this exact    */
/* signature and behaviour. No new function needed — just use it.           */
/* Declared here for documentation completeness; defined in emit_mode.c.    */
void emit_label_define(bb_label_t *lbl);

/* ---- Convenience macro ------------------------------------------------- */
#define emit_label_ok(lbl)  ((lbl)->offset != BB_LABEL_UNRESOLVED)

#endif /* EMIT_LABEL_NEW_H */
