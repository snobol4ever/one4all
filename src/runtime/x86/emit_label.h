#ifndef EMIT_LABEL_H
#define EMIT_LABEL_H

#include "emit_defs.h"
#include "emit_buf.h"
#include <stdarg.h>

/* bb_label_init/initf — initialise a label (unresolved) */
void bb_label_init (bb_label_t *lbl, const char *name);
void bb_label_initf(bb_label_t *lbl, const char *fmt, ...);

/* bb_label_define — define label at current position; backpatch pending refs */
void bb_label_define(bb_label_t *lbl);

#endif /* EMIT_LABEL_H */
