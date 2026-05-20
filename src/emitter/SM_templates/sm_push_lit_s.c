#include "sm_template_common.h"
#include "emit_sm.h"

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
