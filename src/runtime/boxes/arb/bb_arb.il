// bb_arb.il — ARB: match 0..N chars lazily; β tries one more
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6

.assembly extern mscorlib {}

.class public auto ansi beforefieldinit
       Snobol4.Runtime.Boxes.BbArb
       extends [mscorlib]System.Object
       implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private int32 _count
  .field private int32 _start

  .method public specialname rtspecialname instance void .ctor() cil managed
  {
    .maxstack 1
    ldarg.0
    call instance void [mscorlib]System.Object::.ctor()
    ret
  }

  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 2
    ldarg.0
    ldc.i4.0
    stfld      int32 Snobol4.Runtime.Boxes.BbArb::_count
    ldarg.0
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    stfld      int32 Snobol4.Runtime.Boxes.BbArb::_start
    // zero-width success first (lazy)
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::ZeroWidth(int32)
    ret
  }

  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 3
    // _count++
    ldarg.0
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArb::_count
    ldc.i4.1
    add
    stfld      int32 Snobol4.Runtime.Boxes.BbArb::_count
    // if _start + _count > Length → fail
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArb::_start
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArb::_count
    add
    ldarg.1
    callvirt   instance int32 Snobol4.Runtime.Boxes.MatchState::get_Length()
    bgt        ARB_B_FAIL
    // cursor = start + count
    ldarg.1
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArb::_start
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArb::_count
    add
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArb::_start
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbArb::_count
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    ret
  ARB_B_FAIL:
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
}
