/* bb_notany.c — BB template for NOTANY(cset) character match.
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Function body is byte-identical to the consolidated bb_pat.c original
   that this file restores; separators and includes match the original per-file
   shape pre-EC-UNI-13(a). */
#include "bb_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_notany(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_X86) {
        const char *save_n1 = g_emit.op_name1, *save_n2 = g_emit.op_name2, *save_k = g_emit.op_kind;
        g_emit.op_name1 = nd->sval ? nd->sval : "";
        g_emit.op_name2 = "bb_notany";
        g_emit.op_kind  = "NOTANY";
        bb_charset_emit();
        g_emit.op_name1 = save_n1; g_emit.op_name2 = save_n2; g_emit.op_kind = save_k;
        return;
    }
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
