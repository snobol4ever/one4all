// bb_box.il — Dynamic Byrd Box foundation for .NET MSIL
//
// Defines:
//   Spec        — valuetype (mirrors spec_t: start/length, Fail sentinel)
//   MatchState  — class (mirrors C globals Σ/Δ/Ω: subject string + cursor)
//   IByrdBox    — interface (α/β entry ports)
//
// Assemble as part of boxes.dll:
//   ilasm /dll /output:boxes.dll bb_box.il bb_executor.il bb_lit.il ...
//
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6
// SPRINT:  D-169

.assembly extern mscorlib {}

// ── Spec valuetype ──────────────────────────────────────────────────────────
// mirrors spec_t { int start; int length; }
// Spec.Fail has Start == -1

.class public sequential ansi sealed beforefieldinit
       Snobol4.Runtime.Boxes.Spec
       extends [mscorlib]System.ValueType
{
  .field public int32 Start
  .field public int32 Length

  // static readonly Spec Fail = new Spec(-1, 0)
  .field public static initonly valuetype Snobol4.Runtime.Boxes.Spec Fail

  .method static void .cctor() cil managed
  {
    .maxstack 3
    ldsflda    valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ldc.i4.m1
    stfld      int32 Snobol4.Runtime.Boxes.Spec::Start
    ldsflda    valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ldc.i4.0
    stfld      int32 Snobol4.Runtime.Boxes.Spec::Length
    ret
  }

  // bool get_IsFail() => Start < 0
  .method public instance bool get_IsFail() cil managed
  {
    .maxstack 2
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.Spec::Start
    ldc.i4.0
    clt
    ret
  }

  // static Spec Of(int start, int length)
  .method public static valuetype Snobol4.Runtime.Boxes.Spec Of(int32 start, int32 length) cil managed
  {
    .maxstack 2
    .locals init (valuetype Snobol4.Runtime.Boxes.Spec V_0)
    ldloca.s   V_0
    ldarg.0
    stfld      int32 Snobol4.Runtime.Boxes.Spec::Start
    ldloca.s   V_0
    ldarg.1
    stfld      int32 Snobol4.Runtime.Boxes.Spec::Length
    ldloc.0
    ret
  }

  // static Spec ZeroWidth(int pos)
  .method public static valuetype Snobol4.Runtime.Boxes.Spec ZeroWidth(int32 pos) cil managed
  {
    .maxstack 2
    ldarg.0
    ldc.i4.0
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    ret
  }

  // Spec Cat(Spec right) => new Spec(Start, Length + right.Length)
  .method public instance valuetype Snobol4.Runtime.Boxes.Spec Cat(valuetype Snobol4.Runtime.Boxes.Spec right) cil managed
  {
    .maxstack 4
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.Spec::Start
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.Spec::Length
    ldarga.s   right
    ldfld      int32 Snobol4.Runtime.Boxes.Spec::Length
    add
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    ret
  }
}

// ── MatchState class ────────────────────────────────────────────────────────
// mirrors C globals: const char *Σ (subject), int Δ (cursor), int Ω (length)

.class public auto ansi beforefieldinit
       Snobol4.Runtime.Boxes.MatchState
       extends [mscorlib]System.Object
{
  .field private string _subject
  .field public  int32  Cursor

  .method public specialname rtspecialname instance void .ctor(string subject, int32 startCursor) cil managed
  {
    .maxstack 2
    ldarg.0
    call       instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    dup
    brtrue     MS_NOTNULL
    pop
    ldstr      ""
  MS_NOTNULL:
    stfld      string Snobol4.Runtime.Boxes.MatchState::_subject
    ldarg.0
    ldarg.2
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ret
  }

  .method public specialname rtspecialname instance void .ctor(string subject) cil managed
  {
    .maxstack 2
    ldarg.0
    ldarg.1
    ldc.i4.0
    call       instance void Snobol4.Runtime.Boxes.MatchState::.ctor(string, int32)
    ret
  }

  // string get_Subject()
  .method public instance string get_Subject() cil managed
  {
    .maxstack 1
    ldarg.0
    ldfld      string Snobol4.Runtime.Boxes.MatchState::_subject
    ret
  }

  // int get_Length()
  .method public instance int32 get_Length() cil managed
  {
    .maxstack 1
    ldarg.0
    ldfld      string Snobol4.Runtime.Boxes.MatchState::_subject
    callvirt   instance int32 [mscorlib]System.String::get_Length()
    ret
  }

  // bool MatchesAt(int pos, string lit)
  .method public instance bool MatchesAt(int32 pos, string lit) cil managed
  {
    .maxstack 4
    .locals init (int32 V_litlen)
    // if pos + lit.Length > Subject.Length → false
    ldarg.2
    callvirt   instance int32 [mscorlib]System.String::get_Length()
    stloc.0
    ldarg.1
    ldloc.0
    add
    ldarg.0
    call       instance int32 Snobol4.Runtime.Boxes.MatchState::get_Length()
    bgt        MA_FALSE
    // Subject.AsSpan(pos, litlen).SequenceEqual(lit.AsSpan())
    ldarg.0
    ldfld      string Snobol4.Runtime.Boxes.MatchState::_subject
    ldarg.1
    ldloc.0
    call       valuetype [mscorlib]System.ReadOnlySpan`1<char> [mscorlib]System.MemoryExtensions::AsSpan(string, int32, int32)
    ldarg.2
    call       valuetype [mscorlib]System.ReadOnlySpan`1<char> [mscorlib]System.MemoryExtensions::AsSpan(string)
    call       bool [mscorlib]System.MemoryExtensions::SequenceEqual<char>(valuetype [mscorlib]System.ReadOnlySpan`1<!!0>, valuetype [mscorlib]System.ReadOnlySpan`1<!!0>)
    ret
  MA_FALSE:
    ldc.i4.0
    ret
  }

  // bool CharInSet(int pos, string charset)
  .method public instance bool CharInSet(int32 pos, string charset) cil managed
  {
    .maxstack 3
    // if pos >= Length → false
    ldarg.1
    ldarg.0
    call       instance int32 Snobol4.Runtime.Boxes.MatchState::get_Length()
    bge        CS_FALSE
    // charset.IndexOf(Subject[pos]) >= 0
    ldarg.2
    ldarg.0
    ldfld      string Snobol4.Runtime.Boxes.MatchState::_subject
    ldarg.1
    callvirt   instance char [mscorlib]System.String::get_Chars(int32)
    callvirt   instance int32 [mscorlib]System.String::IndexOf(char)
    ldc.i4.0
    blt        CS_FALSE
    ldc.i4.1
    ret
  CS_FALSE:
    ldc.i4.0
    ret
  }
}

// ── IByrdBox interface ──────────────────────────────────────────────────────

.class public interface abstract auto ansi
       Snobol4.Runtime.Boxes.IByrdBox
{
  .method public virtual abstract instance
          valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed {}

  .method public virtual abstract instance
          valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed {}
}
