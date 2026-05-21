/* bb_len.c — BB template for LEN(n) fixed-length match.
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Function body is byte-identical to the consolidated bb_pat.c original
   that this file restores; separators and includes match the original per-file
   shape pre-EC-UNI-13(a). */
#include "bb_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_len(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_X86) {
        /* Lifted from emit_bb.c::emit_bb_xlnth.  Snocone discipline: read values (n,
           label-name strings) from g_emit, write assembly strings via bb3c_format.
           No pointer-laundering helpers (emit_jmp / emit_label_define / emit_seq_*). */
        int n = (int)nd->ival;
        const char * lbl_succ = g_emit.lbl_succ;
        const char * lbl_fail = g_emit.lbl_fail;
        const char * lbl_back = g_emit.lbl_back;
        char nbuf[32]; snprintf(nbuf, sizeof nbuf, "%d", n);
        emit_bb_box_banner("LEN", nbuf);
        FILE * o = emit_outf();
        bb3c_format(o, "", ".intel_syntax", "noprefix");
        bb3c_format(o, "", "lea", "rax, [rip + Δ]");
        bb3c_format(o, "", "mov", "eax, dword ptr [rax]");
        char addarg[64]; snprintf(addarg, sizeof addarg, "eax, %d", n);
        bb3c_format(o, "", "add", addarg);
        bb3c_format(o, "", "lea", "rcx, [rip + Σlen]");
        bb3c_format(o, "", "cmp", "eax, dword ptr [rcx]");
        emit_text_jmp(lbl_fail, JMP_JG);
        bb3c_format(o, "", "lea", "rax, [rip + Δ]");
        bb3c_format(o, "", "mov", "ecx, dword ptr [rax]");
        char addarg2[64]; snprintf(addarg2, sizeof addarg2, "ecx, %d", n);
        bb3c_format(o, "", "add", addarg2);
        bb3c_format(o, "", "mov", "dword ptr [rax], ecx");
        emit_text_jmp(lbl_succ, JMP_JMP);
        emit_text_label(lbl_back);
        emit_text_jmp(lbl_fail, JMP_JMP);
        return;
    }
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
