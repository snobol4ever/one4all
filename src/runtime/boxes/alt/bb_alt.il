// bb_alt.il — ALT: alternation
// α: try children[0..n-1] in order; β retries same child
// AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6

.assembly extern mscorlib {}

.class public auto ansi beforefieldinit
       Snobol4.Runtime.Boxes.BbAlt
       extends [mscorlib]System.Object
       implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private class Snobol4.Runtime.Boxes.IByrdBox[] _children
  .field private int32 _current
  .field private int32 _savedPos

  .method public specialname rtspecialname instance void
          .ctor(class Snobol4.Runtime.Boxes.IByrdBox[] children) cil managed
  {
    .maxstack 2
    ldarg.0
    call       instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    stfld      class Snobol4.Runtime.Boxes.IByrdBox[] Snobol4.Runtime.Boxes.BbAlt::_children
    ret
  }

  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 4
    .locals init (int32 V_i, valuetype Snobol4.Runtime.Boxes.Spec V_cr)

    // _savedPos = ms.Cursor
    ldarg.0
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    stfld      int32 Snobol4.Runtime.Boxes.BbAlt::_savedPos

    ldc.i4.0
    stloc.0   // i = 0

  ALT_A_LOOP:
    ldloc.0
    ldarg.0
    ldfld      class Snobol4.Runtime.Boxes.IByrdBox[] Snobol4.Runtime.Boxes.BbAlt::_children
    ldlen
    conv.i4
    bge        ALT_A_FAIL

    // ms.Cursor = _savedPos
    ldarg.1
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbAlt::_savedPos
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor

    // cr = children[i].α(ms)
    ldarg.0
    ldfld      class Snobol4.Runtime.Boxes.IByrdBox[] Snobol4.Runtime.Boxes.BbAlt::_children
    ldloc.0
    ldelem.ref
    ldarg.1
    callvirt   instance valuetype Snobol4.Runtime.Boxes.Spec
               Snobol4.Runtime.Boxes.IByrdBox::α(class Snobol4.Runtime.Boxes.MatchState)
    stloc.1

    ldloca.s   V_cr
    call       instance bool Snobol4.Runtime.Boxes.Spec::get_IsFail()
    brtrue     ALT_A_NEXT

    // success: _current = i; return cr
    ldarg.0
    ldloc.0
    stfld      int32 Snobol4.Runtime.Boxes.BbAlt::_current
    ldloc.1
    ret

  ALT_A_NEXT:
    ldloc.0
    ldc.i4.1
    add
    stloc.0
    br         ALT_A_LOOP

  ALT_A_FAIL:
    ldarg.1
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbAlt::_savedPos
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }

  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 3
    .locals init (valuetype Snobol4.Runtime.Boxes.Spec V_cr)
    // cr = children[_current].β(ms)
    ldarg.0
    ldfld      class Snobol4.Runtime.Boxes.IByrdBox[] Snobol4.Runtime.Boxes.BbAlt::_children
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbAlt::_current
    ldelem.ref
    ldarg.1
    callvirt   instance valuetype Snobol4.Runtime.Boxes.Spec
               Snobol4.Runtime.Boxes.IByrdBox::β(class Snobol4.Runtime.Boxes.MatchState)
    stloc.0
    ldloca.s   V_cr
    call       instance bool Snobol4.Runtime.Boxes.Spec::get_IsFail()
    brtrue     ALT_B_FAIL
    ldloc.0
    ret
  ALT_B_FAIL:
    ldsfld     valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
}
