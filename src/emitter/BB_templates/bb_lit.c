#include "bb_template_common.h"

void bb_lit(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM_TEXT) {
        char tag[32]; snprintf(tag, sizeof tag, "lit_%d_%d", sid, nid);
        jvm_class_hdr(out, "lit");
        fprintf(out, ".field private final lit Ljava/lang/String;\n");
        fprintf(out, ".field private final len I\n");
        fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Ljava/lang/String;)V\n");
        fprintf(out, "    .limit stack 3\n    .limit locals 3\n");
        fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_lit/lit Ljava/lang/String;\n");
        fprintf(out, "    aload_0\n    aload_2\n    invokevirtual java/lang/String/length()I\n    putfield bb/bb_lit/len I\n");
        fprintf(out, "    return\n.end method\n");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n");
        fprintf(out, "    .limit stack 6\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n    iadd\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n");
        fprintf(out, "    if_icmpgt %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/lit Ljava/lang/String;\n    iconst_0\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n");
        fprintf(out, "    invokevirtual java/lang/String/regionMatches(ILjava/lang/String;II)Z\n");
        fprintf(out, "    ifeq %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    getfield bb/bb_lit/len I\n");
        fprintf(out, "    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n");
        fprintf(out, "    .limit stack 4\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_lit/len I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) {
        fprintf(out, "function make_pat_%d_%d(ms) { const lit = ", nd->ival, nid);
        js_escape(out, nd->sval);
        fprintf(out, "; const len = lit.length; let self = { succ: null, fail: null,\n");
        fprintf(out, "alpha() { if (ms.delta + len > ms.omega || ms.sigma.slice(ms.delta, ms.delta + len) !== lit) { self.fail.alpha(); return; } ms.delta += len; self.succ.alpha(); },\n");
        fprintf(out, "beta() { ms.delta -= len; self.fail.alpha(); }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_NET_TEXT) {
        const char * lit = nd->sval ? nd->sval : "";
        net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private string _lit\n  .field private int32  _len\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(string lit) cil managed\n  {\n");
        fprintf(out, "    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    dup\n    brtrue     LIT_%d_%d_NN\n    pop\n    ldstr      \"\"\n", sid, nid);
        fprintf(out, "  LIT_%d_%d_NN:\n", sid, nid);
        fprintf(out, "    stfld      string pat_%d_%d::_lit\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.0\n");
        fprintf(out, "    ldfld      string pat_%d_%d::_lit\n", sid, nid);
        fprintf(out, "    callvirt   instance int32 [mscorlib]System.String::get_Length()\n");
        fprintf(out, "    stfld      int32 pat_%d_%d::_len\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        net_cursor_load(out);
        fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n    add\n", sid, nid);
        fprintf(out, "    ldarg.1\n"); net_ms_length(out);
        fprintf(out, "    bgt        LIT_%d_%d_A_FAIL\n", sid, nid);
        fprintf(out, "    ldarg.1\n");
        net_cursor_load(out);
        fprintf(out, "    ldfld      string pat_%d_%d::_lit\n", sid, nid);
        fprintf(out, "    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::MatchesAt(int32, string)\n");
        fprintf(out, "    brfalse    LIT_%d_%d_A_FAIL\n", sid, nid);
        net_cursor_load(out);
        fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n", sid, nid);
        net_spec_of(out);
        fprintf(out, "    stloc.0\n");
        fprintf(out, "    ldarg.1\n    ldarg.1\n");
        net_cursor_load(out);
        fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n    add\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        fprintf(out, "    ldloc.0\n    ret\n");
        fprintf(out, "  LIT_%d_%d_A_FAIL:\n", sid, nid);
        net_fail_ret(out);
        fprintf(out, "  }\n");
        net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n");
        fprintf(out, "    ldarg.1\n    ldarg.1\n");
        net_cursor_load(out);
        fprintf(out, "    ldfld      int32 pat_%d_%d::_len\n    sub\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        net_fail_ret(out);
        fprintf(out, "  }\n}\n");
        net_escape_ldstr(out, lit);
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }
    /* IS_WASM_TEXT: n/a — BB WASM never landed in original code */
    /* IS_WASM_BIN: n/a — BB WASM never landed in original code */
}
