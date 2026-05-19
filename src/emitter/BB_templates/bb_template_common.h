/* bb_template_common.h — included by every BB_templates/ec_bb_*.c file.
   Provides the standard includes, mode macros, and shared helper prototypes.
   Do NOT include this from emit_core.c — it lives in BB_templates/ only. */
#pragma once
#include "emit_core.h"
#include "sm_jit_interp.h"
#include "emit_form.h"
#include "emit_ir.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Mode test macros (defined in emit_core.h): IS_TEXT, IS_BIN, IS_JVM, IS_JS, IS_NET */

/* Shared helpers defined in emit_core.c — not static, linkable from template TUs. */
void ec_jvm_class_hdr(FILE *out, const char *name);
void ec_jvm_init_ms_only(FILE *out, const char *name);
void ec_jvm_init_ms_int(FILE *out, const char *name, const char *field);
void ec_jvm_init_ms_str(FILE *out, const char *name, const char *field);
void ec_jvm_val_helper(FILE *out, const char *name);
void ec_net_class_hdr(FILE *out, int sid, int nid);
void ec_net_alpha_hdr(FILE *out);
void ec_net_beta_hdr(FILE *out);
void ec_net_cursor_load(FILE *out);
void ec_net_ms_length(FILE *out);
void ec_net_spec_of(FILE *out);
void ec_net_fail_ret(FILE *out);
void ec_net_push_i4(FILE *out, int v);
void ec_net_ctor_none(FILE *out, int sid, int nid);
void ec_net_spec_zw(FILE *out);
void ec_net_charset_class(FILE *out, int sid, int nid, const char *tag);
void ec_net_escape_ldstr(FILE *out, const char *s);
void ec_js_escape(FILE *out, const char *s);
