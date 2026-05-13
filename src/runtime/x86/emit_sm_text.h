
#ifndef EMIT_SM_TEXT_H
#define EMIT_SM_TEXT_H

#include "sm_prog.h"
#include <stdio.h>

/* sm_codegen_text — emit SM_Program as GNU-as .s to `out`. */
int sm_codegen_text(SM_Program *prog, FILE *out, const char *src_path);


extern int g_jit_emit_inline;

#endif
