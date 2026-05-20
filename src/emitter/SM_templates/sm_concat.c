#include "sm_template_common.h"
#include "emit_sm.h"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_concat(const SM_Instr * instr, FILE * out) {
    (void)instr;
    if (IS_X86_TEXT) { emit_sm_concat_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    if (IS_JVM_TEXT) { fprintf(out, "    invokestatic rt/SnoRt/concat()V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.concat(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_NET_TEXT) { fprintf(out, "    call       void SnoRt::concat()\n"); return; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_concat)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
