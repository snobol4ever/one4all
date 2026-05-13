/* emit_label_new.c — RW-1: label lifecycle with new names.
 *
 * emit_label_init / emit_label_initf are thin wrappers over
 * bb_label_init / bb_label_initf from emit_label.c.
 * emit_label_define already exists in emit_mode.c — no wrapper needed.
 *
 * Old emit_label.c still compiled alongside — no callers changed in RW-1.
 */

#include "emit_label_new.h"
#include "emit_label.h"
#include <stdarg.h>

void emit_label_init(bb_label_t *lbl, const char *name) {
    bb_label_init(lbl, name);
}
void emit_label_initf(bb_label_t *lbl, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    bb_label_initf(lbl, fmt, ap);
    va_end(ap);
}
