#include "sm_template_common.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_stno(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_stno_template(out, instr); return; }
    if (IS_JVM) { jvm_push_int2(out, (long)instr->a[0].i); emit_textf("    i2l\n    invokestatic rt/SnoRt/set_stno(J)V\n"); return; }
    if (IS_JS) { emit_textf("rt.set_stno(%lld); ", (long long)instr->a[0].i); return; }
    if (IS_NET) { net_push_i4(out, (int)instr->a[0].i); emit_textf("    call       void SnoRt::set_stno(int32)\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_set_stno (i32.const %lld))\n", (long long)instr->a[0].i); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_acomp(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_acomp_dispatch(out, instr, 0); return; }
    if (IS_JVM) { jvm_push_int2(out, (long)instr->a[0].i); emit_textf("    invokestatic rt/SnoRt/acomp(I)V\n"); return; }
    if (IS_JS) { return; }
    if (IS_NET) { net_push_i4(out, (int)instr->a[0].i); emit_textf("    call       void SnoRt::acomp(int32)\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_acomp (i32.const %lld))\n", (long long)instr->a[0].i); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_lcomp(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_lcomp_dispatch(out, instr, 0); return; }
    if (IS_JVM) { jvm_push_int2(out, (long)instr->a[0].i); emit_textf("    invokestatic rt/SnoRt/lcomp(I)V\n"); return; }
    if (IS_JS) { return; }
    if (IS_NET) { net_push_i4(out, (int)instr->a[0].i); emit_textf("    call       void SnoRt::lcomp(int32)\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_lcomp (i32.const %lld))\n", (long long)instr->a[0].i); return; }
}
