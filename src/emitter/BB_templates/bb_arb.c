/* bb_arb.c — BB template for ARB arbitrary-length match.
   One file per Byrd Box per RULES.md (BB_templates folder rule).
   Function body is byte-identical to the consolidated bb_pat.c original
   that this file restores; separators and includes match the original per-file
   shape pre-EC-UNI-13(a). */
#include "bb_template_common.h"
extern int g_flat_node_id;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_arb(void) {
    BB_t * nd = g_emit.node; FILE * out = g_emit.out;
    int nid = bb_node_id(nd); int sid = 0;
    if (IS_X86) {
        /* Lifted from emit_bb.c::emit_bb_xfarb.  Snocone discipline: read values
           (label-name strings, integer id) from g_emit / scalars; write assembly
           text via bb3c_format directly. */
        const char * lbl_succ = g_emit.lbl_succ;
        const char * lbl_fail = g_emit.lbl_fail;
        const char * lbl_back = g_emit.lbl_back;
        emit_bb_box_banner("ARB", "");
        if (IS_TEXT) {
            int id = g_flat_node_id++;
            char zlbl[80], zlbl_def[88];
            snprintf(zlbl,     sizeof zlbl,     ".Larb%d_z", id);
            snprintf(zlbl_def, sizeof zlbl_def, "%s:", zlbl);
            FILE * o = emit_outf();
            bb3c_format(o, "",       ".section", ".data");
            bb3c_format(o, zlbl_def, ".long",    "0");
            bb3c_format(o, "",       ".long",    "0");
            bb3c_format(o, "",       ".section", ".text");
            bb3c_format(o, "",       ".intel_syntax", "noprefix");
            char lcnt[80], lstart[80];
            snprintf(lcnt,   sizeof lcnt,   "%s + 0", zlbl);
            snprintf(lstart, sizeof lstart, "%s + 4", zlbl);
            bb3c_format(o, "", "lea",  "rax, [rip + Δ]");
            bb3c_format(o, "", "mov",  "ecx, dword ptr [rax]");
            char cnt_store[160]; snprintf(cnt_store, sizeof cnt_store, "dword ptr [rip + %s], 0", lcnt);
            bb3c_format(o, "", "mov",  cnt_store);
            char start_store[160]; snprintf(start_store, sizeof start_store, "rax, [rip + %s]", lstart);
            bb3c_format(o, "", "lea",  start_store);
            bb3c_format(o, "", "mov",  "dword ptr [rax], ecx");
            bb3c_format(o, "", "jmp",  lbl_succ);
            char back_def[BB_LABEL_NAME_MAX + 4]; snprintf(back_def, sizeof back_def, "%s:", lbl_back);
            bb3c_format(o, back_def, "", "");
            char cnt_ref[160]; snprintf(cnt_ref, sizeof cnt_ref, "rax, [rip + %s]", lcnt);
            bb3c_format(o, "", "lea",  cnt_ref);
            bb3c_format(o, "", "mov",  "ecx, dword ptr [rax]");
            bb3c_format(o, "", "inc",  "ecx");
            bb3c_format(o, "", "mov",  "dword ptr [rax], ecx");
            char sref[160]; snprintf(sref, sizeof sref, "rax, [rip + %s]", lstart);
            bb3c_format(o, "", "lea",  sref);
            bb3c_format(o, "", "mov",  "edx, dword ptr [rax]");
            bb3c_format(o, "", "add",  "edx, ecx");
            bb3c_format(o, "", "lea",  "rax, [rip + Σlen]");
            bb3c_format(o, "", "cmp",  "edx, dword ptr [rax]");
            bb3c_format(o, "", "jg",   lbl_fail);
            bb3c_format(o, "", "lea",  "rax, [rip + Δ]");
            bb3c_format(o, "", "mov",  "dword ptr [rax], edx");
            bb3c_format(o, "", "jmp",  lbl_succ);
        }
        return;
    }
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
