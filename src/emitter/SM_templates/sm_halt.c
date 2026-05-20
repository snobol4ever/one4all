#include "sm_template_common.h"
#include "emit_globals.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EC-UNI-10(b): parameterless; reads g_emit.in_body / g_emit.out. */
int sm_halt(void) {
    FILE * out = g_emit.out;
    if (IS_X86) return emit_halt_line(out, 0);
    if (IS_JVM) {
        const char * end_lbl = g_emit.in_body ? "sm_pc_body_end" : "sm_pc_fn_end";
        emit_textf("    invokestatic rt/SnoRt/halt_tos()V\n    goto_w %s\n", end_lbl);
        return 0;
    }
    if (IS_JS)   { emit_textf("break loop; "); return 1; }
    if (IS_NET)  { emit_textf("    call       void SnoRt::halt_tos()\n    br         NET_DONE\n"); return 1; }
    if (IS_WASM) { emit_textf("          (call $sno_halt_tos)\n          (br $done)\n"); return 1; }
    return 0;
}
