#include "sm_template_common.h"
#include "emit_sm.h"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_lcomp — SM_LCOMP: lexicographic comparison; operand is comparison-code integer. */
void sm_lcomp(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_lcomp_dispatch(out, instr, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    if (IS_JVM_TEXT) { jvm_push_int2(out, (long)instr->a[0].i); fprintf(out, "    invokestatic rt/SnoRt/lcomp(I)V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_NET_TEXT) { net_push_i4(out, (int)instr->a[0].i); fprintf(out, "    call       void SnoRt::lcomp(int32)\n"); return; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_lcomp (i32.const %lld))\n", (long long)instr->a[0].i); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
