#include "bb_template_common.h"

void ec_bb_abort(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_JVM) {
        ec_jvm_class_hdr(out, "abort");
        fprintf(out, ".inner class public static final abort_exception inner bb/bb_abort$AbortException outer bb/bb_abort\n");
        ec_jvm_init_ms_only(out, "abort");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 2\n    .limit locals 1\n    new bb/bb_abort$AbortException\n    dup\n    invokespecial bb/bb_abort$AbortException/<init>()V\n    athrow\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 2\n    .limit locals 1\n    new bb/bb_abort$AbortException\n    dup\n    invokespecial bb/bb_abort$AbortException/<init>()V\n    athrow\n.end method\n");
        (void)sid; (void)nid; return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { self.fail.alpha(); return null; },\nbeta() { self.fail.alpha(); return null; }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        ec_net_class_hdr(out, sid, nid); ec_net_ctor_none(out, sid, nid);
        ec_net_alpha_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_fail_ret(out); fprintf(out, "  }\n");
        ec_net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
        return;
    }
    /* IS_TEXT / IS_BIN: x86 path — not wired here yet (EC-3+). */
}
