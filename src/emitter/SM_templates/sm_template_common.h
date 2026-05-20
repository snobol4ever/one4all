/* sm_template_common.h — included by every SM_templates/sm_*.c file.
   Provides standard includes, mode macros, and shared helper prototypes.
   Do NOT include this from emit_core.c — it lives in SM_templates/ only. */
#pragma once
#include "emit_core.h"
#include "SM.h"
#include "emit_globals.h"
#include "emit_io.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* Mode test macros (defined in emit_core.h): IS_JVM, IS_JS, IS_NET, IS_WASM */
/* EC-3 JVM helpers declared in emit_core.h: jvm_push_int2, jvm_emit_ldc_string */
/* EC-3 JS helper: js_escape (declared in bb_template_common.h, defined in emit_core.c) */
void js_escape(FILE * out, const char * s);
/* EC-3 NET helper: net_escape_ldstr (defined in emit_core.c) */
void net_escape_ldstr(FILE * out, const char * s);
/* EC-3 NET helper: net_push_i4 (defined in emit_core.c) */
void net_push_i4(FILE * out, int v);
/* EC-WASM helpers: intern string/name into the WASM linear-memory string table (emit_core.c) */
int wasm_intern_str(const char * s);
int wasm_intern_name(const char * s);

/* EC-UNI-13(b): WasmUserFn type + lookup, exposed for SM_templates/sm_calls.c.
   Type defined here so the SM_CALL_FN/SM_SUSPEND_VALUE WASM arm (which performs
   user-fn frame setup) can read fn->{name,entry_pc,nparams,params[]} verbatim. */
#define WASM_USERFNS_MAX 256
#define WASM_MAX_PARAMS  16
typedef struct { char name[128]; int entry_pc; int nparams; char params[WASM_MAX_PARAMS][128]; } WasmUserFn;
WasmUserFn * wasm_userfn_find(const char * name);

/* EC-UNI-13(b): JVM identifier sanitiser, exposed for SM_templates/sm_calls.c. */
void jvm_sanitize_name(char * dst, size_t dsz, const char * src);

/* EC-UNI-13(b): JS string-literal escaper, exposed for SM_templates/sm_calls.c.
   js_escape (declared above) escapes raw characters; js_escape_string wraps with
   double-quotes and escapes the body. */
void js_escape_string(FILE * out, const char * s);

/* EC-UNI-8.1: shared inline helpers, moved from per-family-file `static` to
 * `static inline` so each per-opcode split TU can use them without an external
 * link symbol. Each helper takes its own copy in every TU that uses it (zero
 * extra symbols at link time, zero behavioural change).
 *
 * EC-UNI-10(b): Layer-2 ret-guard helpers now read from g_emit; `op` and `sfx`
 * stay as parameters because they vary per call site (multiple op-variants
 * share one helper). */

/* JVM last_ok guard for _S/_F return variants. */
static inline void jvm_ret_guard(int op_s, int op_f, int op, const char * sfx) {
    FILE * out = g_emit.out;
    int i = g_emit.i;
    if (op == op_s) { fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n    ifeq sm_pc_%d_%s_skip\n", i, sfx); }
    if (op == op_f) { fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n    ifne sm_pc_%d_%s_skip\n", i, sfx); }
}
/* NET last_ok guard for _S/_F return variants. */
static inline void net_ret_guard(int op_s, int op_f, int op) {
    FILE * out = g_emit.out;
    int i = g_emit.i;
    if (op == op_s) { fprintf(out, "    call       bool SnoRt::last_ok()\n    brfalse    NET_L%d\n", i + 1); }
    if (op == op_f) { fprintf(out, "    call       bool SnoRt::last_ok()\n    brtrue     NET_L%d\n",  i + 1); }
}
/* JVM pat-helpers (were static in sm_pat.c). */
static inline void jvm_pat_str_push(FILE * out, int i, const char * tag, const char * method) {
    fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
    fprintf(out, "    dup\n    ifnonnull pat_%s_nn_%d\n    pop\n    ldc \"\"\n    goto pat_%s_done_%d\n", tag, i, tag, i);
    fprintf(out, "pat_%s_nn_%d:\n    invokevirtual java/lang/Object/toString()Ljava/lang/String;\n", tag, i);
    fprintf(out, "pat_%s_done_%d:\n    invokestatic rt/SnoPat/%s\n", tag, i, method);
    fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n");
}
static inline void jvm_pat_long_push(FILE * out, const char * method) {
    fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
    fprintf(out, "    invokestatic rt/SnoRt/coerce_to_long(Ljava/lang/Object;)J\n");
    fprintf(out, "    invokestatic rt/SnoPat/%s\n", method);
    fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n");
}
static inline void jvm_pat_noarg_push(FILE * out, const char * method) {
    fprintf(out, "    invokestatic rt/SnoPat/%s\n", method);
    fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n");
}
static inline void jvm_pat_pat_push(FILE * out, const char * method) {
    fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
    fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
    fprintf(out, "    invokestatic rt/SnoPat/%s\n", method);
    fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n");
}
static inline void jvm_pat_2pat_push(FILE * out, const char * method) {
    fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
    fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
    fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
    fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
    fprintf(out, "    swap\n");
    fprintf(out, "    invokestatic rt/SnoPat/%s\n", method);
    fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n");
}
