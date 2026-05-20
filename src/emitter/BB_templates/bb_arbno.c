#include "bb_template_common.h"

void bb_arbno(IR_t * nd, FILE * out) {
    int nid = ir_node_id(nd); int sid = 0;
    if (IS_BIN) return; /* x86 binary: emit_flat_body path, not emit_bb_node */
    if (IS_JVM) {
        char tag[32]; snprintf(tag, sizeof tag, "arbno_%d_%d", sid, nid);
        jvm_class_hdr(out, "arbno");
        fprintf(out, ".field private static final MAX_DEPTH I = 64\n");
        fprintf(out, ".field private final body Lbb/bb_box;\n");
        fprintf(out, ".field private final frame_start [I\n.field private final frame_match_st [I\n.field private final frame_match_ln [I\n");
        fprintf(out, ".field private depth I\n");
        fprintf(out, ".method public <init>(Lbb/bb_box$MatchState;Lbb/bb_box;)V\n    .limit stack 4\n    .limit locals 3\n");
        fprintf(out, "    aload_0\n    aload_1\n    invokespecial bb/bb_box/<init>(Lbb/bb_box$MatchState;)V\n");
        fprintf(out, "    aload_0\n    aload_2\n    putfield bb/bb_arbno/body Lbb/bb_box;\n");
        fprintf(out, "    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_start [I\n");
        fprintf(out, "    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_match_st [I\n");
        fprintf(out, "    aload_0\n    bipush 64\n    newarray int\n    putfield bb/bb_arbno/frame_match_ln [I\n");
        fprintf(out, "    return\n.end method\n");
        fprintf(out, ".method public \316\261()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    iconst_0\n    putfield bb/bb_arbno/depth I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    iconst_0\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    iconst_0\n    iconst_0\n    iastore\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    iconst_0\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
        fprintf(out, "    aload_0\n    invokevirtual bb/bb_arbno/tryBody()Lbb/bb_box$Spec;\n    areturn\n.end method\n");
        fprintf(out, ".method public \316\262()Lbb/bb_box$Spec;\n    .limit stack 5\n    .limit locals 1\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/depth I\n    ifle %s_beta_omega\n", tag);
        fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    isub\n    putfield bb/bb_arbno/depth I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    putfield bb/bb_box$MatchState/delta I\n");
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_beta_omega:\n    aconst_null\n    areturn\n.end method\n", tag);
        fprintf(out, ".method private tryBody()Lbb/bb_box$Spec;\n    .limit stack 6\n    .limit locals 4\n%s_tryBody_loop:\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/body Lbb/bb_box;\n    invokevirtual bb/bb_box/\316\261()Lbb/bb_box$Spec;\n    astore_1\n    aload_1\n    ifnull %s_tryBody_omega\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    if_icmpne %s_tryBody_advance\n", tag);
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_tryBody_advance:\n", tag);
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    istore_2\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_1\n    getfield bb/bb_box$Spec/len I\n    iadd\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    iadd\n    bipush 64\n    if_icmpge %s_tryBody_full\n", tag);
        fprintf(out, "    aload_0\n    dup\n    getfield bb/bb_arbno/depth I\n    iconst_1\n    iadd\n    putfield bb/bb_arbno/depth I\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iload_2\n    iastore\n");
        fprintf(out, "    istore 3\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iload 3\n    iastore\n");
        fprintf(out, "    aload_0\n    getfield bb/bb_arbno/frame_start [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    aload_0\n    getfield bb/bb_arbno/ms Lbb/bb_box$MatchState;\n    getfield bb/bb_box$MatchState/delta I\n    iastore\n");
        fprintf(out, "    goto %s_tryBody_loop\n%s_tryBody_full:\n", tag, tag);
        fprintf(out, "    new bb/bb_box$Spec\n    dup_x1\n    swap\n    iload_2\n    swap\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n");
        fprintf(out, "%s_tryBody_omega:\n", tag);
        fprintf(out, "    new bb/bb_box$Spec\n    dup\n    aload_0\n    getfield bb/bb_arbno/frame_match_st [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    aload_0\n    getfield bb/bb_arbno/frame_match_ln [I\n    aload_0\n    getfield bb/bb_arbno/depth I\n    iaload\n    invokespecial bb/bb_box$Spec/<init>(II)V\n    areturn\n.end method\n");
        return;
    }
    if (IS_JS) {
        fprintf(out, "function make_pat_%d_%d(ms) { const stack = []; let self = { succ: null, fail: null,\n", nd->ival, nid);
        fprintf(out, "alpha() { stack.length = 0; stack.push({ start: ms.delta }); while (true) { const frame = stack[stack.length - 1]; const br = self.body.alpha();\n");
        fprintf(out, "if (br === null) { return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
        fprintf(out, "if (ms.delta === frame.start) { return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
        fprintf(out, "stack.push({ start: ms.delta }); } },\n");
        fprintf(out, "beta() { if (stack.length <= 1) { self.fail.alpha(); return; } stack.pop(); const frame = stack[stack.length - 1]; ms.delta = frame.start; return ms.sigma.slice(stack[0].start, ms.delta - stack[0].start); }\n");
        fprintf(out, "}; return self; }\n");
        return;
    }
    if (IS_NET) {
        net_class_hdr(out, sid, nid);
        fprintf(out, "  .field private class [boxes]Snobol4.Runtime.Boxes.IByrdBox _body\n");
        fprintf(out, "  .field private int32[] _matchStart\n  .field private int32[] _matchLen\n");
        fprintf(out, "  .field private int32[] _startStack\n  .field private int32   _depth\n");
        fprintf(out, "  .method public specialname rtspecialname instance void .ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox body) cil managed\n  {\n");
        fprintf(out, "    .maxstack 2\n    ldarg.0\n    call       instance void [mscorlib]System.Object::.ctor()\n");
        fprintf(out, "    ldarg.0\n    ldarg.1\n    stfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_body\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldc.i4     64\n    newarr     [mscorlib]System.Int32\n    stfld      int32[] pat_%d_%d::_startStack\n    ret\n  }\n", sid, nid);
        net_alpha_hdr(out);
        fprintf(out, "    .maxstack 4\n    .locals init (int32 V_startHere, valuetype [boxes]Snobol4.Runtime.Boxes.Spec V_br)\n");
        fprintf(out, "    ldarg.0\n    ldc.i4.0\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldc.i4.0\n", sid, nid); net_cursor_load(out); fprintf(out, "    stelem.i4\n");
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n    ldc.i4.0\n    ldc.i4.0\n    stelem.i4\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n    ldc.i4.0\n", sid, nid); net_cursor_load(out); fprintf(out, "    stelem.i4\n");
        fprintf(out, "  ARBNO_%d_%d_LOOP:\n", sid, nid); net_cursor_load(out); fprintf(out, "    stloc.0\n");
        fprintf(out, "    ldarg.0\n    ldfld      class [boxes]Snobol4.Runtime.Boxes.IByrdBox pat_%d_%d::_body\n    ldarg.1\n", sid, nid);
        fprintf(out, "    callvirt   instance valuetype [boxes]Snobol4.Runtime.Boxes.Spec [boxes]Snobol4.Runtime.Boxes.IByrdBox::Alpha(class [boxes]Snobol4.Runtime.Boxes.MatchState)\n");
        fprintf(out, "    stloc.1\n    ldloca.s   V_br\n    call       instance bool [boxes]Snobol4.Runtime.Boxes.Spec::get_IsFail()\n");
        fprintf(out, "    brtrue     ARBNO_%d_%d_STOP\n", sid, nid); net_cursor_load(out); fprintf(out, "    ldloc.0\n    beq        ARBNO_%d_%d_STOP\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4     63\n    bge        ARBNO_%d_%d_STOP\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    add\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n    ldc.i4.0\n    ldelem.i4\n    stelem.i4\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    sub\n    ldelem.i4\n", sid, nid);
        fprintf(out, "    ldloca.s   V_br\n    ldfld      int32 [boxes]Snobol4.Runtime.Boxes.Spec::Length\n    add\n    stelem.i4\n");
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        net_cursor_load(out); fprintf(out, "    stelem.i4\n    br         ARBNO_%d_%d_LOOP\n", sid, nid);
        fprintf(out, "  ARBNO_%d_%d_STOP:\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        net_spec_of(out); fprintf(out, "    ret\n  }\n");
        net_beta_hdr(out);
        fprintf(out, "    .maxstack 3\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.0\n    ble        ARBNO_%d_%d_BFAIL\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.0\n    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldc.i4.1\n    sub\n    stfld      int32 pat_%d_%d::_depth\n", sid, nid, sid, nid);
        fprintf(out, "    ldarg.1\n    ldarg.0\n    ldfld      int32[] pat_%d_%d::_startStack\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        fprintf(out, "    stfld      int32 [boxes]Snobol4.Runtime.Boxes.MatchState::Cursor\n");
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchStart\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32[] pat_%d_%d::_matchLen\n", sid, nid);
        fprintf(out, "    ldarg.0\n    ldfld      int32 pat_%d_%d::_depth\n    ldelem.i4\n", sid, nid);
        net_spec_of(out); fprintf(out, "    ret\n  ARBNO_%d_%d_BFAIL:\n", sid, nid); net_fail_ret(out); fprintf(out, "  }\n}\n");
        fprintf(out, "    newobj     instance void pat_%d_%d::.ctor(class [boxes]Snobol4.Runtime.Boxes.IByrdBox)\n", sid, nid);
    }
    /* IS_WASM: n/a — BB WASM never landed in original code */
}
