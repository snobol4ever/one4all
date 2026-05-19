#include "bb_template_common.h"

void ec_bb_rem(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0; (void)sid;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_xstar — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        ec_jvm_class_hdr(out, "rem"); ec_jvm_init_ms_only(out, "rem");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/omega I\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iload_1\n    isub\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { const r = ms.sigma.slice(ms.delta, ms.omega); ms.delta = ms.omega; self.succ.alpha(); return r; },\n");
        fprintf(out, "beta() { self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        ec_net_class_hdr(out, sid, nid); ec_net_ctor_none(out, sid, nid);
        ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        ec_net_cursor_load(out); fprintf(out, "    ldarg.1\n"); ec_net_ms_length(out);
        ec_net_spec_of(out); fprintf(out, "    stloc.0\n    ldarg.1\n    ldarg.1\n"); ec_net_ms_length(out);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  }\n");
        ec_net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
    }
}
