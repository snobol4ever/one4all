/* bb_abort.c — BB template for ABORT pattern-level halt.
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Function body is byte-identical to the consolidated bb_pat.c original
   that this file restores; separators and includes match the original per-file
   shape pre-EC-UNI-13(a). */
#include "bb_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_abort(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_JVM) {
        jvm_class_hdr(out, "abort");
        emit_textf(".inner class public static final abort_exception inner bb/bb_abort$AbortException outer bb/bb_abort\n");
        jvm_init_ms_only(out, "abort");
        jvm_alpha_method_hdr(out, 2, 1);
        emit_textf("    new bb/bb_abort$AbortException\n    dup\n    invokespecial bb/bb_abort$AbortException/<init>()V\n    athrow\n.end method\n");
        jvm_beta_method_hdr(out, 2, 1);
        emit_textf("    new bb/bb_abort$AbortException\n    dup\n    invokespecial bb/bb_abort$AbortException/<init>()V\n    athrow\n.end method\n");
        (void)sid; (void)nid; return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        emit_textf("alpha() { self.fail.alpha(); return null; },\nbeta() { self.fail.alpha(); return null; }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        net_class_hdr(out, sid, nid); net_ctor_none(out, sid, nid);
        net_alpha_hdr(out); emit_textf("    .maxstack 1\n"); net_fail_ret(out); emit_textf("  }\n");
        net_beta_hdr(out); emit_textf("    .maxstack 1\n"); net_fail_ret(out); emit_textf("  }\n}\n");
        emit_textf("    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
        return;
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
