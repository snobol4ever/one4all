// bb_brk.il — BREAK: scan to first char in set
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6
.assembly extern mscorlib {}
.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbBrk
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
    brtrue     BRK_NN
    pop
    ldstr      ""
  BRK_NN:
    stfld      string Snobol4.Runtime.Boxes.BbBrk::_chars
    ret
  }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 4
    .locals init (valuetype Snobol4.Runtime.Boxes.Spec V_r)
    ldarg.0
    ldc.i4.0
    stfld      int32 Snobol4.Runtime.Boxes.BbBrk::_count
  BRK_A_LOOP:
    // while cursor+count < Length && !CharInSet(cursor+count)
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbBrk::_count
    add
    ldarg.1
    callvirt   instance int32 Snobol4.Runtime.Boxes.MatchState::get_Length()
    bge        BRK_A_EOS
    ldarg.1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbBrk::_count
    add
    ldarg.0
    ldfld      string Snobol4.Runtime.Boxes.BbBrk::_chars
    callvirt   instance bool Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)
    brtrue     BRK_A_FOUND
    ldarg.0
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbBrk::_count
    ldc.i4.1
    add
    stfld      int32 Snobol4.Runtime.Boxes.BbBrk::_count
    br         BRK_A_LOOP
  BRK_A_EOS:
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  BRK_A_FOUND:
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbBrk::_count
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    stloc.0
    ldarg.1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbBrk::_count
    add
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc.0
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
    ldfld      int32 Snobol4.Runtime.Boxes.BbBrk::_count
    sub
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
}
