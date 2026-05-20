#include "sm_template_common.h"
#include "emit_sm.h"

void sm_pat_deref(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_deref_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n    invokestatic rt/SnoPat/deref(Ljava/lang/Object;)Lrt/SnoPat;\n    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_deref(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_deref)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
