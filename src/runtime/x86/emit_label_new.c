/* emit_label_new.c — RW-6: label lifecycle, standalone (emit_label.c absorbed).
 *
 * bb_label_init / bb_label_initf / bb_label_define live here now.
 * emit_label_init / emit_label_initf are thin new-name aliases.
 * emit_label_define is in emit_mode.c (uses fmt_label_save).
 */

#include "emit_label_new.h"
#include "emit_label.h"
#include "emit_mode.h"
#include "emit_defs.h"
#include "emit_buf.h"
#include "emit_text3c.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_label_init(bb_label_t *lbl, const char *name)
{
    strncpy(lbl->name, name, BB_LABEL_NAME_MAX - 1);
    lbl->name[BB_LABEL_NAME_MAX - 1] = '\0';
    lbl->offset = BB_LABEL_UNRESOLVED;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_label_initf(bb_label_t *lbl, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(lbl->name, BB_LABEL_NAME_MAX, fmt, ap);
    va_end(ap);
    lbl->offset = BB_LABEL_UNRESOLVED;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_label_define(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        FILE *f = bb_emit_out ? bb_emit_out : stdout;
        char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", lbl->name);
        bb3c_format(f, lbuf, "", "");
        return;
    }
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_label_init(bb_label_t *lbl, const char *name)  { bb_label_init(lbl, name); }
void emit_label_initf(bb_label_t *lbl, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vsnprintf(lbl->name, BB_LABEL_NAME_MAX, fmt, ap);
    va_end(ap);
    lbl->offset = BB_LABEL_UNRESOLVED;
}
