/* bb_break.c — BB template for BREAK(cset) up-to-cset match.
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Function body is byte-identical to the consolidated bb_pat.c original
   that this file restores; separators and includes match the original per-file
   shape pre-EC-UNI-13(a). */
#include "bb_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_break(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_X86) {
        const char *save_n1 = g_emit.op_name1, *save_n2 = g_emit.op_name2, *save_k = g_emit.op_kind;
        g_emit.op_name1 = nd->sval ? nd->sval : "";
        g_emit.op_name2 = "bb_brk";
        g_emit.op_kind  = "BREAK";
        bb_charset_emit();
        g_emit.op_name1 = save_n1; g_emit.op_name2 = save_n2; g_emit.op_kind = save_k;
        return;
    }
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
