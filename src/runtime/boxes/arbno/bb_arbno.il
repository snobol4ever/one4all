// bb_arbno.il — ARBNO: zero-or-more greedy; β unwinds stack
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6

.assembly extern mscorlib {}

.class public auto ansi beforefieldinit
       Snobol4.Runtime.Boxes.BbArbno
       extends [mscorlib]System.Object
       implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private class Snobol4.Runtime.Boxes.IByrdBox _body
  // stacks as int arrays: [0]=start, [1]=len interleaved; depth index
  .field private int32[] _matchStart
  .field private int32[] _matchLen
  .field private int32[] _startStack
  .field private int32   _depth

  .method public specialname rtspecialname instance void
          .ctor(class Snobol4.Runtime.Boxes.IByrdBox body) cil managed
  {
    .maxstack 2
    ldarg.0
    call       instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    stfld      class Snobol4.Runtime.Boxes.IByrdBox Snobol4.Runtime.Boxes.BbArbno::_body
    ldarg.0
    ldc.i4     64
    newarr     [mscorlib]System.Int32
    stfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchStart
    ldarg.0
    ldc.i4     64
    newarr     [mscorlib]System.Int32
    stfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchLen
    ldarg.0
    ldc.i4     64
    newarr     [mscorlib]System.Int32
    stfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_startStack
    ret
  }

  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 4
    .locals init (int32 V_startHere, valuetype Snobol4.Runtime.Boxes.Spec V_br)

    // _depth = 0; push frame(ZeroWidth, cursor)
    ldarg.0
    ldc.i4.0
    stfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchStart
    ldc.i4.0
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    stelem.i4
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchLen
    ldc.i4.0
    ldc.i4.0
    stelem.i4
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_startStack
    ldc.i4.0
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    stelem.i4

  ARBNO_A_LOOP:
    // startHere = ms.Cursor
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    stloc.0
    // br = _body.α(ms)
    ldarg.0
    ldfld      class Snobol4.Runtime.Boxes.IByrdBox Snobol4.Runtime.Boxes.BbArbno::_body
    ldarg.1
    callvirt   instance valuetype Snobol4.Runtime.Boxes.Spec
               Snobol4.Runtime.Boxes.IByrdBox::α(class Snobol4.Runtime.Boxes.MatchState)
    stloc.1
    // if br.IsFail → stop, succeed with top frame
    ldloca.s   V_br
    call       instance bool Snobol4.Runtime.Boxes.Spec::get_IsFail()
    brtrue     ARBNO_A_STOP
    // zero-advance guard: if cursor == startHere → stop
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc.0
    beq        ARBNO_A_STOP
    // advance: depth++ push new frame (if < 64)
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldc.i4     63
    bge        ARBNO_A_STOP
    ldarg.0
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldc.i4.1
    add
    stfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    // _matchStart[depth] = _matchStart[depth-1] (cumulative start = always frame0 start)
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchStart
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchStart
    ldc.i4.0
    ldelem.i4
    stelem.i4
    // _matchLen[depth] = _matchLen[depth-1] + br.Length
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchLen
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchLen
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldc.i4.1
    sub
    ldelem.i4
    ldloca.s   V_br
    ldfld      int32 Snobol4.Runtime.Boxes.Spec::Length
    add
    stelem.i4
    // _startStack[depth] = cursor
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_startStack
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    stelem.i4
    br         ARBNO_A_LOOP

  ARBNO_A_STOP:
    // return Spec.Of(_matchStart[_depth], _matchLen[_depth])
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchStart
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldelem.i4
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchLen
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldelem.i4
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    ret
  }

  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 3
    // if _depth <= 0 → fail
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldc.i4.0
    ble        ARBNO_B_FAIL
    // _depth--
    ldarg.0
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldc.i4.1
    sub
    stfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    // ms.Cursor = _startStack[_depth]
    ldarg.1
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_startStack
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldelem.i4
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchStart
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldelem.i4
    ldarg.0
    ldfld      int32[] Snobol4.Runtime.Boxes.BbArbno::_matchLen
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArbno::_depth
    ldelem.i4
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    ret
  ARBNO_B_FAIL:
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
}
