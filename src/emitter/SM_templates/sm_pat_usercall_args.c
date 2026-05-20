#include "sm_template_common.h"
#include "emit_sm.h"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_usercall_args(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_usercall_args_template(out, instr); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    const char * fname = instr->a[0].s ? instr->a[0].s : ""; int nargs = (int)instr->a[1].i;
    if (IS_JVM) {
        fprintf(out, "    bipush %d\n    anewarray java/lang/Object\n", nargs);
        for (int k = nargs - 1; k >= 0; k--) { fprintf(out, "    dup\n    bipush %d\n", k); fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n    aastore\n"); }
        jvm_emit_ldc_string(out, fname);
        fprintf(out, "    swap\n    invokestatic rt/SnoPat/usercallArgs(Ljava/lang/String;[Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_usercall_args("); js_escape(out, fname); fprintf(out, ", %d); ", nargs); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_WASM_TEXT) { fprintf(out, "          ;; SM_PAT_USERCALL_ARGS not yet implemented\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
