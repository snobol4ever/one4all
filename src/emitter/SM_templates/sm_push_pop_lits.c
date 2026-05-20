#include "sm_template_common.h"
#include "emit_sm.h"
/* EC-UNI-8: every template fn carries an arm or `n/a` comment for each cell of the
 *           backend × mode matrix. JS_BIN is the canonical n/a — JS has no binary form.
 *           *_BIN arms for JVM/NET/WASM are no-op stubs; EC-UNI-6/7 will wire bytes.
 */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_push_lit_i — SM_PUSH_LIT_I: push 64-bit integer literal onto the SNOBOL4 value stack. */
void sm_push_lit_i(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_push_lit_i_line(out, instr, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path. Today: legacy emit_walk_codegen handles. */ return; }
    if (IS_JVM_TEXT) { jvm_push_int2(out, (long)instr->a[0].i); fprintf(out, "    i2l\n    invokestatic rt/SnoRt/push_int(J)V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT)  { fprintf(out, "rt.push_int(%lld); ", (long long)instr->a[0].i); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_NET_TEXT) { fprintf(out, "    ldc.i4 %lld\n    call       void SnoRt::push_int(int32)\n", (long long)instr->a[0].i); return; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }
    if (IS_WASM_TEXT){ fprintf(out, "          (call $sno_push_int (i32.const %lld))\n", (long long)instr->a[0].i); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_push_lit_s — SM_PUSH_LIT_S / SM_PUSH_LIT_CS: push string literal. */
void sm_push_lit_s(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_push_lit_s_dispatch(out, instr, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed */ return; }
    const char * s = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM_TEXT) { jvm_emit_ldc_string(out, s); fprintf(out, "    invokestatic rt/SnoRt/push_str(Ljava/lang/String;)V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_JS_TEXT)  { fprintf(out, "rt.push_str("); js_escape(out, s); fprintf(out, ", %d); ", (int)strlen(s)); return; }
    /* IS_JS_BIN: n/a */
    if (IS_NET_TEXT) { fprintf(out, "    ldstr      "); js_escape(out, s); fprintf(out, "\n    ldc.i4     %d\n    call       void SnoRt::push_str(string, int32)\n", (int)strlen(s)); return; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_WASM_TEXT){ int addr = wasm_intern_str(s); fprintf(out, "          (call $sno_push_str (i32.const 0x%x) (i32.const %d))\n", addr, (int)strlen(s)); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_push_lit_f — SM_PUSH_LIT_F: push double-precision float literal. */
void sm_push_lit_f(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_push_lit_f_dispatch(out, instr, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed */ return; }
    if (IS_JVM_TEXT) { fprintf(out, "    ldc2_w %.17g\n    invokestatic rt/SnoRt/push_real(D)V\n", instr->a[0].f); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_JS_TEXT)  { fprintf(out, "rt.push_real_bits(%.17g); ", instr->a[0].f); return; }
    /* IS_JS_BIN: n/a */
    if (IS_NET_TEXT) { fprintf(out, "    ldc.r8     %.17g\n    call       void SnoRt::push_real(float64)\n", instr->a[0].f); return; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_WASM_TEXT){ fprintf(out, "          (call $sno_push_real (f64.const %.17g))\n", instr->a[0].f); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_push_null — SM_PUSH_NULL / SM_PUSH_NULL_NOFLIP: push null/empty-string value. */
void sm_push_null(const SM_Instr * instr, FILE * out) {
    (void)instr;
    if (IS_X86_TEXT) { emit_sm_push_null_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed */ return; }
    if (IS_JVM_TEXT) { fprintf(out, "    invokestatic rt/SnoRt/push_null()V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_JS_TEXT)  { fprintf(out, "rt.push_null(); "); return; }
    /* IS_JS_BIN: n/a */
    if (IS_NET_TEXT) { fprintf(out, "    call       void SnoRt::push_null()\n"); return; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_WASM_TEXT){ fprintf(out, "          (call $sno_push_null)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_void_pop — SM_VOID_POP: discard top-of-stack value without binding. */
void sm_void_pop(const SM_Instr * instr, FILE * out) {
    (void)instr;
    if (IS_X86_TEXT) { emit_sm_pop(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed */ return; }
    if (IS_JVM_TEXT) { fprintf(out, "    invokestatic rt/SnoRt/pop_void()V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_JS_TEXT)  { fprintf(out, "rt.pop_void(); "); return; }
    /* IS_JS_BIN: n/a */
    if (IS_NET_TEXT) { fprintf(out, "    call       void SnoRt::pop_void()\n"); return; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_WASM_TEXT){ fprintf(out, "          (call $sno_pop_void)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_push_var — SM_PUSH_VAR: push current value of named variable onto stack. */
void sm_push_var(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_push_var_dispatch(out, instr, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed */ return; }
    const char * s = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM_TEXT) { jvm_emit_ldc_string(out, s); fprintf(out, "    invokestatic rt/SnoRt/push_var(Ljava/lang/String;)V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_JS_TEXT)  { fprintf(out, "rt.push_var("); js_escape(out, s); fprintf(out, "); "); return; }
    /* IS_JS_BIN: n/a */
    if (IS_NET_TEXT) { net_escape_ldstr(out, s); fprintf(out, "    call       void SnoRt::push_var(string)\n"); return; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_WASM_TEXT){ int addr = wasm_intern_name(s); fprintf(out, "          (call $sno_push_var (i32.const 0x%x) (i32.const %d))\n", addr, (int)strlen(s)); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_store_var — SM_STORE_VAR: pop top of stack and store into named variable. */
void sm_store_var(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_store_var_dispatch(out, instr, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed */ return; }
    const char * s = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM_TEXT) { jvm_emit_ldc_string(out, s); fprintf(out, "    invokestatic rt/SnoRt/store_var(Ljava/lang/String;)V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_JS_TEXT)  { fprintf(out, "rt.store_var("); js_escape(out, s); fprintf(out, "); "); return; }
    /* IS_JS_BIN: n/a */
    if (IS_NET_TEXT) { net_escape_ldstr(out, s); fprintf(out, "    call       void SnoRt::store_var(string)\n"); return; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed */ return; }
    if (IS_WASM_TEXT){ int addr = wasm_intern_name(s); fprintf(out, "          (call $sno_store_var (i32.const 0x%x) (i32.const %d))\n", addr, (int)strlen(s)); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed */ return; }
}
