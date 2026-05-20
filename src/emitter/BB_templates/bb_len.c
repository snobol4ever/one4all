#include "bb_template_common.h"

void bb_len(BB_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "len_%d_%d", sid, nid);
        jvm_class_hdr(out, "len");
        fprintf(out, ".field private final n I\n.field private final dyn Ljava/util/function/IntSupplier;\n");
        jvm_init_ms_int(out, "len", "n"); jvm_val_helper(out, "len");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    iadd\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/omega I\n    if_icmpgt %s_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    istore_1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    iadd\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    iload_1\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 4\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_len/ms Lbb/bb_box$MatchState;\n    dup\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    invokevirtual bb/bb_len/val()I\n    isub\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    aconst_null\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        int64_t n = nd->ival;
        fprintf(out, "function make_pat_%d_%d(ms) { const n = %ld; let self = { succ: null, fail: null,\n", nd->ival, nid, n);
        fprintf(out, "alpha() { if (ms.delta + n > ms.omega) { self.fail.alpha(); return; } const r = ms.sigma.slice(ms.delta, ms.delta + n); ms.delta += n; self.succ.alpha(); return r; },\n");
        fprintf(out, "beta() { ms.delta -= n; self.fail.alpha(); }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    if (IS_NET) {
        int n = (int)nd->ival;
        net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private int32 _n\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      int32 pat_%d_%d::_n\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        fprintf(out, "    .maxstack 4\n    .locals init (valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_r)\n");
        net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    add\n", sid, nid);
        fprintf(out, "    ldarg.1\n"); net_ms_length(out); fprintf(out, "    bgt        LEN_%d_%d_FAIL\n", sid, nid);
        net_cursor_load(out); fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n", sid, nid);
        net_spec_of(out); fprintf(out, "    stloc.0\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    add\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n    ldloc.0\n    ret\n");
        fprintf(out, "  LEN_%d_%d_FAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n");
        net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n    ldarg.1\n    ldarg.1\n"); net_cursor_load(out);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_n\n    sub\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n"); net_fail_ret(out); fprintf(out, "  }\n}\n");
        net_push_i4(out, n); fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(int32)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
