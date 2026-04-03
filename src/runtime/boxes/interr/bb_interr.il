// bb_interr.il — INTERR: ?X — zero-width if child succeeds, ω if child fails
.assembly extern mscorlib {}
.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbInterr
       extends [mscorlib]System.Object implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private class Snobol4.Runtime.Boxes.IByrdBox _child
  .method public specialname rtspecialname instance void
          .ctor(class Snobol4.Runtime.Boxes.IByrdBox child) cil managed
  {
    .maxstack 2
    ldarg.0
    call instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    stfld class Snobol4.Runtime.Boxes.IByrdBox Snobol4.Runtime.Boxes.BbInterr::_child
    ret
  }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 3
    .locals init (int32 V_saved, valuetype Snobol4.Runtime.Boxes.Spec V_cr)
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    stloc.0
    ldarg.0
    ldfld      class Snobol4.Runtime.Boxes.IByrdBox Snobol4.Runtime.Boxes.BbInterr::_child
    ldarg.1
    callvirt   instance valuetype Snobol4.Runtime.Boxes.Spec
               Snobol4.Runtime.Boxes.IByrdBox::α(class Snobol4.Runtime.Boxes.MatchState)
    stloc.1
    // always restore cursor
    ldarg.1
    ldloc.0
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloca.s   V_cr
    call       instance bool Snobol4.Runtime.Boxes.Spec::get_IsFail()
    brtrue     INT_A_FAIL   // child_ω → INT_ω
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::ZeroWidth(int32)
    ret
  INT_A_FAIL:
    ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  { .maxstack 1 ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail ret }
}
