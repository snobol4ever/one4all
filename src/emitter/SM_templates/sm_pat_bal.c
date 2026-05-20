#include "sm_template_common.h"
#include "emit_sm.h"

void sm_pat_bal  (const SM_Instr * instr, FILE * out) { if (IS_X86_TEXT) { emit_sm_pat_bal_dispatch(out, 0); return; } (void)instr; if (IS_JVM) jvm_pat_noarg_push(out, "bal()Lrt/SnoPat;");    if (IS_JS) fprintf(out, "rt.pat_bal(); ");   if (IS_WASM) fprintf(out, "          (call $sno_pat_bal)\n"); }
