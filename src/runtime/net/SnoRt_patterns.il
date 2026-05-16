// Pattern box implementations inlined into SnoRt
// Minimal subset for SN4-NET-5d gate: bb_lit, bb_seq, bb_fail, MatchState, ByrdBoxExecutor

.namespace Snobol4.Runtime.Boxes
{
  // IByrdBox interface — minimal version
  .class public interface abstract auto ansi IByrdBox_minimal
  {
    .method public virtual abstract instance valuetype Spec Alpha(class MatchState ms) cil managed {}
    .method public virtual abstract instance valuetype Spec Beta(class MatchState ms) cil managed {}
  }

  // Spec valuetype — match result
  .class public sequential ansi sealed beforefieldinit Spec extends [mscorlib]System.ValueType
  {
    .field public int32 Start
    .field public int32 Length
    .field public static initonly valuetype Spec Fail
    
    .method static void .cctor() cil managed
    {
      .maxstack 2
      ldsflda valuetype Spec Spec::Fail
      ldc.i4.m1
      stfld int32 Spec::Start
      ret
    }
    
    .method public instance bool get_IsFail() cil managed
    {
      .maxstack 2
      ldarg.0
      ldfld int32 Spec::Start
      ldc.i4.0
      clt
      ret
    }
    
    .property instance bool IsFail() { .get instance bool Spec::get_IsFail() }
    
    .method public static valuetype Spec Of(int32 start, int32 length) cil managed
    {
      .maxstack 2
      .locals init (valuetype Spec V_0)
      ldloca.s V_0
      ldarg.0
      stfld int32 Spec::Start
      ldloca.s V_0
      ldarg.1
      stfld int32 Spec::Length
      ldloc.0
      ret
    }
  }

  // MatchState class — subject + cursor
  .class public auto ansi beforefieldinit MatchState extends [mscorlib]System.Object
  {
    .field private string _subject
    .field public int32 Cursor
    
    .method public specialname rtspecialname instance void .ctor(string subject) cil managed
    {
      .maxstack 2
      ldarg.0
      call instance void [mscorlib]System.Object::.ctor()
      ldarg.0
      ldarg.1
      dup
      brtrue MS_NN
      pop
      ldstr ""
    MS_NN:
      stfld string MatchState::_subject
      ldarg.0
      ldc.i4.0
      stfld int32 MatchState::Cursor
      ret
    }
    
    .method public instance string get_Subject() cil managed
    {
      .maxstack 1
      ldarg.0
      ldfld string MatchState::_subject
      ret
    }
    
    .property instance string Subject() { .get instance string MatchState::get_Subject() }
    
    .method public instance int32 get_Length() cil managed
    {
      .maxstack 1
      ldarg.0
      ldfld string MatchState::_subject
      callvirt instance int32 [mscorlib]System.String::get_Length()
      ret
    }
    
    .property instance int32 Length() { .get instance int32 MatchState::get_Length() }
  }

  // bb_lit class — match literal string
  .class public auto ansi beforefieldinit bb_lit extends [mscorlib]System.Object implements IByrdBox_minimal
  {
    .field private string _lit
    .field private int32 _len
    
    .method public specialname rtspecialname instance void .ctor(string lit) cil managed
    {
      .maxstack 2
      ldarg.0
      call instance void [mscorlib]System.Object::.ctor()
      ldarg.0
      ldarg.1
      dup
      brtrue LIT_NN
      pop
      ldstr ""
    LIT_NN:
      dup
      stfld string bb_lit::_lit
      callvirt instance int32 [mscorlib]System.String::get_Length()
      stfld int32 bb_lit::_len
      ret
    }
    
    .method public virtual instance valuetype Spec Alpha(class MatchState ms) cil managed
    {
      .maxstack 4
      .locals init (int32 V_0)
      ldarg.1
      ldfld int32 MatchState::Cursor
      ldarg.0
      ldfld int32 bb_lit::_len
      add
      ldarg.1
      callvirt instance int32 MatchState::get_Length()
      bgt LIT_A_FAIL
      ldarg.0
      ldfld string bb_lit::_lit
      ldarg.1
      ldfld string MatchState::_subject
      ldarg.1
      ldfld int32 MatchState::Cursor
      ldarg.0
      ldfld int32 bb_lit::_len
      callvirt instance int32 [mscorlib]System.String::Substring(int32, int32)
      callvirt instance bool [mscorlib]System.String::Equals(string)
      brfalse LIT_A_FAIL
      ldarg.1
      ldarg.1
      ldfld int32 MatchState::Cursor
      ldarg.0
      ldfld int32 bb_lit::_len
      add
      stfld int32 MatchState::Cursor
      ldarg.1
      ldfld int32 MatchState::Cursor
      ldarg.0
      ldfld int32 bb_lit::_len
      sub
      ldarg.0
      ldfld int32 bb_lit::_len
      call valuetype Spec Spec::Of(int32, int32)
      ret
    LIT_A_FAIL:
      ldsfld valuetype Spec Spec::Fail
      ret
    }
    
    .method public virtual instance valuetype Spec Beta(class MatchState ms) cil managed
    {
      .maxstack 2
      ldarg.1
      ldarg.1
      ldfld int32 MatchState::Cursor
      ldarg.0
      ldfld int32 bb_lit::_len
      sub
      stfld int32 MatchState::Cursor
      ldsfld valuetype Spec Spec::Fail
      ret
    }
  }
}
