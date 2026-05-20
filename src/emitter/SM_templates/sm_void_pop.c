#include "sm_template_common.h"
#include "emit_sm.h"

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
