// bb_atp.il — ATP: @var — write cursor position as integer string into varname
.assembly extern mscorlib {}
.class public auto ansi beforefieldinit Snobol4.Runtime.Boxes.BbAtp
       extends [mscorlib]System.Object implements Snobol4.Runtime.Boxes.IByrdBox
{
  .field private string _varname
  .field public  class [mscorlib]System.Action`2<string,string> SetVar

  .method public specialname rtspecialname instance void .ctor(string varname) cil managed
  {
    .maxstack 2
    ldarg.0
    call       instance void [mscorlib]System.Object::.ctor()
    ldarg.0
    ldarg.1
    dup
    brtrue     ATP_NN
    pop
    ldstr      ""
  ATP_NN:
    stfld      string Snobol4.Runtime.Boxes.BbAtp::_varname
    ret
  }

  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          α(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  {
    .maxstack 4
    // if SetVar != null && varname != "" → SetVar(varname, cursor.ToString())
    ldarg.0
    ldfld      class [mscorlib]System.Action`2<string,string> Snobol4.Runtime.Boxes.BbAtp::SetVar
    brfalse    ATP_A_SKIP
    ldarg.0
    ldfld      string Snobol4.Runtime.Boxes.BbAtp::_varname
    ldstr      ""
    callvirt   instance bool [mscorlib]System.String::Equals(string)
    brtrue     ATP_A_SKIP
    ldarg.0
    ldfld      class [mscorlib]System.Action`2<string,string> Snobol4.Runtime.Boxes.BbAtp::SetVar
    ldarg.0
    ldfld      string Snobol4.Runtime.Boxes.BbAtp::_varname
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    box        [mscorlib]System.Int32
    callvirt   instance string [mscorlib]System.Object::ToString()
    callvirt   instance void class [mscorlib]System.Action`2<string,string>::Invoke(!0, !1)
  ATP_A_SKIP:
    ldarg.1
    ldfld      int32 Snobol4.Runtime.Boxes.MatchState::Cursor
    call       valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::ZeroWidth(int32)
    ret
  }

  .method public virtual instance valuetype Snobol4.Runtime.Boxes.Spec
          β(class Snobol4.Runtime.Boxes.MatchState ms) cil managed
  { .maxstack 1 ldsfld valuetype Snobol4.Runtime.Boxes.Spec Snobol4.Runtime.Boxes.Spec::Fail ret }
}
