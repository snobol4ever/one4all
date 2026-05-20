#include "sm_template_common.h"
#include "emit_sm.h"

void sm_pat_cat   (const SM_Instr * instr, FILE * out) { if (IS_X86_TEXT) { emit_sm_pat_cat_dispatch(out, 0); return; } (void)instr; if (IS_JVM) jvm_pat_2pat_push(out, "cat(Lrt/SnoPat;Lrt/SnoPat;)Lrt/SnoPat;"); if (IS_JS) fprintf(out, "rt.pat_cat(); "); if (IS_WASM) fprintf(out, "          (call $sno_pat_cat)\n"); }
