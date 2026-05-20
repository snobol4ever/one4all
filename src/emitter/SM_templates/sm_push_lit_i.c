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
