#include "bb_template_common.h"

void bb_alt(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_flat_alt — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "alt_%d_%d", sid, nid);
        jvm_class_hdr(out, "alt");
        fprintf(out, ".field private final children [Lbb/bb_box;\n.field private final n I\n.field private current I\n.field private position I\n");
        fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;[Lbb/bb_box;)V\n    .limit stack 3\n    .limit locals 3\n");
        fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_alt/children [Lbb/bb_box;\n");
        fprintf(out, "    aload_0\n    aload_2\n    arraylength\n    putfield bb/bb_alt/n I\n    return\n.end method\n");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 3\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    aload_0\n    getfield bb/bb_alt/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_alt/position I\n");
        fprintf(out, "    aload_0\n    iconst_1\n    putfield bb/bb_alt/current I\n");
        fprintf(out, "    aload_0\n    invokevirtual bb/bb_alt/tryAlpha()Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_alt/children [Lbb/bb_box;\n    aload_0\n    getfield bb/bb_alt/current I\n    iconst_1\n    isub\n    aaload\n");
        fprintf(out, "    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_beta_omega\n", tag);
        fprintf(out, "    aload_1\n    areturn\n%s_beta_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method private tryAlpha()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 2\n%s_try_loop:\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_alt/current I\n    aload_0\n    getfield bb/bb_alt/n I\n    if_icmpgt %s_try_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_alt/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_alt/position I\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_alt/children [Lbb/bb_box;\n    aload_0\n    getfield bb/bb_alt/current I\n    iconst_1\n    isub\n    aaload\n");
        fprintf(out, "    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_try_next\n    aload_1\n    areturn\n", tag);
        fprintf(out, "%s_try_next:\n    aload_0\n    dup\n    getfield bb/bb_alt/current I\n    iconst_1\n    iadd\n    putfield bb/bb_alt/current I\n    goto %s_try_loop\n", tag, tag);
        fprintf(out, "%s_try_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { const children = self.children || []; let idx = 0; let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { idx = 0; while (idx < children.length) { const r = children[idx].alpha(); if (r !== null) { self.succ.alpha(); return r; } idx++; } self.fail.alpha(); return null; },\n");
        fprintf(out, "beta() { idx--; if (idx >= 0 && idx < children.length) { const r = children[idx].beta(); if (r !== null) { return r; } return self.beta(); } self.fail.alpha(); return null; }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    if (IS_NET) {
        net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] _children\n  .field private int32 _idx\n  .field private int32 _savedPos\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] children) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        fprintf(out, "    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
        fprintf(out, "  ALT_%d_%d_LOOP:\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ldlen\n    conv.i4\n", sid, nid);
        fprintf(out, "    bge        ALT_%d_%d_FAIL\n", sid, nid);
        fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldelem.ref\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.0\n    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid, sid, nid);
        fprintf(out, "    ldloca.s   V_r\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brtrue     ALT_%d_%d_LOOP\n    ldloc.0\n    ret\n  ALT_%d_%d_FAIL:\n", sid, nid, sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
        net_beta_hdr(out);
        fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n  ALT_%d_%d_BLOOP:\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ldlen\n    conv.i4\n", sid, nid);
        fprintf(out, "    bge        ALT_%d_%d_BFAIL\n", sid, nid);
        fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldelem.ref\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.0\n    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid, sid, nid);
        fprintf(out, "    ldloca.s   V_r\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brtrue     ALT_%d_%d_BLOOP\n    ldloc.0\n    ret\n  ALT_%d_%d_BFAIL:\n", sid, nid, sid, nid); net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox[])\n", sid, nid);
    }
}
