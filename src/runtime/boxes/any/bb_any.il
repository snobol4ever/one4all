// bb_any.il — ANY: match one char if in charset; NOTANY: if NOT in charset
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6

.assembly extern mscorlib {}

.class public auto ansi beforefieldinit
       Snobol4.Runtime.Boxes.BbAny
       extends [mscorlib]System.Object
       implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private string _chars

  .method public specialname rtspecialname instance void .ctor(string chars) cil managed
  {
    .maxstack 2
    ldarg.0
    call       instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    dup
    brtrue     ANY_NN
    pop
    ldstr      ""
  ANY_NN:
    stfld      string Snobol4.Runtime.Boxes.BbAny::_chars
    ret
  }

  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 3
    .locals init (valuetype Snobol4.Runtime.Boxes.Spec V_r)
    ldarg.1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      string Snobol4.Runtime.Boxes.BbAny::_chars
    callvirt   instance bool Snobol4.Runtime.Boxes.MatchState::CharInSet(int32, string)
    brfalse    ANY_A_FAIL
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldc.i4.1
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    stloc.0
    ldarg.1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldc.i4.1
    add
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc.0
    ret
  ANY_A_FAIL:
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
    ldc.i4.1
    sub
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
}
