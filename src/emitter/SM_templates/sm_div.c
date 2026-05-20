#include "sm_template_common.h"
#include "emit_sm.h"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_div(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { edp4_sm_arith(out, instr, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { fprintf(out, "    bipush 3\n    invokestatic rt/SnoRt/arith(I)V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.arith('div'); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_NET_TEXT) { fprintf(out, "    ldc.i4.4\n    call       void SnoRt::arith(int32)\n"); return; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_arith (i32.const 3))\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
