#include "bb_template_common.h"

void bb_cat(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM_TEXT) {
        char tag[32]; snprintf(tag, sizeof tag, "seq_%d_%d", sid, nid);
        jvm_class_hdr(out, "seq");
        fprintf(out, ".field private final left Lbb/bb_box;\n.field private final right Lbb/bb_box;\n");
        fprintf(out, ".field private matched_start I\n.field private matched_len I\n");
        fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;Lbb/bb_box;)V\n    .limit stack 3\n    .limit locals 4\n");
        fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_seq/left Lbb/bb_box;\n");
        fprintf(out, "    aload_0\n    aload_3\n    putfield bb/bb_seq/right Lbb/bb_box;\n    return\n.end method\n");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    aload_0\n    getfield bb/bb_seq/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_seq/matched_start I\n");
        fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_seq/matched_len I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_seq/left Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_omega\n", tag);
        fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    putfield bb/bb_seq/matched_len I\n");
        fprintf(out, "    aload_0\n    invokevirtual bb/bb_seq/rightAlpha()Lbb/bb_box$Spec;\n    areturn\n");
        fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_seq/right Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_beta_right_omega\n", tag);
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_seq/matched_start I\n    aload_0\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_beta_right_omega:\n    aload_0\n    invokevirtual bb/bb_seq/leftBeta()Lbb/bb_box$Spec;\n    areturn\n.end method\n", tag);
        fprintf(out, ".method private rightAlpha()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_seq/right Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_rA_omega\n", tag);
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_seq/matched_start I\n    aload_0\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_rA_omega:\n    aload_0\n    invokevirtual bb/bb_seq/leftBeta()Lbb/bb_box$Spec;\n    areturn\n.end method\n", tag);
        fprintf(out, ".method private leftBeta()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_seq/left Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_lB_omega\n", tag);
        fprintf(out, "    aload_0\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    putfield bb/bb_seq/matched_len I\n");
        fprintf(out, "    aload_0\n    invokevirtual bb/bb_seq/rightAlpha()Lbb/bb_box$Spec;\n    areturn\n");
        fprintf(out, "%s_lB_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        return;
    }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) {
        fprintf(out, "function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { const lr = self.left.alpha(); if (lr === null) { self.fail.alpha(); return; }\n");
        fprintf(out, "let rr = self.right.alpha(); while (rr === null) { const lr2 = self.left.beta(); if (lr2 === null) { self.fail.alpha(); return; } rr = self.right.alpha(); }\n");
        fprintf(out, "self.succ.alpha(); return rr; },\n");
        fprintf(out, "beta() { let rr = self.right.beta(); while (rr === null) { const lr = self.left.beta(); if (lr === null) { self.fail.alpha(); return; } rr = self.right.alpha(); }\n");
        fprintf(out, "return rr; }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_NET_TEXT) {
        net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _left\n");
        fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _right\n");
        fprintf(out, "  .field private int32 _mStart\n  .field private int32 _mLen\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox left, class [boxes]Snobol4.Runtime.Boxes.IByrdBox right) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.2\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_lr, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_rr)\n");
        fprintf(out, "    ldarg.0\n"); net_cursor_load(out); fprintf(out, "    stfld      int32 pat_%d_%d::_mStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.0\n    ldloca.s   V_lr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brtrue     CAT_%d_%d_FAIL\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldloca.s   V_lr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    stfld      int32 pat_%d_%d::_mLen\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.1\n    ldloca.s   V_rr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brtrue     CAT_%d_%d_FAIL\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mLen\n", sid, nid);
        fprintf(out, "    ldloca.s   V_rr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n");
        net_spec_of(out); fprintf(out, "    ret\n  CAT_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
        net_beta_hdr(out);
        fprintf(out, "    .maxstack 2\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_rr)\n");
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.0\n    ldloca.s   V_rr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brfalse    CAT_%d_%d_BNOK\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    ret\n  CAT_%d_%d_BNOK:\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_mLen\n", sid, nid);
        fprintf(out, "    ldloca.s   V_rr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n");
        net_spec_of(out); fprintf(out, "    ret\n  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox, class [boxes]Snobol4.Runtime.Boxes.IByrdBox)\n", sid, nid);
    }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }
    /* IS_WASM_TEXT: n/a — BB WASM never landed in original code */
    /* IS_WASM_BIN: n/a — BB WASM never landed in original code */
}
