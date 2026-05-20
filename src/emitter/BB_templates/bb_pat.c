/* bb_pat.c — consolidated BB pattern templates.
   EC-UNI-13 (2026-05-20): the 16 single-file PAT BB templates (bb_lit.c .. bb_capture.c)
   were merged into this single TU. Function bodies are byte-identical to the per-file
   originals; only the leading #include was deduplicated, blank lines stripped, and 200-col
   separators inserted between functions per RULES.md C-style. Forward decls in
   bb_templates.h are unchanged. */
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_any(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "any_%d_%d", sid, nid);
        jvm_class_hdr(out, "any"); emit_textf(".field private final chars Ljava/lang/String;\n"); jvm_init_ms_str(out, "any", "chars");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpge %s_omega\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_any/chars Ljava/lang/String;\n");
        emit_textf("    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
        emit_textf("    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    invokevirtual java/lang/String/charAt(I)C\n    invokevirtual java/lang/String/indexOf(I)I\n    iflt %s_omega\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        emit_textf("    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    new bb/bb_box$Spec\n    dup\n    iload_1\n    iconst_1\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        emit_textf("%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        emit_textf("    aload_0\n    getfield bb/bb_any/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid); js_escape(out, nd->sval);
        emit_textf("; let self = { succ: null, fail: null,\nalpha() { if (ms.delta >= ms.omega || chars.indexOf(ms.sigma[ms.delta]) < 0) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + 1); ms.delta++; self.succ.alpha(); return r; },\nbeta() { ms.delta--; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * chars = nd->sval ? nd->sval : "";
        net_charset_class(out, sid, nid, "ANY"); net_alpha_hdr(out);
        emit_textf("    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldfld      string pat_%d_%d::_chars\n    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n    brfalse    ANY_%d_%d_A_FAIL\n", sid, nid, sid, nid);
        net_cursor_load(out); emit_textf("    ldc.i4.1\n"); net_spec_of(out); emit_textf("    stloc.0\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldc.i4.1\n    add\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  ANY_%d_%d_A_FAIL:\n", sid, nid);
        net_fail_ret(out); emit_textf("  }\n"); net_beta_hdr(out);
        emit_textf("    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldc.i4.1\n    sub\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n"); net_fail_ret(out); emit_textf("  }\n}\n");
        net_escape_ldstr(out, chars); emit_textf("    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_notany(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "notany_%d_%d", sid, nid);
        jvm_class_hdr(out, "notany"); emit_textf(".field private final chars Ljava/lang/String;\n"); jvm_init_ms_str(out, "notany", "chars");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpge %s_omega\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_notany/chars Ljava/lang/String;\n");
        emit_textf("    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n");
        emit_textf("    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    invokevirtual java/lang/String/charAt(I)C\n    invokevirtual java/lang/String/indexOf(I)I\n    ifge %s_omega\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        emit_textf("    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    new bb/bb_box$Spec\n    dup\n    iload_1\n    iconst_1\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        emit_textf("%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        emit_textf("    aload_0\n    getfield bb/bb_notany/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    iconst_1\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid); js_escape(out, nd->sval);
        emit_textf("; let self = { succ: null, fail: null,\nalpha() { if (ms.delta >= ms.omega || chars.indexOf(ms.sigma[ms.delta]) >= 0) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + 1); ms.delta++; self.succ.alpha(); return r; },\nbeta() { ms.delta--; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * chars = nd->sval ? nd->sval : "";
        net_charset_class(out, sid, nid, "NOTANY"); net_alpha_hdr(out);
        emit_textf("    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldfld      string pat_%d_%d::_chars\n    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n    brtrue     NOTANY_%d_%d_A_FAIL\n", sid, nid, sid, nid);
        net_cursor_load(out); emit_textf("    ldc.i4.1\n"); net_spec_of(out); emit_textf("    stloc.0\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldc.i4.1\n    add\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  NOTANY_%d_%d_A_FAIL:\n", sid, nid);
        net_fail_ret(out); emit_textf("  }\n"); net_beta_hdr(out);
        emit_textf("    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldc.i4.1\n    sub\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n"); net_fail_ret(out); emit_textf("  }\n}\n");
        net_escape_ldstr(out, chars); emit_textf("    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_span(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "span_%d_%d", sid, nid);
        jvm_class_hdr(out, "span"); emit_textf(".field private final chars Ljava/lang/String;\n.field private matched_len I\n"); jvm_init_ms_str(out, "span", "chars");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 3\n");
        emit_textf("    aload_0\n    iconst_0\n    putfield bb/bb_span/matched_len I\n%s_loop:\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aload_0\n    getfield bb/bb_span/matched_len I\n    iadd\n    istore_1\n");
        emit_textf("    iload_1\n    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpge %s_done\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_span/chars Ljava/lang/String;\n    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n    iload_1\n    invokevirtual java/lang/String/charAt(I)C\n");
        emit_textf("    invokevirtual java/lang/String/indexOf(I)I\n    iflt %s_done\n    aload_0\n    dup\n    getfield bb/bb_span/matched_len I\n    iconst_1\n    iadd\n    putfield bb/bb_span/matched_len I\n    goto %s_loop\n", tag, tag);
        emit_textf("%s_done:\n    aload_0\n    getfield bb/bb_span/matched_len I\n    ifle %s_omega\n", tag, tag);
        emit_textf("    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_2\n");
        emit_textf("    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_span/matched_len I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    new bb/bb_box$Spec\n    dup\n    iload_2\n    aload_0\n    getfield bb/bb_span/matched_len I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        emit_textf("    aload_0\n    getfield bb/bb_span/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_span/matched_len I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid); js_escape(out, nd->sval);
        emit_textf("; let delta = 0; let self = { succ: null, fail: null,\nalpha() { delta = 0; while (ms.delta + delta < ms.omega && chars.indexOf(ms.sigma[ms.delta + delta]) >= 0) delta++; if (delta <= 0) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\nbeta() { ms.delta -= delta; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * chars = nd->sval ? nd->sval : "";
        net_class_hdr(out, sid, nid); emit_textf("  .field private string _chars\n  .field private int32  _count\n");
        emit_textf("  .method public specialname rtspecialname instance void .ctor(string chars) cil managed\n  {\n    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        emit_textf("    ldarg.0\n    ldarg.1\n    dup\n    brtrue     SP_%d_%d_NN\n    pop\n    ldstr      \"\"\n  SP_%d_%d_NN:\n    stfld      string pat_%d_%d::_chars\n    ret\n  }\n", sid, nid, sid, nid, sid, nid);
        net_alpha_hdr(out); emit_textf("    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_count\n  SP_%d_%d_LOOP:\n", sid, nid, sid, nid);
        emit_textf("    ldarg.1\n"); net_cursor_load(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    ldfld      string pat_%d_%d::_chars\n    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n    brfalse    SP_%d_%d_DONE\n", sid, nid, sid, nid, sid, nid);
        emit_textf("    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_count\n    br         SP_%d_%d_LOOP\n  SP_%d_%d_DONE:\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.0\n    ble        SP_%d_%d_FAIL\n", sid, nid, sid, nid, sid, nid, sid, nid, sid, nid, sid, nid);
        net_cursor_load(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n", sid, nid); net_spec_of(out); emit_textf("    stloc.0\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  SP_%d_%d_FAIL:\n", sid, nid, sid, nid); net_fail_ret(out); emit_textf("  }\n");
        net_beta_hdr(out); emit_textf("    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    sub\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n", sid, nid); net_fail_ret(out); emit_textf("  }\n}\n");
        net_escape_ldstr(out, chars); emit_textf("    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_break(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "brk_%d_%d", sid, nid);
        jvm_class_hdr(out, "brk"); emit_textf(".field private final chars Ljava/lang/String;\n.field private matched_len I\n"); jvm_init_ms_str(out, "brk", "chars");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 3\n    aload_0\n    iconst_0\n    putfield bb/bb_brk/matched_len I\n%s_loop:\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_brk/matched_len I\n    iadd\n    istore_1\n");
        emit_textf("    iload_1\n    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpge %s_omega\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_brk/chars Ljava/lang/String;\n    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/sigma Ljava/lang/String;\n    iload_1\n    invokevirtual java/lang/String/charAt(I)C\n");
        emit_textf("    invokevirtual java/lang/String/indexOf(I)I\n    ifge %s_found\n    aload_0\n    dup\n    getfield bb/bb_brk/matched_len I\n    iconst_1\n    iadd\n    putfield bb/bb_brk/matched_len I\n    goto %s_loop\n%s_found:\n", tag, tag, tag);
        emit_textf("    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_2\n");
        emit_textf("    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_brk/matched_len I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    new bb/bb_box$Spec\n    dup\n    iload_2\n    aload_0\n    getfield bb/bb_brk/matched_len I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        emit_textf("    aload_0\n    getfield bb/bb_brk/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_brk/matched_len I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { const chars = ", nd->ival, nid); js_escape(out, nd->sval);
        emit_textf("; let delta = 0; let self = { succ: null, fail: null,\nalpha() { delta = 0; while (ms.delta + delta < ms.omega && chars.indexOf(ms.sigma[ms.delta + delta]) < 0) delta++; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\nbeta() { ms.delta -= delta; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        const char * chars = nd->sval ? nd->sval : "";
        net_class_hdr(out, sid, nid); emit_textf("  .field private string _chars\n  .field private int32  _count\n");
        emit_textf("  .method public specialname rtspecialname instance void .ctor(string chars) cil managed\n  {\n    .maxstack 3\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        emit_textf("    ldarg.0\n    ldarg.1\n    dup\n    brtrue     BRK_%d_%d_NN\n    pop\n    ldstr      \"\"\n  BRK_%d_%d_NN:\n    stfld      string pat_%d_%d::_chars\n    ret\n  }\n", sid, nid, sid, nid, sid, nid);
        net_alpha_hdr(out); emit_textf("    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_count\n  BRK_%d_%d_LOOP:\n", sid, nid, sid, nid);
        net_cursor_load(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    ldarg.1\n", sid, nid); net_ms_length(out);
        emit_textf("    bge        BRK_%d_%d_EOS\n    ldarg.1\n", sid, nid); net_cursor_load(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    ldarg.0\n    ldfld      string pat_%d_%d::_chars\n", sid, nid, sid, nid);
        emit_textf("    callvirt   instance bool [boxes]Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)\n    brtrue     BRK_%d_%d_FOUND\n", sid, nid);
        emit_textf("    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_count\n    br         BRK_%d_%d_LOOP\n  BRK_%d_%d_EOS:\n", sid, nid, sid, nid, sid, nid, sid, nid); net_fail_ret(out);
        emit_textf("  BRK_%d_%d_FOUND:\n", sid, nid); net_cursor_load(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n", sid, nid); net_spec_of(out); emit_textf("    stloc.0\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  }\n", sid, nid);
        net_beta_hdr(out); emit_textf("    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    sub\n    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n", sid, nid); net_fail_ret(out); emit_textf("  }\n}\n");
        net_escape_ldstr(out, chars); emit_textf("    newobj     instance void pat_%d_%d::.ctor(string)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_arb(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "arb_%d_%d", sid, nid);
        jvm_class_hdr(out, "arb");
        emit_textf(".field private arb_count I\n.field private arb_start I\n");
        jvm_init_ms_only(out, "arb");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        emit_textf("    aload_0\n    iconst_0\n    putfield bb/bb_arb/arb_count I\n");
        emit_textf("    aload_0\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_arb/arb_start I\n");
        emit_textf("    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        emit_textf("    aload_0\n    dup\n    getfield bb/bb_arb/arb_count I\n    iconst_1\n    iadd\n    putfield bb/bb_arb/arb_count I\n");
        emit_textf("    aload_0\n    getfield bb/bb_arb/arb_start I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    iadd\n");
        emit_textf("    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpgt %s_omega\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_arb/arb_start I\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n");
        emit_textf("    aload_0\n    getfield bb/bb_arb/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arb/arb_count I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    areturn\n%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { let delta = 0; let self = { succ: null, fail: null,\n", nd->ival, nid);
        emit_textf("alpha() { delta = ms.omega - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
        emit_textf("beta() { if (delta <= 0) { self.fail.alpha(); return; } delta--; ms.delta--; const r = ms.sigma.slice(ms.delta, ms.delta + delta + 1); return r; }\n");
        emit_textf("}; return self; }\n");
        return;
    }
    if (IS_NET) {
        net_class_hdr(out, sid, nid);
        emit_textf("  .field private int32 _count\n  .field private int32 _start\n");
        net_ctor_none(out, sid, nid);
        net_alpha_hdr(out);
        emit_textf("    .maxstack 2\n");
        emit_textf("    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_count\n", sid, nid);
        emit_textf("    ldarg.0\n    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stfld      int32 pat_%d_%d::_start\n", sid, nid);
        net_cursor_load(out); net_spec_zw(out); emit_textf("    ret\n  }\n");
        net_beta_hdr(out);
        emit_textf("    .maxstack 3\n");
        emit_textf("    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_count\n", sid, nid, sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
        emit_textf("    ldarg.1\n"); net_ms_length(out);
        emit_textf("    bgt        ARB_%d_%d_FAIL\n", sid, nid);
        emit_textf("    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n    add\n", sid, nid);
        emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_start\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_count\n", sid, nid);
        net_spec_of(out); emit_textf("    ret\n");
        emit_textf("  ARB_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); emit_textf("  }\n}\n");
        emit_textf("    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_arbno(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "arbno_%d_%d", sid, nid);
        jvm_class_hdr(out, "arbno");
        emit_textf(".field private static final MAX_DEPTH I = 64\n");
        emit_textf(".field private final body Lbb/bb_box;\n");
        emit_textf(".field private final frame_start [I\n.field private final frame_match_st [I\n.field private final frame_match_ln [I\n");
        emit_textf(".field private depth I\n");
        emit_textf(".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;)V\n    .limit stack 4\n    .limit locals 3\n");
        emit_textf("    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        emit_textf("    aload_0\n    aload_2\n    putfield bb/bb_arbno/body Lbb/bb_box;\n");
        emit_textf("    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_start [I\n");
        emit_textf("    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_match_st [I\n");
        emit_textf("    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_match_ln [I\n");
        emit_textf("    return\n.end method\n");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        emit_textf("    aload_0\n    iconst_0\n    putfield bb/bb_arbno/depth I\n");
        emit_textf("    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    iconst_0\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
        emit_textf("    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    iconst_0\n    iconst_0\n    iastore\n");
        emit_textf("    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    iconst_0\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
        emit_textf("    aload_0\n    invokevirtual bb/bb_arbno/tryBody()Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        emit_textf("    aload_0\n    getfield bb/bb_arbno/depth I\n    ifle %s_beta_omega\n", tag);
        emit_textf("    aload_0\n    dup\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    isub\n    putfield bb/bb_arbno/depth I\n");
        emit_textf("    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        emit_textf("%s_beta_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        emit_textf(".method private tryBody()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 4\n%s_tryBody_loop:\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_arbno/body Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_tryBody_omega\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    if_icmpne %s_tryBody_advance\n", tag);
        emit_textf("    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        emit_textf("%s_tryBody_advance:\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    istore_2\n");
        emit_textf("    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n");
        emit_textf("    aload_0\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    iadd\n    bipush 64\n    if_icmpge %s_tryBody_full\n", tag);
        emit_textf("    aload_0\n    dup\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    iadd\n    putfield bb/bb_arbno/depth I\n");
        emit_textf("    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iload_2\n    iastore\n");
        emit_textf("    istore 3\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iload 3\n    iastore\n");
        emit_textf("    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
        emit_textf("    goto %s_tryBody_loop\n%s_tryBody_full:\n", tag, tag);
        emit_textf("    new bb/bb_box$Spec\n    dup_x1\n    swap\n    iload_2\n    swap\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        emit_textf("%s_tryBody_omega:\n", tag);
        emit_textf("    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { const stack = []; let self = { succ: null, fail: null,\n", nd->ival, nid);
        emit_textf("alpha() { stack.length = 0; stack.push({ start: ms.delta }); while (true) { const frame = stack[stack.length - 1]; const br = self.body.alpha();\n");
        emit_textf("if (br === null) { return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
        emit_textf("if (ms.delta === frame.start) { return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
        emit_textf("stack.push({ start: ms.delta }); } },\n");
        emit_textf("beta() { if (stack.length <= 1) { self.fail.alpha(); return; } stack.pop(); const frame = stack[stack.length - 1]; ms.delta = frame.start; return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
        emit_textf("}; return self; }\n");
        return;
    }
    if (IS_NET) {
        net_class_hdr(out, sid, nid);
        emit_textf("  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _body\n");
        emit_textf("  .field private int32[] _matchStart\n  .field private int32[] _matchLen\n");
        emit_textf("  .field private int32[] _startStack\n  .field private int32   _depth\n");
        emit_textf("  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox body) cil managed\n  {\n");
        emit_textf("    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        emit_textf("    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_body\n", sid, nid);
        emit_textf("    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
        emit_textf("    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        emit_textf("    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_startStack\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        emit_textf("    .maxstack 4\n    .locals init (int32 V_startHere, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_br)\n");
        emit_textf("    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldc.i4.0\n", sid, nid); net_cursor_load(out); emit_textf("    stelem.i4\n");
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n    ldc.i4.0\n    ldc.i4.0\n    stelem.i4\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n    ldc.i4.0\n", sid, nid); net_cursor_load(out); emit_textf("    stelem.i4\n");
        emit_textf("  ARBNO_%d_%d_LOOP:\n", sid, nid); net_cursor_load(out); emit_textf("    stloc.0\n");
        emit_textf("    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_body\n    ldarg.1\n", sid, nid);
        emit_textf("    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        emit_textf("    stloc.1\n    ldloca.s   V_br\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        emit_textf("    brtrue     ARBNO_%d_%d_STOP\n", sid, nid); net_cursor_load(out); emit_textf("    ldloc.0\n    beq        ARBNO_%d_%d_STOP\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4     63\n    bge        ARBNO_%d_%d_STOP\n", sid, nid, sid, nid);
        emit_textf("    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldc.i4.0\n    ldelem.i4\n    stelem.i4\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    sub\n    ldelem.i4\n", sid, nid);
        emit_textf("    ldloca.s   V_br\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n    stelem.i4\n");
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        net_cursor_load(out); emit_textf("    stelem.i4\n    br         ARBNO_%d_%d_LOOP\n", sid, nid);
        emit_textf("  ARBNO_%d_%d_STOP:\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        net_spec_of(out); emit_textf("    ret\n  }\n");
        net_beta_hdr(out);
        emit_textf("    .maxstack 3\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.0\n    ble        ARBNO_%d_%d_BFAIL\n", sid, nid, sid, nid);
        emit_textf("    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    sub\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        emit_textf("    ldarg.1\n    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        net_spec_of(out); emit_textf("    ret\n  ARBNO_%d_%d_BFAIL:\n", sid, nid); net_fail_ret(out); emit_textf("  }\n}\n");
        emit_textf("    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_cat(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "seq_%d_%d", sid, nid);
        jvm_class_hdr(out, "seq");
        emit_textf(".field private final left Lbb/bb_box;\n.field private final right Lbb/bb_box;\n");
        emit_textf(".field private matched_start I\n.field private matched_len I\n");
        emit_textf(".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;Lbb/bb_box;)V\n    .limit stack 3\n    .limit locals 4\n");
        emit_textf("    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        emit_textf("    aload_0\n    aload_2\n    putfield bb/bb_seq/left Lbb/bb_box;\n");
        emit_textf("    aload_0\n    aload_3\n    putfield bb/bb_seq/right Lbb/bb_box;\n    return\n.end method\n");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        emit_textf("    aload_0\n    aload_0\n    getfield bb/bb_seq/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_seq/matched_start I\n");
        emit_textf("    aload_0\n    iconst_0\n    putfield bb/bb_seq/matched_len I\n");
        emit_textf("    aload_0\n    getfield bb/bb_seq/left Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_omega\n", tag);
        emit_textf("    aload_0\n    dup\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    putfield bb/bb_seq/matched_len I\n");
        emit_textf("    aload_0\n    invokevirtual bb/bb_seq/rightAlpha()Lbb/bb_box$Spec;\n    areturn\n");
        emit_textf("%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_seq/right Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_beta_right_omega\n", tag);
        emit_textf("    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_seq/matched_start I\n    aload_0\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        emit_textf("%s_beta_right_omega:\n    aload_0\n    invokevirtual bb/bb_seq/leftBeta()Lbb/bb_box$Spec;\n    areturn\n.end method\n", tag);
        emit_textf(".method private rightAlpha()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_seq/right Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_rA_omega\n", tag);
        emit_textf("    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_seq/matched_start I\n    aload_0\n    getfield bb/bb_seq/matched_len I\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        emit_textf("%s_rA_omega:\n    aload_0\n    invokevirtual bb/bb_seq/leftBeta()Lbb/bb_box$Spec;\n    areturn\n.end method\n", tag);
        emit_textf(".method private leftBeta()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_seq/left Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_lB_omega\n", tag);
        emit_textf("    aload_0\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    putfield bb/bb_seq/matched_len I\n");
        emit_textf("    aload_0\n    invokevirtual bb/bb_seq/rightAlpha()Lbb/bb_box$Spec;\n    areturn\n");
        emit_textf("%s_lB_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        emit_textf("alpha() { const lr = self.left.alpha(); if (lr === null) { self.fail.alpha(); return; }\n");
        emit_textf("let rr = self.right.alpha(); while (rr === null) { const lr2 = self.left.beta(); if (lr2 === null) { self.fail.alpha(); return; } rr = self.right.alpha(); }\n");
        emit_textf("self.succ.alpha(); return rr; },\n");
        emit_textf("beta() { let rr = self.right.beta(); while (rr === null) { const lr = self.left.beta(); if (lr === null) { self.fail.alpha(); return; } rr = self.right.alpha(); }\n");
        emit_textf("return rr; }\n");
        emit_textf("}; return self; }\n");
        return;
    }
    if (IS_NET) {
        net_class_hdr(out, sid, nid);
        emit_textf("  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _left\n");
        emit_textf("  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _right\n");
        emit_textf("  .field private int32 _mStart\n  .field private int32 _mLen\n");
        emit_textf("  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox left, class [boxes]Snobol4.Runtime.Boxes.IByrdBox right) cil managed\n  {\n");
        emit_textf("    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        emit_textf("    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n", sid, nid);
        emit_textf("    ldarg.0\n    ldarg.2\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        emit_textf("    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_lr, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_rr)\n");
        emit_textf("    ldarg.0\n"); net_cursor_load(out); emit_textf("    stfld      int32 pat_%d_%d::_mStart\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n    ldarg.1\n", sid, nid);
        emit_textf("    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        emit_textf("    stloc.0\n    ldloca.s   V_lr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        emit_textf("    brtrue     CAT_%d_%d_FAIL\n", sid, nid);
        emit_textf("    ldarg.0\n    ldloca.s   V_lr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    stfld      int32 pat_%d_%d::_mLen\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ldarg.1\n", sid, nid);
        emit_textf("    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        emit_textf("    stloc.1\n    ldloca.s   V_rr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        emit_textf("    brtrue     CAT_%d_%d_FAIL\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_mStart\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_mLen\n", sid, nid);
        emit_textf("    ldloca.s   V_rr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n");
        net_spec_of(out); emit_textf("    ret\n  CAT_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); emit_textf("  }\n");
        net_beta_hdr(out);
        emit_textf("    .maxstack 2\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_rr)\n");
        emit_textf("    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_right\n    ldarg.1\n", sid, nid);
        emit_textf("    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        emit_textf("    stloc.0\n    ldloca.s   V_rr\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        emit_textf("    brfalse    CAT_%d_%d_BNOK\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_left\n    ldarg.1\n", sid, nid);
        emit_textf("    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Beta(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        emit_textf("    ret\n  CAT_%d_%d_BNOK:\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_mStart\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_mLen\n", sid, nid);
        emit_textf("    ldloca.s   V_rr\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n");
        net_spec_of(out); emit_textf("    ret\n  }\n}\n");
        emit_textf("    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox, class [boxes]Snobol4.Runtime.Boxes.IByrdBox)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_alt(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "alt_%d_%d", sid, nid);
        jvm_class_hdr(out, "alt");
        emit_textf(".field private final children [Lbb/bb_box;\n.field private final n I\n.field private current I\n.field private position I\n");
        emit_textf(".method public <init>(Lbb/bb_box$MatchState;[Lbb/bb_box;)V\n    .limit stack 3\n    .limit locals 3\n");
        emit_textf("    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        emit_textf("    aload_0\n    aload_2\n    putfield bb/bb_alt/children [Lbb/bb_box;\n");
        emit_textf("    aload_0\n    aload_2\n    arraylength\n    putfield bb/bb_alt/n I\n    return\n.end method\n");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 3\n    .limit locals 1\n");
        emit_textf("    aload_0\n    aload_0\n    getfield bb/bb_alt/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    putfield bb/bb_alt/position I\n");
        emit_textf("    aload_0\n    iconst_1\n    putfield bb/bb_alt/current I\n");
        emit_textf("    aload_0\n    invokevirtual bb/bb_alt/tryAlpha()Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_alt/children [Lbb/bb_box;\n    aload_0\n    getfield bb/bb_alt/current I\n    iconst_1\n    isub\n    aaload\n");
        emit_textf("    invokevirtual bb/bb_box/\316\262()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_beta_omega\n", tag);
        emit_textf("    aload_1\n    areturn\n%s_beta_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        emit_textf(".method private tryAlpha()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 2\n%s_try_loop:\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_alt/current I\n    aload_0\n    getfield bb/bb_alt/n I\n    if_icmpgt %s_try_omega\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_alt/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_alt/position I\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aload_0\n    getfield bb/bb_alt/children [Lbb/bb_box;\n    aload_0\n    getfield bb/bb_alt/current I\n    iconst_1\n    isub\n    aaload\n");
        emit_textf("    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_try_next\n    aload_1\n    areturn\n", tag);
        emit_textf("%s_try_next:\n    aload_0\n    dup\n    getfield bb/bb_alt/current I\n    iconst_1\n    iadd\n    putfield bb/bb_alt/current I\n    goto %s_try_loop\n", tag, tag);
        emit_textf("%s_try_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { const children = self.children || []; let idx = 0; let self = { succ: null, fail: null,\n", nd->ival, nid);
        emit_textf("alpha() { idx = 0; while (idx < children.length) { const r = children[idx].alpha(); if (r !== null) { self.succ.alpha(); return r; } idx++; } self.fail.alpha(); return null; },\n");
        emit_textf("beta() { idx--; if (idx >= 0 && idx < children.length) { const r = children[idx].beta(); if (r !== null) { return r; } return self.beta(); } self.fail.alpha(); return null; }\n");
        emit_textf("}; return self; }\n");
        return;
    }
    if (IS_NET) {
        net_class_hdr(out, sid, nid);
        emit_textf("  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] _children\n  .field private int32 _idx\n  .field private int32 _savedPos\n");
        emit_textf("  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] children) cil managed\n  {\n");
        emit_textf("    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        emit_textf("    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        emit_textf("    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        emit_textf("    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid);
        emit_textf("    ldarg.0\n    ldarg.1\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    stfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
        emit_textf("  ALT_%d_%d_LOOP:\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ldlen\n    conv.i4\n", sid, nid);
        emit_textf("    bge        ALT_%d_%d_FAIL\n", sid, nid);
        emit_textf("    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
        emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        emit_textf("    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldelem.ref\n    ldarg.1\n", sid, nid);
        emit_textf("    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        emit_textf("    stloc.0\n    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid, sid, nid);
        emit_textf("    ldloca.s   V_r\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        emit_textf("    brtrue     ALT_%d_%d_LOOP\n    ldloc.0\n    ret\n  ALT_%d_%d_FAIL:\n", sid, nid, sid, nid); net_fail_ret(out); emit_textf("  }\n");
        net_beta_hdr(out);
        emit_textf("    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n  ALT_%d_%d_BLOOP:\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n    ldlen\n    conv.i4\n", sid, nid);
        emit_textf("    bge        ALT_%d_%d_BFAIL\n", sid, nid);
        emit_textf("    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_savedPos\n", sid, nid);
        emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        emit_textf("    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox[] pat_%d_%d::_children\n", sid, nid);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldelem.ref\n    ldarg.1\n", sid, nid);
        emit_textf("    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        emit_textf("    stloc.0\n    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_idx\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_idx\n", sid, nid, sid, nid);
        emit_textf("    ldloca.s   V_r\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        emit_textf("    brtrue     ALT_%d_%d_BLOOP\n    ldloc.0\n    ret\n  ALT_%d_%d_BFAIL:\n", sid, nid, sid, nid); net_fail_ret(out); emit_textf("  }\n}\n");
        emit_textf("    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox[])\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_len(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "len_%d_%d", sid, nid);
        jvm_class_hdr(out, "len");
        emit_textf(".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n");
        jvm_init_ms_int(out, "len", "n"); jvm_val_helper(out, "len");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    iadd\n");
        emit_textf("    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpgt %s_omega\n", tag);
        emit_textf("    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        emit_textf("    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        emit_textf("%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        emit_textf("    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        int64_t n = nd->ival;
        emit_textf("function make_pat_%d_%d(ms) { const n = %ld; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        emit_textf("alpha() { if (ms.delta + n > ms.omega) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + n); ms.delta += n; self.succ.alpha(); return r; },\n");
        emit_textf("beta() { ms.delta -= n; self.fail.alpha(); }\n");
        emit_textf("}; return self; }\n");
        return;
    }
    if (IS_NET) {
        int n = (int)nd->ival;
        net_class_hdr(out, sid, nid);
        emit_textf("  .field private int32 _n\n");
        emit_textf("  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
        emit_textf("    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        emit_textf("    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        emit_textf("    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        net_cursor_load(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    add\n", sid, nid);
        emit_textf("    ldarg.1\n"); net_ms_length(out); emit_textf("    bgt        LEN_%d_%d_FAIL\n", sid, nid);
        net_cursor_load(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid);
        net_spec_of(out); emit_textf("    stloc.0\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    add\n", sid, nid);
        emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n");
        emit_textf("  LEN_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); emit_textf("  }\n");
        net_beta_hdr(out);
        emit_textf("    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n", sid, nid);
        emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n"); net_fail_ret(out); emit_textf("  }\n}\n");
        net_push_i4(out, n); emit_textf("    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_tab(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0; int rtab = (nd->ival2 != 0);
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        const char * name = rtab ? "rtab" : "tab"; char tag[32]; snprintf(tag, sizeof tag, "%s_%d_%d", name, sid, nid);
        jvm_class_hdr(out, name);
        emit_textf(".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n.field private advance I\n");
        jvm_init_ms_int(out, name, "n"); jvm_val_helper(out, name);
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals %d\n", rtab ? 4 : 3);
        if (rtab) {
            emit_textf("    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    aload_0\n    invokevirtual bb/bb_rtab/val()I\n    isub\n    istore_1\n");
            emit_textf("    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iload_1\n    if_icmpgt %s_omega\n", tag);
            emit_textf("    iload_1\n    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    isub\n    istore_2\n");
            emit_textf("    aload_0\n    iload_2\n    putfield bb/bb_rtab/advance I\n");
            emit_textf("    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_3\n");
            emit_textf("    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    iload_1\n    putfield bb/bb_box$MatchState/delta I\n");
            emit_textf("    new bb/bb_box$Spec\n    dup\n    iload_3\n    iload_2\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
            emit_textf("%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
            emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
            emit_textf("    aload_0\n    getfield bb/bb_rtab/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_rtab/advance I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
            emit_textf("    aconst_null\n    areturn\n.end method\n");
        } else {
            emit_textf("    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_tab/val()I\n    if_icmpgt %s_omega\n", tag);
            emit_textf("    aload_0\n    invokevirtual bb/bb_tab/val()I\n    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    isub\n    istore_1\n");
            emit_textf("    aload_0\n    iload_1\n    putfield bb/bb_tab/advance I\n");
            emit_textf("    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_2\n");
            emit_textf("    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    aload_0\n    invokevirtual bb/bb_tab/val()I\n    putfield bb/bb_box$MatchState/delta I\n");
            emit_textf("    new bb/bb_box$Spec\n    dup\n    iload_2\n    iload_1\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
            emit_textf("%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
            emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
            emit_textf("    aload_0\n    getfield bb/bb_tab/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_tab/advance I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
            emit_textf("    aconst_null\n    areturn\n.end method\n");
        }
        return;
    }
    if (IS_JS) {
        int64_t n = nd->ival;
        emit_textf("function make_pat_%d_%d(ms) { const n = %ld; let delta = 0; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        if (rtab)
            emit_textf("alpha() { const tgt = ms.omega - n; if (ms.delta > tgt) { self.fail.alpha(); return; } delta = tgt - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
        else
            emit_textf("alpha() { if (ms.delta > n || ms.delta > ms.omega) { self.fail.alpha(); return; } delta = n - ms.delta; if (ms.delta + delta > ms.omega) delta = ms.omega - ms.delta; const r = ms.sigma.slice(ms.delta, ms.delta + delta); ms.delta += delta; self.succ.alpha(); return r; },\n");
        emit_textf("beta() { ms.delta -= delta; self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        int n = (int)nd->ival; const char * lbl = rtab ? "RTAB" : "TAB";
        net_class_hdr(out, sid, nid);
        emit_textf("  .field private int32 _n\n  .field private int32 _advance\n");
        emit_textf("  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
        emit_textf("    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        emit_textf("    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        if (rtab) {
            emit_textf("    .maxstack 4\n    .locals init (int32 V_target, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
            emit_textf("    ldarg.1\n"); net_ms_length(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n    stloc.0\n", sid, nid);
            net_cursor_load(out); emit_textf("    ldloc.0\n    bgt        %s_%d_%d_FAIL\n", lbl, sid, nid);
            emit_textf("    ldarg.0\n    ldloc.0\n"); net_cursor_load(out); emit_textf("    sub\n    stfld      int32 pat_%d_%d::_advance\n", sid, nid);
            net_cursor_load(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n", sid, nid);
            net_spec_of(out); emit_textf("    stloc.1\n    ldarg.1\n    ldloc.0\n");
            emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.1\n    ret\n");
        } else {
            emit_textf("    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
            net_cursor_load(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    bgt        %s_%d_%d_FAIL\n", sid, nid, lbl, sid, nid);
            emit_textf("    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid); net_cursor_load(out);
            emit_textf("    sub\n    stfld      int32 pat_%d_%d::_advance\n", sid, nid);
            net_cursor_load(out); emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n", sid, nid);
            net_spec_of(out); emit_textf("    stloc.0\n    ldarg.1\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid);
            emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n");
        }
        emit_textf("  %s_%d_%d_FAIL:\n", lbl, sid, nid); net_fail_ret(out); emit_textf("  }\n");
        net_beta_hdr(out);
        emit_textf("    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        emit_textf("    ldarg.0\n    ldfld      int32 pat_%d_%d::_advance\n    sub\n", sid, nid);
        emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n"); net_fail_ret(out); emit_textf("  }\n}\n");
        net_push_i4(out, n); emit_textf("    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_rem(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0; (void)sid;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        jvm_class_hdr(out, "rem"); jvm_init_ms_only(out, "rem");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 2\n");
        emit_textf("    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        emit_textf("    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/omega I\n    putfield bb/bb_box$MatchState/delta I\n");
        emit_textf("    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    getfield bb/bb_rem/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iload_1\n    isub\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        emit_textf("alpha() { const r = ms.sigma.slice(ms.delta, ms.omega); ms.delta = ms.omega; self.succ.alpha(); return r; },\n");
        emit_textf("beta() { self.fail.alpha(); }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        net_class_hdr(out, sid, nid); net_ctor_none(out, sid, nid);
        net_alpha_hdr(out);
        emit_textf("    .maxstack 3\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        net_cursor_load(out); emit_textf("    ldarg.1\n"); net_ms_length(out);
        net_spec_of(out); emit_textf("    stloc.0\n    ldarg.1\n    ldarg.1\n"); net_ms_length(out);
        emit_textf("    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n  }\n");
        net_beta_hdr(out); emit_textf("    .maxstack 1\n"); net_fail_ret(out); emit_textf("  }\n}\n");
        emit_textf("    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_fence(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_JVM) {
        jvm_class_hdr(out, "fence"); jvm_init_ms_only(out, "fence");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        emit_textf("    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_fence/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iconst_0\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 1\n    .limit locals 1\n    aconst_null\n    areturn\n.end method\n");
        (void)sid; (void)nid; return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        emit_textf("alpha() { self.succ.alpha(); return ''; },\nbeta() { self.fail.alpha(); return null; }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        net_class_hdr(out, sid, nid); net_ctor_none(out, sid, nid);
        net_alpha_hdr(out); emit_textf("    .maxstack 1\n"); net_cursor_load(out); net_spec_zw(out); emit_textf("    ret\n  }\n");
        net_beta_hdr(out); emit_textf("    .maxstack 1\n"); net_fail_ret(out); emit_textf("  }\n}\n");
        emit_textf("    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
        return;
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_abort(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_JVM) {
        jvm_class_hdr(out, "abort");
        emit_textf(".inner class public static final abort_exception inner bb/bb_abort$AbortException outer bb/bb_abort\n");
        jvm_init_ms_only(out, "abort");
        emit_textf(".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 2\n    .limit locals 1\n    new bb/bb_abort$AbortException\n    dup\n    invokespecial bb/bb_abort$AbortException/<init>()V\n    athrow\n.end method\n");
        emit_textf(".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 2\n    .limit locals 1\n    new bb/bb_abort$AbortException\n    dup\n    invokespecial bb/bb_abort$AbortException/<init>()V\n    athrow\n.end method\n");
        (void)sid; (void)nid; return;
    }
    if (IS_JS) {
        emit_textf("function make_pat_%d_%d(ms) { let self = { succ: null, fail: null,\n", nd->ival, nid);
        emit_textf("alpha() { self.fail.alpha(); return null; },\nbeta() { self.fail.alpha(); return null; }\n}; return self; }\n");
        return;
    }
    if (IS_NET) {
        net_class_hdr(out, sid, nid); net_ctor_none(out, sid, nid);
        net_alpha_hdr(out); emit_textf("    .maxstack 1\n"); net_fail_ret(out); emit_textf("  }\n");
        net_beta_hdr(out); emit_textf("    .maxstack 1\n"); net_fail_ret(out); emit_textf("  }\n}\n");
        emit_textf("    newobj     instance void pat_%d_%d::.ctor()\n", sid, nid);
        return;
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
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
