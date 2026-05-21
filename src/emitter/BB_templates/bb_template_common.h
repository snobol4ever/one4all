/* bb_template_common.h — included by every BB_templates/bb_*.c file.
   Provides the standard includes, mode macros, and shared helper prototypes.
   Do NOT include this from emit_core.c — it lives in BB_templates/ only. */
#pragma once
#include "emit_core.h"
#include "emit_globals.h"
#include "emit_io.h"
#include "sm_jit_interp.h"
#include "emit_form.h"
#include "emit_ir.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Mode test macros (defined in emit_core.h): IS_TEXT, IS_BIN, IS_JVM, IS_JS, IS_NET */

/* Shared helpers defined in emit_core.c — not static, linkable from template TUs. */
void jvm_class_hdr(FILE *out, const char *name);
void jvm_alpha_method_hdr(FILE *out, int stack, int locals);
void jvm_beta_method_hdr(FILE *out, int stack, int locals);
void jvm_init_ms_only(FILE *out, const char *name);
void jvm_init_ms_int(FILE *out, const char *name, const char *field);
void jvm_init_ms_str(FILE *out, const char *name, const char *field);
void jvm_val_helper(FILE *out, const char *name);
void net_class_hdr(FILE *out, int sid, int nid);
void net_alpha_hdr(FILE *out);
void net_beta_hdr(FILE *out);
void net_cursor_load(FILE *out);
void net_ms_length(FILE *out);
void net_spec_of(FILE *out);
void net_fail_ret(FILE *out);
void net_push_i4(FILE *out, int v);
void net_ctor_none(FILE *out, int sid, int nid);
void net_spec_zw(FILE *out);
void net_charset_class(FILE *out, int sid, int nid, const char *tag);
void net_escape_ldstr(FILE *out, const char *s);
void js_escape(FILE *out, const char *s);
