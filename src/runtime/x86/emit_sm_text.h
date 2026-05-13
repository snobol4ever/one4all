/*
 * emit_sm_text.h — SM_Program → GNU-as .s text (mode 4, --jit-emit --x64)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#ifndef EMIT_SM_TEXT_H
#define EMIT_SM_TEXT_H

#include "sm_prog.h"
#include <stdio.h>

/* sm_codegen_text — emit SM_Program as GNU-as .s to `out`.
 * src_path is optional (used for source-line annotations); may be NULL.
 * Returns 0 on success, -1 on error. */
int sm_codegen_text(SM_Program *prog, FILE *out, const char *src_path);

/* EDP-2: set by scrip.c --jit-emit-inline; read by sm_codegen_text pipeline. */
extern int g_jit_emit_inline;

#endif /* EMIT_SM_TEXT_H */
