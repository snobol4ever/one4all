#include "sm_template_common.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_push_lit_i(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_push_lit_i_line(out, instr, 0); return; }
    if (IS_JVM) { jvm_push_int2(out, (long)instr->a[0].i); fprintf(out, "    i2l\n    invokestatic rt/SnoRt/push_int(J)V\n"); return; }
    if (IS_JS)  { fprintf(out, "rt.push_int(%lld); ", (long long)instr->a[0].i); return; }
    if (IS_NET) { fprintf(out, "    ldc.i4 %lld\n    call       void SnoRt::push_int(int32)\n", (long long)instr->a[0].i); return; }
    if (IS_WASM){ fprintf(out, "          (call $sno_push_int (i32.const %lld))\n", (long long)instr->a[0].i); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_push_lit_s(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_push_lit_s_dispatch(out, instr, 0); return; }
    const char * s = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM) { jvm_emit_ldc_string(out, s); fprintf(out, "    invokestatic rt/SnoRt/push_str(Ljava/lang/String;)V\n"); return; }
    if (IS_JS)  { fprintf(out, "rt.push_str("); js_escape(out, s); fprintf(out, ", %d); ", (int)strlen(s)); return; }
    if (IS_NET) { fprintf(out, "    ldstr      "); js_escape(out, s); fprintf(out, "\n    ldc.i4     %d\n    call       void SnoRt::push_str(string, int32)\n", (int)strlen(s)); return; }
    if (IS_WASM){ int addr = wasm_intern_str(s); fprintf(out, "          (call $sno_push_str (i32.const 0x%x) (i32.const %d))\n", addr, (int)strlen(s)); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_push_lit_f(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_push_lit_f_dispatch(out, instr, 0); return; }
    if (IS_JVM) { fprintf(out, "    ldc2_w %.17g\n    invokestatic rt/SnoRt/push_real(D)V\n", instr->a[0].f); return; }
    if (IS_JS)  { fprintf(out, "rt.push_real_bits(%.17g); ", instr->a[0].f); return; }
    if (IS_NET) { fprintf(out, "    ldc.r8     %.17g\n    call       void SnoRt::push_real(float64)\n", instr->a[0].f); return; }
    if (IS_WASM){ fprintf(out, "          (call $sno_push_real (f64.const %.17g))\n", instr->a[0].f); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_push_null(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    (void)instr;
    if (IS_X86) { emit_sm_push_null_dispatch(out, 0); return; }
    if (IS_JVM) { fprintf(out, "    invokestatic rt/SnoRt/push_null()V\n"); return; }
    if (IS_JS)  { fprintf(out, "rt.push_null(); "); return; }
    if (IS_NET) { fprintf(out, "    call       void SnoRt::push_null()\n"); return; }
    if (IS_WASM){ fprintf(out, "          (call $sno_push_null)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_void_pop(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    (void)instr;
    if (IS_X86) { emit_sm_pop(out, 0); return; }
    if (IS_JVM) { fprintf(out, "    invokestatic rt/SnoRt/pop_void()V\n"); return; }
    if (IS_JS)  { fprintf(out, "rt.pop_void(); "); return; }
    if (IS_NET) { fprintf(out, "    call       void SnoRt::pop_void()\n"); return; }
    if (IS_WASM){ fprintf(out, "          (call $sno_pop_void)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_push_var(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_push_var_dispatch(out, instr, 0); return; }
    const char * s = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM) { jvm_emit_ldc_string(out, s); fprintf(out, "    invokestatic rt/SnoRt/push_var(Ljava/lang/String;)V\n"); return; }
    if (IS_JS)  { fprintf(out, "rt.push_var("); js_escape(out, s); fprintf(out, "); "); return; }
    if (IS_NET) { net_escape_ldstr(out, s); fprintf(out, "    call       void SnoRt::push_var(string)\n"); return; }
    if (IS_WASM){ int addr = wasm_intern_name(s); fprintf(out, "          (call $sno_push_var (i32.const 0x%x) (i32.const %d))\n", addr, (int)strlen(s)); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_store_var(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_store_var_dispatch(out, instr, 0); return; }
    const char * s = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM) { jvm_emit_ldc_string(out, s); fprintf(out, "    invokestatic rt/SnoRt/store_var(Ljava/lang/String;)V\n"); return; }
    if (IS_JS)  { fprintf(out, "rt.store_var("); js_escape(out, s); fprintf(out, "); "); return; }
    if (IS_NET) { net_escape_ldstr(out, s); fprintf(out, "    call       void SnoRt::store_var(string)\n"); return; }
    if (IS_WASM){ int addr = wasm_intern_name(s); fprintf(out, "          (call $sno_store_var (i32.const 0x%x) (i32.const %d))\n", addr, (int)strlen(s)); return; }
}
