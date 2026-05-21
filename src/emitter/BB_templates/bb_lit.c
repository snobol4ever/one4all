/* bb_lit.c — BB template for literal string match.
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Function body is byte-identical to the consolidated bb_pat.c original
   that this file restores; separators and includes match the original per-file
   shape pre-EC-UNI-13(a). */
#include "bb_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_lit(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "lit_%d_%d", sid, nid);
        jvm_class_hdr(out, "lit");
        emit_textf(".field private final lit Ljava/lang/String;\n");
        emit_textf(".field private final len I\n");
        emit_textf(".method public <init>(Lbb/bb_box$MatchState;Ljava/lang/String;)V\n");
        emit_textf("    .limit stack 3\n    .limit locals 3\n");
        emit_textf("    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        emit_textf("    aload_0\n    aload_2\n    putfield bb/bb_lit/lit Ljava/lang/String;\n");
        emit_textf("    aload_0\n    aload_2\n    invokevirtual java/lang/String/length()I\n    putfield bb/bb_lit/len I\n");
        emit_textf("    return\n.end method\n");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n");
        emit_textf("    .limit stack 6\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aload_0\n    getfield bb/bb_lit/len I\n    iadd\n");
        emit_textf("    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n");
        emit_textf("    if_icmpgt %s_omega\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
        emit_textf("    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aload_0\n    getfield bb/bb_lit/lit Ljava/lang/String;\n    iconst_0\n");
        emit_textf("    aload_0\n    getfield bb/bb_lit/len I\n");
        emit_textf("    invokevirtual java/lang/String/regionMatches(ILjava/lang/String;II)Z\n");
        emit_textf("    ifeq %s_omega\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        emit_textf("    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aload_0\n    getfield bb/bb_lit/len I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    getfield bb/bb_lit/len I\n");
        emit_textf("    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        emit_textf("%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n");
        emit_textf("    .limit stack 4\n    .limit locals 1\n");
        emit_textf("    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aload_0\n    getfield bb/bb_lit/len I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { const lit = ", nd->ival, nid);
        js_escape(out, nd->sval);
        emit_textf("; const len = lit.length; let self = { succ: null, fail: null,\n");
        emit_textf("alpha() { if (ms.delta + len > ms.omega || ms.sigma.slice(ms.delta, ms.delta + len) !== lit) { self.fail.alpha(); return; } ms.delta += len; self.succ.alpha(); },\n");
        emit_textf("beta() { ms.delta -= len; self.fail.alpha(); }\n");
        emit_textf("}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * lit = nd->sval ? nd->sval : "";
        net_class_hdr(out, sid, nid);
        emit_textf("  .field private string _lit\n  .field private int32  _len\n");
        emit_textf("  .method public specialname rtspecialname instance void .ctor(string lit) cil managed\n  {\n");
        emit_textf("    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        emit_textf("    ldarg.0\n    ldarg.1\n    dup\n    brtrue     LIT_%d_%d_NN\n    pop\n    ldstr      \"\"\n", sid, nid);
        emit_textf("  LIT_%d_%d_NN:\n", sid, nid);
        emit_textf("    stfld      string pat_%d_%d::_lit\n", sid, nid);
        emit_textf("    ldarg.0\n    ldarg.0\n");
        emit_textf("    ldfld      string pat_%d_%d::_lit\n", sid, nid);
        emit_textf("    callvirt   instance int32 [mscorlib]System.String::get_Length()\n");
        emit_textf("    stfld      int32 pat_%d_%d::_len\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        emit_textf("    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        net_cursor_load(out);
        emit_textf("    ldfld      int32 pat_%d_%d::_len\n    add\n", sid, nid);
        emit_textf("    ldarg.1\n"); net_ms_length(out);
        emit_textf("    bgt        LIT_%d_%d_A_FAIL\n", sid, nid);
        emit_textf("    ldarg.1\n");
        net_cursor_load(out);
        emit_textf("    ldfld      string pat_%d_%d::_lit\n", sid, nid);
        emit_textf("    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::MatchesAt(int32, string)\n");
        emit_textf("    brfalse    LIT_%d_%d_A_FAIL\n", sid, nid);
        net_cursor_load(out);
        emit_textf("    ldfld      int32 pat_%d_%d::_len\n", sid, nid);
        net_spec_of(out);
        emit_textf("    stloc.0\n");
        emit_textf("    ldarg.1\n    ldarg.1\n");
        net_cursor_load(out);
        emit_textf("    ldfld      int32 pat_%d_%d::_len\n    add\n", sid, nid);
        emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        emit_textf("    ldloc.0\n    ret\n");
        emit_textf("  LIT_%d_%d_A_FAIL:\n", sid, nid);
        net_fail_ret(out);
        emit_textf("  }\n");
        net_beta_hdr(out);
        emit_textf("    .maxstack 3\n");
        emit_textf("    ldarg.1\n    ldarg.1\n");
        net_cursor_load(out);
        emit_textf("    ldfld      int32 pat_%d_%d::_len\n    sub\n", sid, nid);
        emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        net_fail_ret(out);
        emit_textf("  }\n}\n");
        net_escape_ldstr(out, lit);
        emit_textf("    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
