#include "bb_template_common.h"

void bb_fence(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_JVM) {
        jvm_class_hdr(out, "fence"); jvm_init_ms_only(out, "fence");
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
        net_class_hdr(out, sid, nid); net_ctor_none(out, sid, nid);
        net_alpha_hdr(out); fprintf(out, "    .maxstack 1\n"); net_cursor_load(out); net_spec_zw(out); fprintf(out, "    ret\n  }\n");
        net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
        return;
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
