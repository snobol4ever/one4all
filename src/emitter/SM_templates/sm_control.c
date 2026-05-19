#include "sm_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_jump — SM_JUMP: unconditional branch to target PC.
   Returns 1 (has_continue) for JS and NET; JVM emits goto directly (no dispatch loop). */
int sm_jump(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    int target = (int)instr->a[0].i;
    if (IS_JVM) {
        const char * end_lbl = ctx->in_body ? "sm_pc_body_end" : "sm_pc_fn_end";
        if (target >= 0 && target < ctx->n && ctx->in_my_method && ctx->in_my_method[target])
            fprintf(out, "    goto_w sm_pc_%d\n", target);
        else if (target >= 0 && target < ctx->n) {
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\n");
        } else fprintf(out, "    goto_w %s\n", end_lbl);
        return 0;
    }
    if (IS_JS)  { fprintf(out, "_pc = %lld; continue; ", (long long)instr->a[0].i); return 1; }
    if (IS_NET) { fprintf(out, "    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", (long long)instr->a[0].i); return 1; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_jump_s — SM_JUMP_S: branch to target if last_ok==true, else fall through. */
int sm_jump_s(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    int target = (int)instr->a[0].i;
    int i = ctx->i;
    if (IS_JVM) {
        if (target >= 0 && target < ctx->n && ctx->in_my_method && ctx->in_my_method[target]) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifeq sm_pc_%d_skip\n", i);
            fprintf(out, "    goto_w sm_pc_%d\n", target);
            fprintf(out, "sm_pc_%d_skip:\n", i);
        } else if (target >= 0 && target < ctx->n) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifeq sm_pc_%d_skip\n", i);
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\nsm_pc_%d_skip:\n", i);
        }
        return 0;
    }
    if (IS_JS)  { fprintf(out, "if (rt.last_ok()) _pc = %lld; else _pc = %d; continue; ", (long long)instr->a[0].i, i + 1); return 1; }
    if (IS_NET) {
        fprintf(out, "    call       bool SnoRt::last_ok()\n");
        fprintf(out, "    brfalse    NET_L%d\n", i + 1);
        fprintf(out, "    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", (long long)instr->a[0].i);
        return 1;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_jump_f — SM_JUMP_F: branch to target if last_ok==false, else fall through. */
int sm_jump_f(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    int target = (int)instr->a[0].i;
    int i = ctx->i;
    if (IS_JVM) {
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
    if (IS_JS)  { fprintf(out, "if (!rt.last_ok()) _pc = %lld; else _pc = %d; continue; ", (long long)instr->a[0].i, i + 1); return 1; }
    if (IS_NET) {
        fprintf(out, "    call       bool SnoRt::last_ok()\n");
        fprintf(out, "    brtrue     NET_L%d\n", i + 1);
        fprintf(out, "    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", (long long)instr->a[0].i);
        return 1;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_halt — SM_HALT: end execution, pushing TOS as result; branch to exit label. */
int sm_halt(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    (void)instr;
    if (IS_JVM) {
        const char * end_lbl = ctx->in_body ? "sm_pc_body_end" : "sm_pc_fn_end";
        fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
        fprintf(out, "    goto_w %s\n", end_lbl);
        return 0;
    }
    if (IS_JS)  { fprintf(out, "break loop; "); return 1; }
    if (IS_NET) { fprintf(out, "    call       void SnoRt::halt_tos()\n    br         NET_DONE\n"); return 1; }
    return 0;
}
