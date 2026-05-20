#include "bb_template_common.h"

void bb_pos(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0; int rpos = (nd->ival2 != 0);
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        const char * name = rpos ? "rpos" : "pos"; char tag[32]; snprintf(tag, sizeof tag, "%s_%d_%d", name, sid, nid);
        jvm_class_hdr(out, name);
        emit_textf(".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n");
        jvm_init_ms_int(out, name, "n"); jvm_val_helper(out, name);
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        if (rpos) {
            emit_textf("    aload_0\n    getfield bb/bb_rpos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
            emit_textf("    aload_0\n    getfield bb/bb_rpos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    aload_0\n    invokevirtual bb/bb_rpos/val()I\n    isub\n");
            emit_textf("    if_icmpne %s_omega\n", tag);
        } else {
            emit_textf("    aload_0\n    getfield bb/bb_pos/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_pos/val()I\n    if_icmpne %s_omega\n", tag);
        }
        emit_textf("    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_%s/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n", name);
        emit_textf("%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        int64_t n = nd->ival;
        emit_textf("function make_pat_%d_%d(ms) { const n = %ld; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        if (rpos)
            emit_textf("alpha() { if (ms.delta !== ms.omega - n) { self.fail.alpha(); return; } self.succ.alpha(); return ''; },\n");
        else
            emit_textf("alpha() { if (ms.delta !== n) { self.fail.alpha(); return; } self.succ.alpha(); return ''; },\n");
        emit_textf("beta() { self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        int n = (int)nd->ival; const char * lbl = rpos ? "RPOS" : "POS";
        net_class_hdr(out, sid, nid);
        emit_textf("  .field private int32 _n\n");
        emit_textf("  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
        emit_textf("    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        emit_textf("    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out); emit_textf("    .maxstack 3\n"); net_cursor_load(out);
        if (rpos) { emit_textf("    ldarg.1\n"); net_ms_length(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n", sid, nid); }
        else { emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid); }
        emit_textf("    bne.un     %s_%d_%d_FAIL\n", lbl, sid, nid);
        net_cursor_load(out); net_spec_zw(out); emit_textf("    ret\n");
        emit_textf("  %s_%d_%d_FAIL:\n", lbl, sid, nid); net_fail_ret(out); emit_textf("  }\n");
        net_beta_hdr(out); emit_textf("    .maxstack 1\n"); net_fail_ret(out); emit_textf("  }\n}\n");
        net_push_i4(out, n); emit_textf("    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
