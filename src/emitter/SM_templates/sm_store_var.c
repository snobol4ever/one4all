#include "sm_template_common.h"
#include "emit_sm.h"

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
