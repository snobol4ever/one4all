// bb_span.il — SPAN: longest prefix of chars in set (≥1 required)
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6
.assembly extern mscorlib {}
.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbSpan
       extends [mscorlib]System.Object implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private string _chars
  .field private int32  _count
  .method public specialname rtspecialname instance void .ctor(string chars) cil managed
  {
    .maxstack 2
    ldarg.0
    call       instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    dup
    brtrue     SP_NN
    pop
    ldstr      ""
  SP_NN:
    stfld      string Snobol4.Runtime.Boxes.BbSpan::_chars
    ret
  }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 3
    .locals init (valuetype Snobol4.Runtime.Boxes.Spec V_r)
    ldarg.0
    ldc.i4.0
    stfld      int32 Snobol4.Runtime.Boxes.BbSpan::_count
  SP_A_LOOP:
    ldarg.1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbSpan::_count
    add
    ldarg.0
    ldfld      string Snobol4.Runtime.Boxes.BbSpan::_chars
    callvirt   instance bool Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)
    brfalse    SP_A_DONE
    ldarg.0
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbSpan::_count
    ldc.i4.1
    add
    stfld      int32 Snobol4.Runtime.Boxes.BbSpan::_count
    br         SP_A_LOOP
  SP_A_DONE:
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbSpan::_count
    ldc.i4.0
    ble        SP_A_FAIL
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbSpan::_count
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    stloc.0
    ldarg.1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbSpan::_count
    add
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc.0
    ret
  SP_A_FAIL:
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 3
    ldarg.1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbSpan::_count
    sub
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
}
