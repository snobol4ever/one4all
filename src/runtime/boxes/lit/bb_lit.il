// bb_lit.il — LIT: literal string match
// α: cursor+len>Ω→ω; subject[cursor..+len]!=lit→ω; cursor+=len; γ
// β: cursor-=len; ω
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6

.assembly extern mscorlib {}

.class public auto ansi beforefieldinit
       Snobol4.Runtime.Boxes.BbLit
       extends [mscorlib]System.Object
       implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private string _lit
  .field private int32  _len

  .method public specialname rtspecialname instance void .ctor(string lit) cil managed
  {
    .maxstack 2
    ldarg.0
    call       instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    dup
    brtrue     LIT_NOTNULL
    pop
    ldstr      ""
  LIT_NOTNULL:
    stfld      string Snobol4.Runtime.Boxes.BbLit::_lit
    ldarg.0
    ldarg.0
    ldfld      string Snobol4.Runtime.Boxes.BbLit::_lit
    callvirt   instance int32 [mscorlib]System.String::get_Length()
    stfld      int32 Snobol4.Runtime.Boxes.BbLit::_len
    ret
  }

  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 4
    .locals init (valuetype Snobol4.Runtime.Boxes.Spec V_result)
    // if cursor + len > Length → fail
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbLit::_len
    add
    ldarg.1
    callvirt   instance int32 Snobol4.Runtime.Boxes.MatchState::get_Length()
    bgt        LIT_A_FAIL
    // if !MatchesAt(cursor, lit) → fail
    ldarg.1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      string Snobol4.Runtime.Boxes.BbLit::_lit
    callvirt   instance bool Snobol4.Runtime.Boxes.MatchState::MatchesAt(int32, string)
    brfalse    LIT_A_FAIL
    // result = Spec.Of(cursor, len)
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbLit::_len
    call       valuetype Snobol4.Runtime.Boxes.Spec
               Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    stloc.0
    // cursor += len
    ldarg.1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbLit::_len
    add
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc.0
    ret
  LIT_A_FAIL:
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }

  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 3
    // cursor -= len
    ldarg.1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbLit::_len
    sub
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
}
