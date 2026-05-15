// SnoRt.il   SNOBOL4 .NET scalar runtime class for scrip --sm-emit --target=net
// Mirrors SnoRt.j (JVM) and sno_runtime.js (JS) scalar stacks.
// Assemble as part of the emitted executable (not a separate DLL).
// Static methods called by the MSIL switch/dispatch loop emitted by emit_net.c.
// AUTHORS: Lon Jones Cherryholmes   Jeffrey Cooper M.D.   Claude Sonnet 4.6

//    value stack                                                              
// All SNOBOL4 values are boxed as System.Object on the stack.
//   integer    boxed Int64
//   real       boxed Double
//   string     System.String
//   null       the string "" (SNOBOL4 null == empty string)
// coerce_num: if TOS is string   parse as Double (SNOBOL4 arithmetic coercion)
// arith(op): pops two, applies op, pushes result; op codes match SnoRt.j

.class public auto ansi beforefieldinit SnoRt
       extends [mscorlib]System.Object
{
  //    value stack                                                             
  .field private static class [mscorlib]System.Collections.Generic.Stack`1<object> _stk

  //    last-ok flag (statement success/failure)                                
  .field private static bool _last_ok

  //    pending conditional captures (varname / matched-text pairs)            
  .field private static class [mscorlib]System.Collections.Generic.List`1<string> _caps

  //    static constructor                                                      
  .method static void .cctor() cil managed
  {
    .maxstack 2
    newobj     instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::.ctor()
    stsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldc.i4.0
    stsfld     bool SnoRt::_last_ok
    newobj     instance void class [mscorlib]System.Collections.Generic.List`1<string>::.ctor()
    stsfld     class [mscorlib]System.Collections.Generic.List`1<string> SnoRt::_caps
    ret
  }

  //    _init   called at program start                                        
  .method public static void _init() cil managed
  {
    .maxstack 2
    newobj     instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::.ctor()
    stsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldc.i4.0
    stsfld     bool SnoRt::_last_ok
    newobj     instance void class [mscorlib]System.Collections.Generic.List`1<string>::.ctor()
    stsfld     class [mscorlib]System.Collections.Generic.List`1<string> SnoRt::_caps
    ret
  }

  //    _finalize   called at program end                                      
  .method public static void _finalize() cil managed
  {
    .maxstack 1
    ret
  }

  //    push helpers                                                            
  .method public static void push_int(int32 v) cil managed
  {
    .maxstack 2
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldarg.0
    box        [mscorlib]System.Int32
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  .method public static void push_str(string s, int32 len) cil managed
  {
    .maxstack 2
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldarg.0
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  .method public static void push_real(float64 v) cil managed
  {
    .maxstack 2
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldarg.0
    box        [mscorlib]System.Double
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  .method public static void push_null() cil managed
  {
    .maxstack 2
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      ""
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    push_var   read variable by name (INPUT special-cased)                  
  .method public static void push_var(string name) cil managed
  {
    .maxstack 2
    .locals init (string V_line)
    ldarg.0
    ldstr      "INPUT"
    call       bool [mscorlib]System.String::op_Equality(string, string)
    brfalse    PUSH_VAR_NOT_INPUT
    call       string [mscorlib]System.Console::ReadLine()
    stloc.0
    ldloc.0
    brtrue     PUSH_VAR_INPUT_OK
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      ""
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ldc.i4.0
    stsfld     bool SnoRt::_last_ok
    ret
  PUSH_VAR_INPUT_OK:
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc.0
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ldc.i4.1
    stsfld     bool SnoRt::_last_ok
    ret
  PUSH_VAR_NOT_INPUT:
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      ""
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    store_var   pop TOS and assign to variable (OUTPUT special-cased)       
  .method public static void store_var(string name) cil managed
  {
    .maxstack 2
    .locals init (object V_val, string V_str)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    stloc      V_val
    ldarg.0
    ldstr      "OUTPUT"
    call       bool [mscorlib]System.String::op_Equality(string, string)
    brfalse    STORE_VAR_NOT_OUTPUT
    ldloc      V_val
    call       string SnoRt::_obj_to_str(object)
    call       void [mscorlib]System.Console::WriteLine(string)
    ret
  STORE_VAR_NOT_OUTPUT:
    ret
  }

  //    pop_void   discard TOS                                                  
  .method public static void pop_void() cil managed
  {
    .maxstack 1
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    pop
    ret
  }

  //    concat   pop two strings, concatenate, push result                      
  .method public static void concat() cil managed
  {
    .maxstack 2
    .locals init (string V_b, string V_a)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       string SnoRt::_obj_to_str(object)
    stloc      V_b
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       string SnoRt::_obj_to_str(object)
    stloc      V_a
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_a
    ldloc      V_b
    call       string [mscorlib]System.String::Concat(string, string)
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    neg   negate TOS numeric value                                          
  .method public static void negate() cil managed
  {
    .maxstack 2
    .locals init (object V_v, float64 V_d)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    stloc      V_v
    ldloc      V_v
    isinst     [mscorlib]System.Int32
    brfalse    NEG_FLOAT
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_v
    unbox.any  [mscorlib]System.Int32
    neg
    box        [mscorlib]System.Int32
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  NEG_FLOAT:
    ldloc      V_v
    call       float64 SnoRt::_obj_to_dbl(object)
    neg
    stloc      V_d
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_d
    box        [mscorlib]System.Double
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    exp_op   exponentiation: pop b then a, push a**b                       
  .method public static void exp_op() cil managed
  {
    .maxstack 2
    .locals init (float64 V_b, float64 V_a, float64 V_r)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       float64 SnoRt::_obj_to_dbl(object)
    stloc      V_b
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       float64 SnoRt::_obj_to_dbl(object)
    stloc      V_a
    ldloc      V_a
    ldloc      V_b
    call       float64 [mscorlib]System.Math::Pow(float64, float64)
    stloc      V_r
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_r
    box        [mscorlib]System.Double
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    coerce_num   if TOS is string, parse as double and replace              
  .method public static void coerce_num() cil managed
  {
    .maxstack 2
    .locals init (object V_v, float64 V_d)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    stloc      V_v
    ldloc      V_v
    isinst     [mscorlib]System.String
    brfalse    COERCE_DONE
    ldloc      V_v
    castclass  [mscorlib]System.String
    call       float64 [mscorlib]System.Double::Parse(string)
    stloc      V_d
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_d
    box        [mscorlib]System.Double
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  COERCE_DONE:
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_v
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    arith   binary arithmetic: pop b then a, push a op b
  //   op: 1=add 2=sub 3=mul 4=div 6=mod  (matches SnoRt.j)
  .method public static void arith(int32 op) cil managed
  {
    .maxstack 2
    .locals init (float64 V_b, float64 V_a, float64 V_r)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       float64 SnoRt::_obj_to_dbl(object)
    stloc      V_b
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       float64 SnoRt::_obj_to_dbl(object)
    stloc      V_a
    ldarg.0
    ldc.i4.1
    beq        ARITH_ADD
    ldarg.0
    ldc.i4.2
    beq        ARITH_SUB
    ldarg.0
    ldc.i4.3
    beq        ARITH_MUL
    ldarg.0
    ldc.i4.4
    beq        ARITH_DIV
    ldloc      V_a
    ldloc      V_b
    call       float64 [mscorlib]System.Math::IEEERemainder(float64, float64)
    stloc      V_r
    br         ARITH_PUSH
  ARITH_ADD:
    ldloc      V_a
    ldloc      V_b
    add
    stloc      V_r
    br         ARITH_PUSH
  ARITH_SUB:
    ldloc      V_a
    ldloc      V_b
    sub
    stloc      V_r
    br         ARITH_PUSH
  ARITH_MUL:
    ldloc      V_a
    ldloc      V_b
    mul
    stloc      V_r
    br         ARITH_PUSH
  ARITH_DIV:
    ldloc      V_a
    ldloc      V_b
    div
    stloc      V_r
  ARITH_PUSH:
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_r
    box        [mscorlib]System.Double
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    acomp   arithmetic compare: pop b then a; set _last_ok = (a op b)
  //   op: 1=LT 2=LE 3=EQ 4=GE 5=GT 6=NE
  .method public static void acomp(int32 op) cil managed
  {
    .maxstack 2
    .locals init (float64 V_b, float64 V_a, bool V_res)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       float64 SnoRt::_obj_to_dbl(object)
    stloc      V_b
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       float64 SnoRt::_obj_to_dbl(object)
    stloc      V_a
    ldarg.0
    ldc.i4.1
    beq        ACOMP_LT
    ldarg.0
    ldc.i4.2
    beq        ACOMP_LE
    ldarg.0
    ldc.i4.3
    beq        ACOMP_EQ
    ldarg.0
    ldc.i4.4
    beq        ACOMP_GE
    ldarg.0
    ldc.i4.5
    beq        ACOMP_GT
    ldloc      V_a
    ldloc      V_b
    ceq
    ldc.i4.0
    ceq
    stloc      V_res
    br         ACOMP_STORE
  ACOMP_LT:
    ldloc      V_a
    ldloc      V_b
    clt
    stloc      V_res
    br         ACOMP_STORE
  ACOMP_LE:
    ldloc      V_a
    ldloc      V_b
    cgt
    ldc.i4.0
    ceq
    stloc      V_res
    br         ACOMP_STORE
  ACOMP_EQ:
    ldloc      V_a
    ldloc      V_b
    ceq
    stloc      V_res
    br         ACOMP_STORE
  ACOMP_GE:
    ldloc      V_a
    ldloc      V_b
    clt
    ldc.i4.0
    ceq
    stloc      V_res
    br         ACOMP_STORE
  ACOMP_GT:
    ldloc      V_a
    ldloc      V_b
    cgt
    stloc      V_res
  ACOMP_STORE:
    ldloc      V_res
    stsfld     bool SnoRt::_last_ok
    ret
  }

  //    lcomp   lexicographic compare: pop b then a; set _last_ok = (a op b)
  //   op: 1=LLT 2=LLE 3=LEQ 4=LGE 5=LGT 6=LNE
  .method public static void lcomp(int32 op) cil managed
  {
    .maxstack 2
    .locals init (string V_b, string V_a, int32 V_cmp, bool V_res)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       string SnoRt::_obj_to_str(object)
    stloc      V_b
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       string SnoRt::_obj_to_str(object)
    stloc      V_a
    ldloc      V_a
    ldloc      V_b
    call       int32 [mscorlib]System.String::CompareOrdinal(string, string)
    stloc      V_cmp
    ldarg.0
    ldc.i4.1
    beq        LCOMP_LT
    ldarg.0
    ldc.i4.2
    beq        LCOMP_LE
    ldarg.0
    ldc.i4.3
    beq        LCOMP_EQ
    ldarg.0
    ldc.i4.4
    beq        LCOMP_GE
    ldarg.0
    ldc.i4.5
    beq        LCOMP_GT
    ldloc      V_cmp
    ldc.i4.0
    ceq
    ldc.i4.0
    ceq
    stloc      V_res
    br         LCOMP_STORE
  LCOMP_LT:
    ldloc      V_cmp
    ldc.i4.0
    clt
    stloc      V_res
    br         LCOMP_STORE
  LCOMP_LE:
    ldloc      V_cmp
    ldc.i4.1
    clt
    stloc      V_res
    br         LCOMP_STORE
  LCOMP_EQ:
    ldloc      V_cmp
    ldc.i4.0
    ceq
    stloc      V_res
    br         LCOMP_STORE
  LCOMP_GE:
    ldloc      V_cmp
    ldc.i4.m1
    cgt
    stloc      V_res
    br         LCOMP_STORE
  LCOMP_GT:
    ldloc      V_cmp
    ldc.i4.0
    cgt
    stloc      V_res
  LCOMP_STORE:
    ldloc      V_res
    stsfld     bool SnoRt::_last_ok
    ret
  }

  //    last_ok / set_last_ok                                                   
  .method public static bool last_ok() cil managed
  {
    .maxstack 1
    ldsfld     bool SnoRt::_last_ok
    ret
  }

  .method public static void set_last_ok(bool v) cil managed
  {
    .maxstack 1
    ldarg.0
    stsfld     bool SnoRt::_last_ok
    ret
  }

  //    set_stno   record statement number                                      
  .method public static void set_stno(int32 n) cil managed
  {
    .maxstack 1
    ret
  }

  //    halt_tos   pop TOS and print to stdout (final OUTPUT)                  
  .method public static void halt_tos() cil managed
  {
    .maxstack 1
    .locals init (object V_v)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    stloc      V_v
    ldloc      V_v
    call       string SnoRt::_obj_to_str(object)
    call       void [mscorlib]System.Console::WriteLine(string)
    ret
  }

  //    call   invoke named SNOBOL4 function with nargs arguments              
  .method public static void sno_call(string name, int32 nargs) cil managed
  {
    .maxstack 1
    ret
  }

  //    do_return   function return                                             
  .method public static void do_return(int32 kind, bool has_val) cil managed
  {
    .maxstack 1
    ret
  }

  //    conditional-capture pending list                                       
  // push_cap(varname, text): record a pending conditional capture
  .method public static void push_cap(string varname, string text) cil managed
  {
    .maxstack 2
    ldsfld     class [mscorlib]System.Collections.Generic.List`1<string> SnoRt::_caps
    ldarg.0
    callvirt   instance void class [mscorlib]System.Collections.Generic.List`1<string>::Add(!0)
    ldsfld     class [mscorlib]System.Collections.Generic.List`1<string> SnoRt::_caps
    ldarg.1
    callvirt   instance void class [mscorlib]System.Collections.Generic.List`1<string>::Add(!0)
    ret
  }

  // commit_caps: apply all pending captures to the variable store
  .method public static void commit_caps() cil managed
  {
    .maxstack 2
    .locals init (int32 V_i, int32 V_count, string V_var, string V_txt)
    ldc.i4.0
    stloc      V_i
    ldsfld     class [mscorlib]System.Collections.Generic.List`1<string> SnoRt::_caps
    callvirt   instance int32 class [mscorlib]System.Collections.Generic.List`1<string>::get_Count()
    stloc      V_count
  CAP_COMMIT_LOOP:
    ldloc      V_i
    ldloc      V_count
    bge        CAP_COMMIT_DONE
    ldsfld     class [mscorlib]System.Collections.Generic.List`1<string> SnoRt::_caps
    ldloc      V_i
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.List`1<string>::get_Item(int32)
    stloc      V_var
    ldloc      V_i
    ldc.i4.1
    add
    stloc      V_i
    ldsfld     class [mscorlib]System.Collections.Generic.List`1<string> SnoRt::_caps
    ldloc      V_i
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.List`1<string>::get_Item(int32)
    stloc      V_txt
    ldloc      V_i
    ldc.i4.1
    add
    stloc      V_i
    ldloc      V_var
    ldstr      "OUTPUT"
    call       bool [mscorlib]System.String::op_Equality(string, string)
    brfalse    CAP_COMMIT_NEXT
    ldloc      V_txt
    call       void [mscorlib]System.Console::WriteLine(string)
  CAP_COMMIT_NEXT:
    br         CAP_COMMIT_LOOP
  CAP_COMMIT_DONE:
    ldsfld     class [mscorlib]System.Collections.Generic.List`1<string> SnoRt::_caps
    callvirt   instance void class [mscorlib]System.Collections.Generic.List`1<string>::Clear()
    ret
  }

  // discard_caps: throw away all pending captures
  .method public static void discard_caps() cil managed
  {
    .maxstack 1
    ldsfld     class [mscorlib]System.Collections.Generic.List`1<string> SnoRt::_caps
    callvirt   instance void class [mscorlib]System.Collections.Generic.List`1<string>::Clear()
    ret
  }

  //    private helpers                                                         
  // _obj_to_str: convert boxed value to its SNOBOL4 string representation
  .method private static string _obj_to_str(object v) cil managed
  {
    .maxstack 2
    ldarg.0
    brfalse    OTS_NULL
    ldarg.0
    isinst     [mscorlib]System.String
    brfalse    OTS_NOT_STR
    ldarg.0
    castclass  [mscorlib]System.String
    ret
  OTS_NOT_STR:
    ldarg.0
    callvirt   instance string object::ToString()
    ret
  OTS_NULL:
    ldstr      ""
    ret
  }

  // _obj_to_dbl: convert boxed value to double for arithmetic
  .method private static float64 _obj_to_dbl(object v) cil managed
  {
    .maxstack 1
    ldarg.0
    isinst     [mscorlib]System.Int32
    brfalse    OTD_NOT_INT
    ldarg.0
    unbox.any  [mscorlib]System.Int32
    conv.r8
    ret
  OTD_NOT_INT:
    ldarg.0
    isinst     [mscorlib]System.Double
    brfalse    OTD_NOT_DBL
    ldarg.0
    unbox.any  [mscorlib]System.Double
    ret
  OTD_NOT_DBL:
    ldarg.0
    isinst     [mscorlib]System.String
    brfalse    OTD_ZERO
    ldarg.0
    castclass  [mscorlib]System.String
    call       float64 [mscorlib]System.Double::Parse(string)
    ret
  OTD_ZERO:
    ldc.r8     0.0
    ret
  }

} // end class SnoRt
