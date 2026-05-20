#include "sm_template_common.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_halt(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    (void)instr;
    if (IS_X86_TEXT) return emit_halt_line(out, 0);
    if (IS_JVM_TEXT) {
        const char * end_lbl = ctx->in_body ? "sm_pc_body_end" : "sm_pc_fn_end";
        fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n    goto_w %s\n", end_lbl);
        return 0;
    }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT)   { fprintf(out, "break loop; "); return 1; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_NET_TEXT)  { fprintf(out, "    call       void SnoRt::halt_tos()\n    br         NET_DONE\n"); return 1; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_halt_tos)\n          (br $done)\n"); return 1; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
    return 0;
}
