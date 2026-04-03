// bb_terminals.il — FENCE, ABORT, FAIL, SUCCEED terminal boxes
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6
.assembly extern mscorlib {}

// BbFence: succeed once; β cuts
.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbFence
       extends [mscorlib]System.Object implements Snobol4.Runtime.Boxes.IByrdBox
{
  .method public specialname rtspecialname instance void .ctor() cil managed
  { .maxstack 1 ldarg.0 call instance void [mscorlib]System.Object::.ctor() ret }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::ZeroWidth(int32)
    ret
  }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  { .maxstack 1 ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail ret }
}

// BbAbort: always ω
.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbAbort
       extends [mscorlib]System.Object implements Snobol4.Runtime.Boxes.IByrdBox
{
  .method public specialname rtspecialname instance void .ctor() cil managed
  { .maxstack 1 ldarg.0 call instance void [mscorlib]System.Object::.ctor() ret }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  { .maxstack 1 ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail ret }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  { .maxstack 1 ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail ret }
}

// BbFail: always ω — force backtrack
.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbFail
       extends [mscorlib]System.Object implements Snobol4.Runtime.Boxes.IByrdBox
{
  .method public specialname rtspecialname instance void .ctor() cil managed
  { .maxstack 1 ldarg.0 call instance void [mscorlib]System.Object::.ctor() ret }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  { .maxstack 1 ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail ret }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  { .maxstack 1 ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail ret }
}

// BbSucceed: always γ zero-width; outer scan loop retries on backtrack
.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbSucceed
       extends [mscorlib]System.Object implements Snobol4.Runtime.Boxes.IByrdBox
{
  .method public specialname rtspecialname instance void .ctor() cil managed
  { .maxstack 1 ldarg.0 call instance void [mscorlib]System.Object::.ctor() ret }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::ZeroWidth(int32)
    ret
  }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 1
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::ZeroWidth(int32)
    ret
  }
}
