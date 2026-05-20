#include "sm_template_common.h"
#include "emit_sm.h"
/* EC-UNI-8.3-fixup: every fn carries the full backend × mode matrix; NET PAT
 * is a known stub so IS_NET_* cells are n/a sentinels. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_fence0(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_fence0_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_noarg_push(out, "fence0()Lrt/SnoPat;"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_fence(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_fence)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_fence1(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_fence1_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_pat_push(out, "fence1(Lrt/SnoPat;)Lrt/SnoPat;"); return; }
    /* IS_JS: n/a — fence1 has no JS arm in original */
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_fence)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_abort(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_abort_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_noarg_push(out, "abort_()Lrt/SnoPat;"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_abort(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_abort)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_fail(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_fail_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_noarg_push(out, "fail_()Lrt/SnoPat;"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_fail(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_fail)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_succeed(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_succeed_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_noarg_push(out, "succeed_()Lrt/SnoPat;"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_succeed(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_succeed)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_arbno(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_arbno_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_pat_push(out, "arbno(Lrt/SnoPat;)Lrt/SnoPat;"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_arbno(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_arbno)\n"); return; }
}
