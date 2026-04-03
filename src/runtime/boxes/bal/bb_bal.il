// bb_bal.il — BAL: match balanced-parentheses expression
.assembly extern mscorlib {}
.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbBal
       extends [mscorlib]System.Object implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private int32 _matchedLen
  .method public specialname rtspecialname instance void .ctor() cil managed
  {
    .maxstack 1
    ldarg.0
    call instance void [mscorlib]System.Object::.ctor()
    ret
  }
  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 4
    .locals init (int32 V_pos, int32 V_depth, int32 V_len, char V_c, int32 V_start)
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    stloc.0   // pos
    ldc.i4.0
    stloc.1   // depth
    ldarg.1
    callvirt   instance int32 Snobol4.Runtime.Boxes.MatchState::get_Length()
    stloc.2   // len
    ldloc.0
    stloc      4  // start = pos
    ldloc.0
    ldloc.2
    bge        BAL_A_FAIL
  BAL_A_LOOP:
    ldarg.1
    callvirt   instance string Snobol4.Runtime.Boxes.MatchState::get_Subject()
    ldloc.0
    callvirt   instance char [mscorlib]System.String::get_Chars(int32)
    stloc.3   // c = subject[pos]
    ldloc.3
    ldc.i4     40   // '('
    bne.un     BAL_CHECK_CLOSE
    ldloc.1
    ldc.i4.1
    add
    stloc.1   // depth++
    br         BAL_NEXT
  BAL_CHECK_CLOSE:
    ldloc.3
    ldc.i4     41   // ')'
    bne.un     BAL_NEXT
    ldloc.1
    ldc.i4.0
    beq        BAL_A_FAIL   // unmatched close
    ldloc.1
    ldc.i4.1
    sub
    stloc.1   // depth--
  BAL_NEXT:
    ldloc.0
    ldc.i4.1
    add
    stloc.0   // pos++
    // while pos < len && depth > 0
    ldloc.0
    ldloc.2
    bge        BAL_DONE
    ldloc.1
    ldc.i4.0
    bgt        BAL_A_LOOP
  BAL_DONE:
    ldloc.1
    ldc.i4.0
    bne.un     BAL_A_FAIL   // unclosed open
    ldarg.0
    ldloc.0
    ldloc      4
    sub
    stfld      int32 Snobol4.Runtime.Boxes.BbBal::_matchedLen
    ldarg.1
    ldloc.0
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldloc      4
    ldarg.0
    ldfld      int32 Snobol4.Runtime.Boxes.BbBal::_matchedLen
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Of(int32, int32)
    ret
  BAL_A_FAIL:
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
    ldfld      int32 Snobol4.Runtime.Boxes.BbBal::_matchedLen
    sub
    stfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail
    ret
  }
}
