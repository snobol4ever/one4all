#include "sm_template_common.h"
#include "emit_sm.h"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_jump_f — SM_JUMP_F: branch to target if last_ok==false, else fall through. */
int sm_jump_f(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    if (IS_X86_TEXT) return emit_sm_jump_f_line(out, instr, 0);
    int target = (int)instr->a[0].i; int i = ctx->i;
    if (IS_JVM_TEXT) {
        if (target >= 0 && target < ctx->n && ctx->in_my_method && ctx->in_my_method[target]) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifne sm_pc_%d_skip\n", i);
            fprintf(out, "    goto_w sm_pc_%d\n", target);
            fprintf(out, "sm_pc_%d_skip:\n", i);
        } else if (target >= 0 && target < ctx->n) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifne sm_pc_%d_skip\n", i);
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\nsm_pc_%d_skip:\n", i);
        }
        return 0;
    }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT)   { fprintf(out, "if (!rt.last_ok()) _pc = %lld; else _pc = %d; continue; ", (long long)instr->a[0].i, i + 1); return 1; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_NET_TEXT)  { fprintf(out, "    call       bool SnoRt::last_ok()\n    brtrue     NET_L%d\n    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", i + 1, (long long)instr->a[0].i); return 1; }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }
    if (IS_WASM_TEXT) { fprintf(out, "          (if (i32.eqz (call $sno_last_ok))\n            (then (i32.const %lld) (local.set $pc))\n            (else (i32.const %d)   (local.set $pc)))\n          (br $lp)\n", (long long)instr->a[0].i, i + 1); return 1; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
    return 0;
}
