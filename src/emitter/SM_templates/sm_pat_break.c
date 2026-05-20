#include "sm_template_common.h"
#include "emit_sm.h"

void sm_pat_break(const SM_Instr * instr, int i, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_break_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_str_push(out, i, "brk", "brk(Ljava/lang/String;)Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_break(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_break)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
