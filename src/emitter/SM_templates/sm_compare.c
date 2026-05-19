#include "sm_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_stno — SM_STNO: set &STNO (statement number) to integer literal. */
void sm_stno(const SM_Instr * instr, FILE * out) {
    if (IS_JVM) { jvm_push_int2(out, (long)instr->a[0].i); fprintf(out, "    i2l\n    invokestatic rt/SnoRt/set_stno(J)V\n"); return; }
    if (IS_JS)  { fprintf(out, "rt.set_stno(%lld); ", (long long)instr->a[0].i); return; }
    if (IS_NET) { net_push_i4(out, (int)instr->a[0].i); fprintf(out, "    call       void SnoRt::set_stno(int32)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_acomp — SM_ACOMP: arithmetic comparison; operand is comparison-code integer. */
void sm_acomp(const SM_Instr * instr, FILE * out) {
    if (IS_JVM) { jvm_push_int2(out, (long)instr->a[0].i); fprintf(out, "    invokestatic rt/SnoRt/acomp(I)V\n"); return; }
    if (IS_JS)  { return; }
    if (IS_NET) { net_push_i4(out, (int)instr->a[0].i); fprintf(out, "    call       void SnoRt::acomp(int32)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_lcomp — SM_LCOMP: lexicographic comparison; operand is comparison-code integer. */
void sm_lcomp(const SM_Instr * instr, FILE * out) {
    if (IS_JVM) { jvm_push_int2(out, (long)instr->a[0].i); fprintf(out, "    invokestatic rt/SnoRt/lcomp(I)V\n"); return; }
    if (IS_JS)  { return; }
    if (IS_NET) { net_push_i4(out, (int)instr->a[0].i); fprintf(out, "    call       void SnoRt::lcomp(int32)\n"); return; }
}
