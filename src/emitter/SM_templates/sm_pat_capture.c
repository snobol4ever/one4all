#include "sm_template_common.h"
#include "emit_sm.h"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_capture(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_capture_template(out, instr); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    const char * s = instr->a[0].s ? instr->a[0].s : ""; int kind = (int)instr->a[1].i;
    if (IS_JVM) {
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        jvm_emit_ldc_string(out, s); jvm_push_int2(out, kind);
        fprintf(out, "    invokestatic rt/SnoPat/capture(Lrt/SnoPat;Ljava/lang/String;I)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_capture("); js_escape(out, s); fprintf(out, ", %d); ", kind); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_WASM_TEXT) { int addr = wasm_intern_name(s); fprintf(out, "          (call $sno_pat_capture (i32.const 0x%x) (i32.const %lld) (i32.const %lld))\n", addr, (long long)instr->a[1].i, (long long)instr->a[2].i); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
