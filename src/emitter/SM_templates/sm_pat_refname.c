#include "sm_template_common.h"
#include "emit_sm.h"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_refname(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_refname_template(out, instr); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    const char * s = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM_TEXT) { jvm_emit_ldc_string(out, s); fprintf(out, "    invokestatic rt/SnoPat/refname(Ljava/lang/String;)Lrt/SnoPat;\n    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_refname("); js_escape(out, s); fprintf(out, "); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_WASM_TEXT) { int addr = wasm_intern_name(s); fprintf(out, "          (call $sno_pat_refname (i32.const 0x%x) (i32.const %lld))\n", addr, (long long)instr->a[1].i); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
