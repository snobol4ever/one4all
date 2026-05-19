#include "bb_template_common.h"

void ec_bb_fence(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_JVM) {
        ec_jvm_class_hdr(out, "fence"); ec_jvm_init_ms_only(out, "fence");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_fence/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
        (void)sid; (void)nid; return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { self.succ.alpha(); return ''; },\nbeta() { self.fail.alpha(); return null; }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        ec_net_class_hdr(out, sid, nid); ec_net_ctor_none(out, sid, nid);
        ec_net_alpha_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_cursor_load(out); ec_net_spec_zw(out); fprintf(out, "    ret\n  }\n");
        ec_net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
        return;
    }
    /* IS_TEXT / IS_BIN: x86 path via emit_flat_node → emit_bb_xfnce — not wired here yet (EC-3+). */
}
