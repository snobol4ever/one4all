#include "bb_template_common.h"

void ec_bb_arb(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_xfarb — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "arb_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "arb");
        fprintf(out, ".field private arb_count I\n.field private arb_start I\n");
        ec_jvm_init_ms_only(out, "arb");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_arb/arb_count I\n");
        fprintf(out, "    aload_0\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_arb/arb_start I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_arb/arb_count I\n    iconst_1\n    iadd\n    putfield bb/bb_arb/arb_count I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arb/arb_start I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    iadd\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpgt %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_arb/arb_start I\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    areturn\n%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { let delta = 0; let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { delta = ms.omega - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
        fprintf(out, "beta() { if (delta <= 0) { self.fail.alpha(); return; } delta--; ms.delta--; const r = ms.sigma.slice(ms.delta, ms.delta + delta + 1); return r; }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    if (IS_NET) {
        ec_net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private int32 _count\n  .field private int32 _start\n");
        ec_net_ctor_none(out, sid, nid);
        ec_net_alpha_hdr(out);
        fprintf(out, "    .maxstack 2\n");
        fprintf(out, "    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_count\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stfld      int32 pat_%d_%d::_start\n", sid, nid);
        ec_net_cursor_load(out); ec_net_spec_zw(out); fprintf(out, "    ret\n  }\n");
        ec_net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n");
        fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_count\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
        fprintf(out, "    ldarg.1\n"); ec_net_ms_length(out);
        fprintf(out, "    bgt        ARB_%d_%d_FAIL\n", sid, nid);
        fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n", sid, nid);
        ec_net_spec_of(out); fprintf(out, "    ret\n");
        fprintf(out, "  ARB_%d_%d_FAIL:\n", sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
    }
}
