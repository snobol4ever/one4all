#include "sm_template_common.h"
#include "emit_sm.h"

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
