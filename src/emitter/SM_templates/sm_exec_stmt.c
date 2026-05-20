#include "sm_template_common.h"
#include "emit_sm.h"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_exec_stmt(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_exec_stmt_template(out, instr); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.exec_stmt("); js_escape(out, instr->a[0].s ? instr->a[0].s : ""); fprintf(out, ", %lld); ", instr->a[1].i); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_exec_stmt (i32.const 0) (i32.const 0) (i32.const 0))\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
