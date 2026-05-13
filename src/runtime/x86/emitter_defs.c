/*
 * emitter_defs.c — macro-definition mode initialiser.
 *
 * emitter_init_macro_def(out) is a thin wrapper around emitter_init_text
 * in DEFINITION mode.  Used by the sm_macros.s / bb_macros.s regen path.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-DEVTABLE / GOAL-MODE4-EMIT
 */

#include "emitter.h"

void emitter_init_macro_def(FILE *out)
{
    emitter_init_text(out, TEXT_MODE_DEFINITION);
}
