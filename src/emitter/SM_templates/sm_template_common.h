/* sm_template_common.h — included by every SM_templates/sm_*.c file.
   Provides standard includes, mode macros, and shared helper prototypes.
   Do NOT include this from emit_core.c — it lives in SM_templates/ only. */
#pragma once
#include "emit_core.h"
#include "sm_prog.h"
#include "sm_ctx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* Mode test macros (defined in emit_core.h): IS_JVM, IS_JS, IS_NET */
/* EC-3 JVM helpers declared in emit_core.h: jvm_push_int2, jvm_emit_ldc_string */
/* EC-3 JS helper: js_escape (declared in bb_template_common.h, defined in emit_core.c) */
void js_escape(FILE * out, const char * s);
/* EC-3 NET helper: net_escape_ldstr (defined in emit_core.c) */
void net_escape_ldstr(FILE * out, const char * s);
/* EC-3 NET helper: net_push_i4 (defined in emit_core.c) */
void net_push_i4(FILE * out, int v);
