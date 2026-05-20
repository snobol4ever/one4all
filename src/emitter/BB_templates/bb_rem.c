#include "bb_template_common.h"

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
