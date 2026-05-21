/* bb_capture.c — BB template for capture (pat . var).
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Function body is byte-identical to the consolidated bb_pat.c original
   that this file restores; separators and includes match the original per-file
   shape pre-EC-UNI-13(a). */
#include "bb_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_capture(int imm) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        (void)imm;
        jvm_class_hdr(out, "capture");
        emit_textf(".inner interface public static abstract var_setter inner bb/bb_capture$VarSetter outer bb/bb_capture\n");
        emit_textf(".field private final child Lbb/bb_box;\n.field private final varname Ljava/lang/String;\n");
        emit_textf(".field private final immediate Z\n.field private final setter Lbb/bb_capture$VarSetter;\n");
        emit_textf(".field private pending_start I\n.field private pending_len I\n.field private has_pending Z\n");
        emit_textf(".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;Ljava/lang/String;ZLbb/bb_capture$VarSetter;)V\n    .limit stack 3\n    .limit locals 6\n");
        emit_textf("    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        emit_textf("    aload_0\n    aload_2\n    putfield bb/bb_capture/child Lbb/bb_box;\n");
        emit_textf("    aload_0\n    aload_3\n    putfield bb/bb_capture/varname Ljava/lang/String;\n");
        emit_textf("    aload_0\n    iload 4\n    putfield bb/bb_capture/immediate Z\n");
        emit_textf("    aload_0\n    aload 5\n    putfield bb/bb_capture/setter Lbb/bb_capture$VarSetter;\n    return\n.end method\n");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 3\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_capture/child Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n");
        emit_textf("    aload_0\n    aload_1\n    invokevirtual bb/bb_capture/runChild(Lbb/bb_box$Spec;)Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 3\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_capture/child Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n");
        emit_textf("    aload_0\n    aload_1\n    invokevirtual bb/bb_capture/runChild(Lbb/bb_box$Spec;)Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        (void)sid; (void)nid; return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { const varname = ", nd->ival, nid);
        js_escape(out, nd->sval);
        emit_textf("; let self = { succ: null, fail: null,\n");
        emit_textf("alpha() { const cr = self.child.alpha(); if (cr === null) { self.fail.alpha(); return; } ms._do_capture(cr, varname, %d); self.succ.alpha(); return cr; },\n", imm);
        emit_textf("beta() { const cr = self.child.beta(); if (cr === null) { self.fail.alpha(); return; } ms._do_capture(cr, varname, %d); return cr; }\n", imm);
        emit_textf("}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * varname = nd->sval ? nd->sval : "";
        net_class_hdr(out, sid, nid);
        emit_textf("  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _child\n  .field private string _varname\n  .field private bool _immediate\n");
        emit_textf("  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox child, string varname, bool imm) cil managed\n  {\n");
        emit_textf("    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        emit_textf("    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n", sid, nid);
        emit_textf("    ldarg.0\n    ldarg.2\n    dup\n    brtrue     CAP_%d_%d_NN\n    pop\n    ldstr      \"\"\n  CAP_%d_%d_NN:\n    stfld      string pat_%d_%d::_varname\n", sid, nid, sid, nid, sid, nid);
        emit_textf("    ldarg.0\n    ldarg.3\n    stfld      bool pat_%d_%d::_immediate\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        emit_textf("    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_cr, int32 V_start, int32 V_len, string V_matched)\n");
        emit_textf("    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stloc.1\n");
        emit_textf("    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n    ldarg.1\n", sid, nid);
        emit_textf("    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        emit_textf("    stloc.0\n    ldloca.s   V_cr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n    brtrue     CAPC_%d_%d_FAIL\n", sid, nid);
        emit_textf("    ldloca.s   V_cr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Start\n    stloc.1\n");
        emit_textf("    ldloca.s   V_cr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    stloc.2\n");
        emit_textf("    ldarg.1\n    callvirt   instance string [boxes]Snobol4.Runtime.Boxes.MatchState::get_Subject()\n");
        emit_textf("    ldloc.1\n    ldloc.2\n    callvirt   instance string [mscorlib]System.String::Substring(int32, int32)\n    stloc.3\n");
        emit_textf("    ldarg.0\n    ldfld      string pat_%d_%d::_varname\n", sid, nid);
        emit_textf("    ldstr      \"OUTPUT\"\n    call       bool [mscorlib]System.String::op_Equality(string, string)\n");
        emit_textf("    brfalse    CAPC_%d_%d_NOTOUT\n    ldloc.3\n    call       void [mscorlib]System.Console::WriteLine(string)\n", sid, nid);
        emit_textf("    br         CAPC_%d_%d_DONE\n  CAPC_%d_%d_NOTOUT:\n  CAPC_%d_%d_DONE:\n    ldloc.0\n    ret\n", sid, nid, sid, nid, sid, nid);
        emit_textf("  CAPC_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); emit_textf("  }\n");
        net_beta_hdr(out);
        emit_textf("    .maxstack 2\n    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_child\n    ldarg.1\n", sid, nid);
        emit_textf("    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n    ret\n  }\n}\n");
        net_escape_ldstr(out, varname); net_push_i4(out, imm);
        emit_textf("    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox, string, bool)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
