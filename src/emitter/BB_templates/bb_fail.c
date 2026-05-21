/* bb_fail.c — BB template for BB_FAIL.
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Honest no-op stub across all five backends.  This kind is handled today by the
   AST/runtime path in src/lower/ir_exec.c and src/lower/lower_*.c; this template
   slot exists so the BB layer is total over BB_op_t and future native-codegen
   work has a place to land.  Phase B fills the arms when a frontend lowers
   directly to native code for BB_FAIL. */
#include "bb_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_fail(void)    {
    if (IS_X86) {
        /* Lifted from emit_bb.c::emit_bb_xfail (which called the static helper
           emit_bb_jmp_pair("FAIL", s, f, b, 0)).  Snocone discipline: read values
           from g_emit, write strings via name-taking primitives. */
        const char * lbl_fail = g_emit.lbl_fail;
        const char * lbl_back = g_emit.lbl_back;
        emit_bb_box_banner("FAIL", "");
        emit_text_jmp(lbl_fail, JMP_JMP);
        emit_text_label(lbl_back);
        emit_text_jmp(lbl_fail, JMP_JMP);
        return;
    }
    if (IS_JVM) return; if (IS_JS) return; if (IS_NET) return; if (IS_WASM) return;
}
