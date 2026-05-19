#include "bb_template_common.h"

void ec_bb_span(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_TEXT || IS_BIN) { /* x86: via emit_bb_charset/bb_span — not wired here yet (EC-3+). */ return; }
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "span_%d_%d", sid, nid);
        ec_jvm_class_hdr(out, "span"); fprintf(out, ".field private final chars Ljava/lang/String;\n.field private matched_len I\n"); ec_jvm_init_ms_str(out, "span", "chars");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 3\n");
        fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_span/matched_len I\n%s_loop:\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_span/matched_len I\n    iadd\n    istore_1\n");
        fprintf(out, "    iload_1\n    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpge %s_done\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_span/chars Ljava/lang/String;\n    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n    iload_1\n    invokevirtual java/lang/String/charAt(I)C\n");
        fprintf(out, "    invokevirtual java/lang/String/indexOf(I)I\n    iflt %s_done\n    aload_0\n    dup\n    getfield bb/bb_span/matched_len I\n    iconst_1\n    iadd\n    putfield bb/bb_span/matched_len I\n    goto %s_loop\n", tag, tag);
        fprintf(out, "%s_done:\n    aload_0\n    getfield bb/bb_span/matched_len I\n    ifle %s_omega\n", tag, tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_span/matched_len I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_2\n    aload_0\n    getfield bb/bb_span/matched_len I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_span/matched_len I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid); ec_js_escape(out, nd->sval);
        fprintf(out, "; let delta = 0; let self = { succ: null, fail: null,\nalpha() { delta = 0; while (ms.delta + delta < ms.omega && chars.indexOf(ms.sigma[ms.delta + delta]) >= 0) delta++; if (delta <= 0) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\nbeta() { ms.delta -= delta; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * chars = nd->sval ? nd->sval : "";
        ec_net_class_hdr(out, sid, nid); fprintf(out, "  .field private string _chars\n  .field private int32  _count\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(string chars) cil managed\n  {\n    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    dup\n    brtrue     SP_%d_%d_NN\n    pop\n    ldstr      \"\"\n  SP_%d_%d_NN:\n    stfld      string pat_%d_%d::_chars\n    ret\n  }\n", sid, nid, sid, nid, sid, nid);
        ec_net_alpha_hdr(out); fprintf(out, "    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_count\n  SP_%d_%d_LOOP:\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.1\n"); ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    ldfld      string pat_%d_%d::_chars\n    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n    brfalse    SP_%d_%d_DONE\n", sid, nid, sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_count\n    br         SP_%d_%d_LOOP\n  SP_%d_%d_DONE:\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.0\n    ble        SP_%d_%d_FAIL\n", sid, nid, sid, nid, sid, nid, sid, nid, sid, nid, sid, nid);
        ec_net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n", sid, nid); ec_net_spec_of(out); fprintf(out, "    stloc.0\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  SP_%d_%d_FAIL:\n", sid, nid, sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n");
        ec_net_beta_hdr(out); fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); ec_net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    sub\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n", sid, nid); ec_net_fail_ret(out); fprintf(out, "  }\n}\n");
        ec_net_escape_ldstr(out, chars); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
}
