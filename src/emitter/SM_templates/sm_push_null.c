#include "sm_template_common.h"
#include "emit_sm.h"

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
