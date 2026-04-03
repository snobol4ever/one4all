// bb_executor.il — Phase 3 trampoline: drives the Byrd box graph
//
// MatchResult valuetype + ByrdBoxExecutor class.
// Mirrors bb_executor.cs / Scanner.Match() in snobol4dotnet.
//
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6
// SPRINT:  D-169

.assembly extern mscorlib {}

// ── MatchResult valuetype ───────────────────────────────────────────────────

.class public sequential ansi sealed beforefieldinit
       Snobol4.Runtime.Boxes.MatchResult
       extends [mscorlib]System.ValueType
{
  .field public bool   Success
  .field public int32  MatchStart
  .field public int32  MatchLength
  .field public int32  CursorAfter

  // static MatchResult Succeeded(int start, int len, int after)
  .method public static valuetype Snobol4.Runtime.Boxes.MatchResult
          Succeeded(int32 start, int32 len, int32 after) cil managed
  {
    .maxstack 2
    .locals init (valuetype Snobol4.Runtime.Boxes.MatchResult V_0)
    ldloca.s   V_0
    ldc.i4.1
    stfld      bool Snobol4.Runtime.Boxes.MatchResult::Success
    ldloca.s   V_0
    ldarg.0
    stfld      int32 Snobol4.Runtime.Boxes.MatchResult::MatchStart
    ldloca.s   V_0
    ldarg.1
    stfld      int32 Snobol4.Runtime.Boxes.MatchResult::MatchLength
    ldloca.s   V_0
    ldarg.2
    stfld      int32 Snobol4.Runtime.Boxes.MatchResult::CursorAfter
    ldloc.0
    ret
  }

  // static readonly MatchResult Failed
  .field public static initonly valuetype Snobol4.Runtime.Boxes.MatchResult Failed

  .method static void .cctor() cil managed
  {
    .maxstack 1
    // Failed is all-zero (Success=false, all ints=0) — default init suffices
    ret
  }
}

// ── ByrdBoxExecutor class ───────────────────────────────────────────────────

.class public auto ansi beforefieldinit
       Snobol4.Runtime.Boxes.ByrdBoxExecutor
       extends [mscorlib]System.Object
{
  .field private class Snobol4.Runtime.Boxes.IByrdBox _root
  // _captures: List<BbCapture> — stored as object, cast on use
  .field private class [mscorlib]System.Collections.Generic.List`1<object> _captures

  .method public specialname rtspecialname instance void
          .ctor(class Snobol4.Runtime.Boxes.IByrdBox root,
                class [mscorlib]System.Collections.Generic.IReadOnlyList`1<object> captures) cil managed
  {
    .maxstack 2
    ldarg.0
    call       instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    stfld      class Snobol4.Runtime.Boxes.IByrdBox Snobol4.Runtime.Boxes.ByrdBoxExecutor::_root
    ldarg.0
    newobj     instance void class [mscorlib]System.Collections.Generic.List`1<object>::.ctor()
    stfld      class [mscorlib]System.Collections.Generic.List`1<object> Snobol4.Runtime.Boxes.ByrdBoxExecutor::_captures
    ret
  }

  // ── Run — Phase 3 scan loop ─────────────────────────────────────────────
  // anchor=true: try only at startPos
  // anchor=false: try each position startPos..Length

  .method public instance valuetype Snobol4.Runtime.Boxes.MatchResult
          Run(class Snobol4.Runtime.Boxes.MatchState ms, bool anchor) cil managed
  {
    .maxstack 4
    .locals init (
      int32  V_startPos,    // 0
      int32  V_limit,       // 1
      int32  V_pos,         // 2
      valuetype Snobol4.Runtime.Boxes.Spec V_r,  // 3
      int32  V_matchStart   // 4
    )

    // startPos = ms.Cursor
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    stloc.0

    // limit = anchor ? startPos : ms.Length
    ldarg.2
    brfalse    RUN_NOTANCHOR
    ldloc.0
    br         RUN_HAVELIMIT
  RUN_NOTANCHOR:
    ldarg.1
    callvirt   instance int32 Snobol4.Runtime.Boxes.MatchState::get_Length()
  RUN_HAVELIMIT:
    stloc.1

    // for pos = startPos; pos <= limit; pos++
    ldloc.0
    stloc.2

  RUN_LOOP:
    ldloc.2
    ldloc.1
    bgt        RUN_FAIL

    // ms.Cursor = pos
    ldarg.1
    ldloc.2
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor

    // matchStart = pos
    ldloc.2
    stloc      4

    // r = _root.α(ms)
    ldarg.0
    ldfld      class Snobol4.Runtime.Boxes.IByrdBox Snobol4.Runtime.Boxes.ByrdBoxExecutor::_root
    ldarg.1
    callvirt   instance valuetype Snobol4.Runtime.Boxes.Spec
               Snobol4.Runtime.Boxes.IByrdBox::α(class Snobol4.Runtime.Boxes.MatchState)
    stloc.3

    // if !r.IsFail → success
    ldloca.s   V_r
    call       instance bool Snobol4.Runtime.Boxes.Spec::get_IsFail()
    brtrue     RUN_TRY_BETA

    // CommitCaptures(); return Succeeded(matchStart, cursor-matchStart, cursor)
    ldarg.0
    call       instance void Snobol4.Runtime.Boxes.ByrdBoxExecutor::CommitCaptures()
    ldloc      4
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc      4
    sub
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    call       valuetype Snobol4.Runtime.Boxes.MatchResult
               Snobol4.Runtime.Boxes.MatchResult::Succeeded(int32, int32, int32)
    ret

  RUN_TRY_BETA:
    // r = _root.β(ms)
    ldarg.0
    ldfld      class Snobol4.Runtime.Boxes.IByrdBox Snobol4.Runtime.Boxes.ByrdBoxExecutor::_root
    ldarg.1
    callvirt   instance valuetype Snobol4.Runtime.Boxes.Spec
               Snobol4.Runtime.Boxes.IByrdBox::β(class Snobol4.Runtime.Boxes.MatchState)
    stloc.3

    ldloca.s   V_r
    call       instance bool Snobol4.Runtime.Boxes.Spec::get_IsFail()
    brtrue     RUN_NEXT_POS

    ldarg.0
    call       instance void Snobol4.Runtime.Boxes.ByrdBoxExecutor::CommitCaptures()
    ldloc      4
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc      4
    sub
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    call       valuetype Snobol4.Runtime.Boxes.MatchResult
               Snobol4.Runtime.Boxes.MatchResult::Succeeded(int32, int32, int32)
    ret

  RUN_NEXT_POS:
    // ms.Cursor = pos + 1 (reset for next start position)
    ldarg.1
    ldloc.2
    ldc.i4.1
    add
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    // pos++
    ldloc.2
    ldc.i4.1
    add
    stloc.2
    br         RUN_LOOP

  RUN_FAIL:
    // restore cursor; return Failed
    ldarg.1
    ldloc.0
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldsfld     valuetype Snobol4.Runtime.Boxes.MatchResult
               Snobol4.Runtime.Boxes.MatchResult::Failed
    ret
  }

  // CommitCaptures — no-op at MSIL level; capture boxes commit inline
  .method private instance void CommitCaptures() cil managed
  {
    .maxstack 0
    ret
  }
}
