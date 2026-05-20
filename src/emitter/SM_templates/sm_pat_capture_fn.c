#include "sm_template_common.h"
#include "emit_sm.h"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_capture_fn(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_capture_fn_template(out, instr); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    const char * fname = instr->a[0].s ? instr->a[0].s : ""; const char * namelist = instr->a[2].s ? instr->a[2].s : "";
    if (IS_JVM) {
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        jvm_emit_ldc_string(out, fname); jvm_emit_ldc_string(out, namelist);
        fprintf(out, "    invokestatic rt/SnoPat/captureFn(Lrt/SnoPat;Ljava/lang/String;Ljava/lang/String;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_capture_fn("); js_escape(out, fname); fprintf(out, ", %lld, ", instr->a[1].i); js_escape(out, namelist); fprintf(out, "); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_WASM_TEXT) { fprintf(out, "          ;; SM_PAT_CAPTURE_FN not yet implemented\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
