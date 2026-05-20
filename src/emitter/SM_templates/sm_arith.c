#include "sm_template_common.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_concat(const SM_Instr * instr, FILE * out) {
    (void)instr;
    if (IS_X86) { emit_sm_concat_dispatch(out, 0); return; }
    if (IS_JVM) { fprintf(out, "    invokestatic rt/SnoRt/concat()V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.concat(); "); return; }
    if (IS_NET) { fprintf(out, "    call       void SnoRt::concat()\n"); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_concat)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_neg(const SM_Instr * instr, FILE * out) {
    (void)instr;
    if (IS_X86) { emit_sm_neg_dispatch(out, 0); return; }
    if (IS_JVM) { fprintf(out, "    invokestatic rt/SnoRt/neg()V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.neg(); "); return; }
    if (IS_NET) { fprintf(out, "    call       void SnoRt::negate()\n"); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_neg)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_coerce_num(const SM_Instr * instr, FILE * out) {
    (void)instr;
    if (IS_X86) { emit_sm_coerce_num_dispatch(out, 0); return; }
    if (IS_JVM) { fprintf(out, "    invokestatic rt/SnoRt/coerce_num()V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.coerce_num(); "); return; }
    if (IS_NET) { fprintf(out, "    call       void SnoRt::coerce_num()\n"); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_coerce_num)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_exp(const SM_Instr * instr, FILE * out) {
    (void)instr;
    if (IS_X86) { emit_sm_exp_dispatch(out, 0); return; }
    if (IS_JVM) { fprintf(out, "    invokestatic rt/SnoRt/exp_op()V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.exp_op(); "); return; }
    if (IS_NET) { fprintf(out, "    call       void SnoRt::exp_op()\n"); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_exp_op)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_add(const SM_Instr * instr, FILE * out) {
    if (IS_X86) { edp4_sm_arith(out, instr, 0); return; }
    (void)instr;
    if (IS_JVM) { fprintf(out, "    bipush 0\n    invokestatic rt/SnoRt/arith(I)V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.arith('add'); "); return; }
    if (IS_NET) { fprintf(out, "    ldc.i4.1\n    call       void SnoRt::arith(int32)\n"); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_arith (i32.const 0))\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_sub(const SM_Instr * instr, FILE * out) {
    if (IS_X86) { edp4_sm_arith(out, instr, 0); return; }
    (void)instr;
    if (IS_JVM) { fprintf(out, "    bipush 1\n    invokestatic rt/SnoRt/arith(I)V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.arith('sub'); "); return; }
    if (IS_NET) { fprintf(out, "    ldc.i4.2\n    call       void SnoRt::arith(int32)\n"); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_arith (i32.const 1))\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_mul(const SM_Instr * instr, FILE * out) {
    if (IS_X86) { edp4_sm_arith(out, instr, 0); return; }
    (void)instr;
    if (IS_JVM) { fprintf(out, "    bipush 2\n    invokestatic rt/SnoRt/arith(I)V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.arith('mul'); "); return; }
    if (IS_NET) { fprintf(out, "    ldc.i4.3\n    call       void SnoRt::arith(int32)\n"); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_arith (i32.const 2))\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_div(const SM_Instr * instr, FILE * out) {
    if (IS_X86) { edp4_sm_arith(out, instr, 0); return; }
    (void)instr;
    if (IS_JVM) { fprintf(out, "    bipush 3\n    invokestatic rt/SnoRt/arith(I)V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.arith('div'); "); return; }
    if (IS_NET) { fprintf(out, "    ldc.i4.4\n    call       void SnoRt::arith(int32)\n"); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_arith (i32.const 3))\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_mod(const SM_Instr * instr, FILE * out) {
    if (IS_X86) { edp4_sm_arith(out, instr, 0); return; }
    (void)instr;
    if (IS_JVM) { fprintf(out, "    invokestatic rt/SnoRt/mod()V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.arith('mod'); "); return; }
    if (IS_NET) { net_push_i4(out, 6); fprintf(out, "    call       void SnoRt::arith(int32)\n"); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_arith (i32.const 4))\n"); return; }
}
