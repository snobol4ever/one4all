#include "bb_template_common.h"

void ec_bb_pos(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0; int rpos = (nd->ival2 != 0);
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_xposi/xrpsi — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        const char * name = rpos ? "rpos" : "pos"; char tag[32]; snprintf(tag, sizeof tag, "%s_%d_%d", name, sid, nid);
        ec_jvm_class_hdr(out, name);
        fprintf(out, ".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n");
        ec_jvm_init_ms_int(out, name, "n"); ec_jvm_val_helper(out, name);
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        if (rpos) {
            fprintf(out, "    aload_0\n    getfield bb/bb_rpos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_rpos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    aload_0\n    invokevirtual bb/bb_rpos/val()I\n    isub\n");
            fprintf(out, "    if_icmpne %s_omega\n", tag);
        } else {
            fprintf(out, "    aload_0\n    getfield bb/bb_pos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_pos/val()I\n    if_icmpne %s_omega\n", tag);
        }
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_%s/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n", name);
        fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        int64_t n = nd->ival;
        fprintf(out, "function make_pat_%d_%d(ms) { const n = %ld; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        if (rpos)
            fprintf(out, "alpha() { if (ms.delta !== ms.omega - n) { self.fail.alpha(); return; } self.succ.alpha(); return ''; },\n");
        else
            fprintf(out, "alpha() { if (ms.delta !== n) { self.fail.alpha(); return; } self.succ.alpha(); return ''; },\n");
        fprintf(out, "beta() { self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        int n = (int)nd->ival; const char * lbl = rpos ? "RPOS" : "POS";
        ec_net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private int32 _n\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
        ec_net_alpha_hdr(out); fprintf(out, "    .maxstack 3\n"); ec_net_cursor_load(out);
        if (rpos) { fprintf(out, "    ldarg.1\n"); ec_net_ms_length(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n", sid, nid); }
        else { fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid); }
        fprintf(out, "    bne.un     %s_%d_%d_FAIL\n", lbl, sid, nid);
        ec_net_cursor_load(out); ec_net_spec_zw(out); fprintf(out, "    ret\n");
        fprintf(out, "  %s_%d_%d_FAIL:\n", lbl, sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n");
        ec_net_beta_hdr(out); fprintf(out, "    .maxstack 1\n"); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        ec_net_push_i4(out, n); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
    }
}
