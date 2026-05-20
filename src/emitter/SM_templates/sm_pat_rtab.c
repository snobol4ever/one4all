#include "sm_template_common.h"
#include "emit_sm.h"

void sm_pat_rtab (const SM_Instr * instr, FILE * out) { if (IS_X86_TEXT) { emit_sm_pat_rtab_dispatch(out, 0); return; } (void)instr; if (IS_JVM) jvm_pat_long_push(out, "rtab(J)Lrt/SnoPat;");  if (IS_JS) fprintf(out, "rt.pat_rtab(); ");  if (IS_WASM) fprintf(out, "          (call $sno_pat_rtab)\n"); }
