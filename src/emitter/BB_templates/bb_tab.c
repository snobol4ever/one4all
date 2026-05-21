/* bb_tab.c — BB template for TAB(n) absolute cursor target.
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Function body is byte-identical to the consolidated bb_pat.c original
   that this file restores; separators and includes match the original per-file
   shape pre-EC-UNI-13(a). */
#include "bb_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_tab(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0; int rtab = (nd->ival2 != 0);
    if (IS_X86) {
        /* Lifted from emit_bb.c::emit_bb_xtb / emit_bb_xrtb.  Snocone discipline:
           read values from g_emit (n, label-name strings, rtab flag), write asm
           text via bb3c_format directly.  The rtab flag selects TAB vs RTAB. */
        int n = (int)nd->ival;
        const char * lbl_succ = g_emit.lbl_succ;
        const char * lbl_fail = g_emit.lbl_fail;
        const char * lbl_back = g_emit.lbl_back;
        char nbuf[32]; snprintf(nbuf, sizeof nbuf, "%d", n);
        emit_bb_box_banner(rtab ? "RTAB" : "TAB", nbuf);
        FILE * o = emit_outf();
        bb3c_format(o, "", ".intel_syntax", "noprefix");
        if (rtab) {
            bb3c_format(o, "", "lea", "rax, [rip + Σlen]");
            bb3c_format(o, "", "mov", "ecx, dword ptr [rax]");
            char subarg[64]; snprintf(subarg, sizeof subarg, "ecx, %d", n);
            bb3c_format(o, "", "sub", subarg);
            bb3c_format(o, "", "lea", "rax, [rip + Δ]");
            bb3c_format(o, "", "cmp", "ecx, dword ptr [rax]");
            emit_text_jmp(lbl_fail, JMP_JL);
            bb3c_format(o, "", "mov", "dword ptr [rax], ecx");
            emit_text_jmp(lbl_succ, JMP_JMP);
        } else {
            bb3c_format(o, "", "lea", "rax, [rip + Δ]");
            bb3c_format(o, "", "mov", "ecx, dword ptr [rax]");
            char cmparg[64]; snprintf(cmparg, sizeof cmparg, "ecx, %d", n);
            bb3c_format(o, "", "cmp", cmparg);
            emit_text_jmp(lbl_fail, JMP_JG);
            char movarg[64]; snprintf(movarg, sizeof movarg, "dword ptr [rax], %d", n);
            bb3c_format(o, "", "mov", movarg);
            emit_text_jmp(lbl_succ, JMP_JMP);
        }
        emit_text_label(lbl_back);
        emit_text_jmp(lbl_fail, JMP_JMP);
        return;
    }
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
