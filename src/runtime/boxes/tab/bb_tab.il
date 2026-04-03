// bb_tab.il — TAB: advance TO absolute pos n; RTAB: advance TO Length-n
.assembly extern mscorlib {}
.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbTab
       extends [mscorlib]System.Object implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private int32 _n
  .field private int32 _advance
  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed
  {
    .maxstack 2
    ldarg.0
    call instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    stfld int32 Snobol4.Runtime.Boxes.BbTab::_n
    ret
  }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 3
    .locals init (valuetype Snobol4.Runtime.Boxes.Spec V_r)
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbTab::_n
    bgt        TAB_A_FAIL
    ldarg.0
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbTab::_n
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    sub
    stfld      int32 Snobol4.Runtime.Boxes.BbTab::_advance
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbTab::_advance
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    stloc.0
    ldarg.1
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbTab::_n
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc.0
    ret
  TAB_A_FAIL:
    ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
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
    ldfld      int32 Snobol4.Runtime.Boxes.BbTab::_advance
    sub
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
}

.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbRtab
       extends [mscorlib]System.Object implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private int32 _n
  .field private int32 _advance
  .method public specialname rtspecialname instance void .ctor(int32 n) cil managed
  {
    .maxstack 2
    ldarg.0
    call instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    stfld int32 Snobol4.Runtime.Boxes.BbRtab::_n
    ret
  }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 4
    .locals init (int32 V_target, valuetype Snobol4.Runtime.Boxes.Spec V_r)
    ldarg.1
    callvirt   instance int32 Snobol4.Runtime.Boxes.MatchState::get_Length()
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbRtab::_n
    sub
    stloc.0
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc.0
    bgt        RTAB_A_FAIL
    ldarg.0
    ldloc.0
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    sub
    stfld      int32 Snobol4.Runtime.Boxes.BbRtab::_advance
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbRtab::_advance
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    stloc.1
    ldarg.1
    ldloc.0
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc.1
    ret
  RTAB_A_FAIL:
    ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
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
    ldfld      int32 Snobol4.Runtime.Boxes.BbRtab::_advance
    sub
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
}
