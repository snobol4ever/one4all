#include "sm_template_common.h"
#include "emit_sm.h"

void sm_pat_fence1(const SM_Instr * instr, FILE * out) { if (IS_X86_TEXT) { emit_sm_pat_fence1_dispatch(out, 0); return; } (void)instr; if (IS_JVM) jvm_pat_pat_push(out, "fence1(Lrt/SnoPat;)Lrt/SnoPat;");  if (IS_WASM) fprintf(out, "          (call $sno_pat_fence)\n"); }
