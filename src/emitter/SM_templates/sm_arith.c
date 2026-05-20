#include "sm_template_common.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_concat(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    (void)instr;
    if (IS_X86) { emit_sm_concat_dispatch(out, 0); return; }
    if (IS_JVM) { emit_textf("    invokestatic rt/SnoRt/concat()V\n"); return; }
    if (IS_JS) { emit_textf("rt.concat(); "); return; }
    if (IS_NET) { emit_textf("    call       void SnoRt::concat()\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_concat)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_neg(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    (void)instr;
    if (IS_X86) { emit_sm_neg_dispatch(out, 0); return; }
    if (IS_JVM) { emit_textf("    invokestatic rt/SnoRt/neg()V\n"); return; }
    if (IS_JS) { emit_textf("rt.neg(); "); return; }
    if (IS_NET) { emit_textf("    call       void SnoRt::negate()\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_neg)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_coerce_num(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    (void)instr;
    if (IS_X86) { emit_sm_coerce_num_dispatch(out, 0); return; }
    if (IS_JVM) { emit_textf("    invokestatic rt/SnoRt/coerce_num()V\n"); return; }
    if (IS_JS) { emit_textf("rt.coerce_num(); "); return; }
    if (IS_NET) { emit_textf("    call       void SnoRt::coerce_num()\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_coerce_num)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_exp(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    (void)instr;
    if (IS_X86) { emit_sm_exp_dispatch(out, 0); return; }
    if (IS_JVM) { emit_textf("    invokestatic rt/SnoRt/exp_op()V\n"); return; }
    if (IS_JS) { emit_textf("rt.exp_op(); "); return; }
    if (IS_NET) { emit_textf("    call       void SnoRt::exp_op()\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_exp_op)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_add(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { edp4_sm_arith(out, instr, 0); return; }
    (void)instr;
    if (IS_JVM) { emit_textf("    bipush 0\n    invokestatic rt/SnoRt/arith(I)V\n"); return; }
    if (IS_JS) { emit_textf("rt.arith('add'); "); return; }
    if (IS_NET) { emit_textf("    ldc.i4.1\n    call       void SnoRt::arith(int32)\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_arith (i32.const 0))\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_sub(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { edp4_sm_arith(out, instr, 0); return; }
    (void)instr;
    if (IS_JVM) { emit_textf("    bipush 1\n    invokestatic rt/SnoRt/arith(I)V\n"); return; }
    if (IS_JS) { emit_textf("rt.arith('sub'); "); return; }
    if (IS_NET) { emit_textf("    ldc.i4.2\n    call       void SnoRt::arith(int32)\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_arith (i32.const 1))\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_mul(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { edp4_sm_arith(out, instr, 0); return; }
    (void)instr;
    if (IS_JVM) { emit_textf("    bipush 2\n    invokestatic rt/SnoRt/arith(I)V\n"); return; }
    if (IS_JS) { emit_textf("rt.arith('mul'); "); return; }
    if (IS_NET) { emit_textf("    ldc.i4.3\n    call       void SnoRt::arith(int32)\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_arith (i32.const 2))\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_div(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { edp4_sm_arith(out, instr, 0); return; }
    (void)instr;
    if (IS_JVM) { emit_textf("    bipush 3\n    invokestatic rt/SnoRt/arith(I)V\n"); return; }
    if (IS_JS) { emit_textf("rt.arith('div'); "); return; }
    if (IS_NET) { emit_textf("    ldc.i4.4\n    call       void SnoRt::arith(int32)\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_arith (i32.const 3))\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_mod(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { edp4_sm_arith(out, instr, 0); return; }
    (void)instr;
    if (IS_JVM) { emit_textf("    invokestatic rt/SnoRt/mod()V\n"); return; }
    if (IS_JS) { emit_textf("rt.arith('mod'); "); return; }
    if (IS_NET) { net_push_i4(out, 6); emit_textf("    call       void SnoRt::arith(int32)\n"); return; }
    if (IS_WASM) { emit_textf("          (call $sno_arith (i32.const 4))\n"); return; }
}
