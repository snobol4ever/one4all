// bb_len.il — LEN: match exactly n characters
.assembly extern mscorlib {}
.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbLen
       extends [mscorlib]System.Object implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private int32 _n
  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed
  {
    .maxstack 2
    ldarg.0
    call       instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    stfld      int32 Snobol4.Runtime.Boxes.BbLen::_n
    ret
  }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 4
    .locals init (valuetype Snobol4.Runtime.Boxes.Spec V_r)
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbLen::_n
    add
    ldarg.1
    callvirt   instance int32 Snobol4.Runtime.Boxes.MatchState::get_Length()
    bgt        LEN_A_FAIL
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbLen::_n
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    stloc.0
    ldarg.1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbLen::_n
    add
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc.0
    ret
  LEN_A_FAIL:
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
    ldfld      int32 Snobol4.Runtime.Boxes.BbLen::_n
    sub
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
}
