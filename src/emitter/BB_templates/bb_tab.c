#include "bb_template_common.h"

void bb_tab(BB_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0; int rtab = (nd->ival2 != 0);
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        const char * name = rtab ? "rtab" : "tab"; char tag[32]; snprintf(tag, sizeof tag, "%s_%d_%d", name, sid, nid);
        jvm_class_hdr(out, name);
        fprintf(out, ".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n.field private advance I\n");
        jvm_init_ms_int(out, name, "n"); jvm_val_helper(out, name);
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals %d\n", rtab ? 4 : 3);
        if (rtab) {
            fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    aload_0\n    invokevirtual bb/bb_rtab/val()I\n    isub\n    istore_1\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iload_1\n    if_icmpgt %s_omega\n", tag);
            fprintf(out, "    iload_1\n    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    isub\n    istore_2\n");
            fprintf(out, "    aload_0\n    iload_2\n    putfield bb/bb_rtab/advance I\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_3\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    iload_1\n    putfield bb/bb_box$MatchState/delta I\n");
            fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_3\n    iload_2\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
            fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
            fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_rtab/advance I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
            fprintf(out, "    aconst_null\n    areturn\n.end method\n");
        } else {
            fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_tab/val()I\n    if_icmpgt %s_omega\n", tag);
            fprintf(out, "    aload_0\n    invokevirtual bb/bb_tab/val()I\n    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    isub\n    istore_1\n");
            fprintf(out, "    aload_0\n    iload_1\n    putfield bb/bb_tab/advance I\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_2\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    aload_0\n    invokevirtual bb/bb_tab/val()I\n    putfield bb/bb_box$MatchState/delta I\n");
            fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_2\n    iload_1\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
            fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
            fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
            fprintf(out, "    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_tab/advance I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
            fprintf(out, "    aconst_null\n    areturn\n.end method\n");
        }
        return;
    }
    if (IS_JS) {
        int64_t n = nd->ival;
        fprintf(out, "function make_pat_%d_%d(ms) { const n = %ld; let delta = 0; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        if (rtab)
            fprintf(out, "alpha() { const tgt = ms.omega - n; if (ms.delta > tgt) { self.fail.alpha(); return; } delta = tgt - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
        else
            fprintf(out, "alpha() { if (ms.delta > n || ms.delta > ms.omega) { self.fail.alpha(); return; } delta = n - ms.delta; if (ms.delta + delta > ms.omega) delta = ms.omega - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
        fprintf(out, "beta() { ms.delta -= delta; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        int n = (int)nd->ival; const char * lbl = rtab ? "RTAB" : "TAB";
        net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private int32 _n\n  .field private int32 _advance\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        if (rtab) {
            fprintf(out, "    .maxstack 4\n    .locals init (int32 V_target, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
            fprintf(out, "    ldarg.1\n"); net_ms_length(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n    stloc.0\n", sid, nid);
            net_cursor_load(out); fprintf(out, "    ldloc.0\n    bgt        %s_%d_%d_FAIL\n", lbl, sid, nid);
            fprintf(out, "    ldarg.0\n    ldloc.0\n"); net_cursor_load(out); fprintf(out, "    sub\n    stfld      int32 pat_%d_%d::_advance\n", sid, nid);
            net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n", sid, nid);
            net_spec_of(out); fprintf(out, "    stloc.1\n    ldarg.1\n    ldloc.0\n");
            fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.1\n    ret\n");
        } else {
            fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
            net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    bgt        %s_%d_%d_FAIL\n", sid, nid, lbl, sid, nid);
            fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid); net_cursor_load(out);
            fprintf(out, "    sub\n    stfld      int32 pat_%d_%d::_advance\n", sid, nid);
            net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n", sid, nid);
            net_spec_of(out); fprintf(out, "    stloc.0\n    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid);
            fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n");
        }
        fprintf(out, "  %s_%d_%d_FAIL:\n", lbl, sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
        net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n    sub\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n"); net_fail_ret(out); fprintf(out, "  }\n}\n");
        net_push_i4(out, n); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
