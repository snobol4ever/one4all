#include "sm_template_common.h"
#include "emit_globals.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-UNI-10(b): parameterless; reads g_emit.instr / g_emit.i / g_emit.n / g_emit.in_body / g_emit.in_my_method / g_emit.out. */
int sm_jump(void) {
    const SM_t * instr = g_emit.instr;
    FILE *       out   = g_emit.out;
    if (IS_X86) return emit_sm_jump_line(out, instr, 0);
    int target = (int)instr->a[0].i;
    if (IS_JVM) {
        const char * end_lbl = g_emit.in_body ? "sm_pc_body_end" : "sm_pc_fn_end";
        if (target >= 0 && target < g_emit.n && g_emit.in_my_method && g_emit.in_my_method[target])
            fprintf(out, "    goto_w sm_pc_%d\n", target);
        else if (target >= 0 && target < g_emit.n) {
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\n");
        } else fprintf(out, "    goto_w %s\n", end_lbl);
        return 0;
    }
    if (IS_JS)   { fprintf(out, "_pc = %lld; continue; ", (long long)instr->a[0].i); return 1; }
    if (IS_NET)  { fprintf(out, "    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", (long long)instr->a[0].i); return 1; }
    if (IS_WASM) { fprintf(out, "          (i32.const %lld) (local.set $pc) (br $lp)\n", (long long)instr->a[0].i); return 1; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_jump_s(void) {
    const SM_t * instr = g_emit.instr;
    FILE *       out   = g_emit.out;
    if (IS_X86) return emit_sm_jump_s_line(out, instr, 0);
    int target = (int)instr->a[0].i; int i = g_emit.i;
    if (IS_JVM) {
        if (target >= 0 && target < g_emit.n && g_emit.in_my_method && g_emit.in_my_method[target]) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifeq sm_pc_%d_skip\n", i);
            fprintf(out, "    goto_w sm_pc_%d\n", target);
            fprintf(out, "sm_pc_%d_skip:\n", i);
        } else if (target >= 0 && target < g_emit.n) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifeq sm_pc_%d_skip\n", i);
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\nsm_pc_%d_skip:\n", i);
        }
        return 0;
    }
    if (IS_JS)   { fprintf(out, "if (rt.last_ok()) _pc = %lld; else _pc = %d; continue; ", (long long)instr->a[0].i, i + 1); return 1; }
    if (IS_NET)  { fprintf(out, "    call       bool SnoRt::last_ok()\n    brfalse    NET_L%d\n    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", i + 1, (long long)instr->a[0].i); return 1; }
    if (IS_WASM) { fprintf(out, "          (if (call $sno_last_ok)\n            (then (i32.const %lld) (local.set $pc))\n            (else (i32.const %d)   (local.set $pc)))\n          (br $lp)\n", (long long)instr->a[0].i, i + 1); return 1; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_jump_f(void) {
    const SM_t * instr = g_emit.instr;
    FILE *       out   = g_emit.out;
    if (IS_X86) return emit_sm_jump_f_line(out, instr, 0);
    int target = (int)instr->a[0].i; int i = g_emit.i;
    if (IS_JVM) {
        if (target >= 0 && target < g_emit.n && g_emit.in_my_method && g_emit.in_my_method[target]) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifne sm_pc_%d_skip\n", i);
            fprintf(out, "    goto_w sm_pc_%d\n", target);
            fprintf(out, "sm_pc_%d_skip:\n", i);
        } else if (target >= 0 && target < g_emit.n) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifne sm_pc_%d_skip\n", i);
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\nsm_pc_%d_skip:\n", i);
        }
        return 0;
    }
    if (IS_JS)   { fprintf(out, "if (!rt.last_ok()) _pc = %lld; else _pc = %d; continue; ", (long long)instr->a[0].i, i + 1); return 1; }
    if (IS_NET)  { fprintf(out, "    call       bool SnoRt::last_ok()\n    brtrue     NET_L%d\n    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", i + 1, (long long)instr->a[0].i); return 1; }
    if (IS_WASM) { fprintf(out, "          (if (i32.eqz (call $sno_last_ok))\n            (then (i32.const %lld) (local.set $pc))\n            (else (i32.const %d)   (local.set $pc)))\n          (br $lp)\n", (long long)instr->a[0].i, i + 1); return 1; }
    return 0;
}
