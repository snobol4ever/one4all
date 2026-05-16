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

  //    return-pc stack for user-defined function calls                         
  .field private static class [mscorlib]System.Collections.Generic.Stack`1<int32> _ret

  //    function-locals frames (one int32->object dict per active call)         
  .field private static class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>> _frames

  //    variable name -> value map (the SNOBOL4 global name-value store)       
  .field private static class [mscorlib]System.Collections.Generic.Dictionary`2<string,object> _vars

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
    newobj     instance void class [mscorlib]System.Collections.Generic.Stack`1<int32>::.ctor()
    stsfld     class [mscorlib]System.Collections.Generic.Stack`1<int32> SnoRt::_ret
    newobj     instance void class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>>::.ctor()
    stsfld     class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>> SnoRt::_frames
    newobj     instance void class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>::.ctor()
    stsfld     class [mscorlib]System.Collections.Generic.Dictionary`2<string,object> SnoRt::_vars
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
    newobj     instance void class [mscorlib]System.Collections.Generic.Stack`1<int32>::.ctor()
    stsfld     class [mscorlib]System.Collections.Generic.Stack`1<int32> SnoRt::_ret
    newobj     instance void class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>>::.ctor()
    stsfld     class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>> SnoRt::_frames
    newobj     instance void class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>::.ctor()
    stsfld     class [mscorlib]System.Collections.Generic.Dictionary`2<string,object> SnoRt::_vars
    ret
  }

  //    _finalize   called at program end                                      
  .method public static void _finalize() cil managed
  {
    .maxstack 1
    ret
  }

  //    push_ret_pc / pop_ret_pc   user-function return-address stack           
  .method public static void push_ret_pc(int32 pc) cil managed
  {
    .maxstack 2
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<int32> SnoRt::_ret
    ldarg.0
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<int32>::Push(!0)
    ret
  }
  .method public static int32 pop_ret_pc() cil managed
  {
    .maxstack 1
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<int32> SnoRt::_ret
    callvirt   instance int32 class [mscorlib]System.Collections.Generic.Stack`1<int32>::get_Count()
    ldc.i4.0
    bgt        POP_RET_OK
    ldc.i4.m1
    ret
  POP_RET_OK:
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<int32> SnoRt::_ret
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<int32>::Pop()
    ret
  }
  //    frame_push  -- push a new empty frame dict onto _frames              
  .method public static void frame_push() cil managed
  {
    .maxstack 2
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>> SnoRt::_frames
    newobj     instance void class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>::.ctor()
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>>::Push(!0)
    ret
  }
  //    frame_save  -- snapshot current value of named var into top frame    
  .method public static void frame_save(string name) cil managed
  {
    .maxstack 4
    .locals init (class [mscorlib]System.Collections.Generic.Dictionary`2<string,object> frame, object curval)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>> SnoRt::_frames
    callvirt   instance int32 class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>>::get_Count()
    ldc.i4.0
    ble        FSAVE_DONE
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>> SnoRt::_frames
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>>::Peek()
    stloc.0
    ldsfld     class [mscorlib]System.Collections.Generic.Dictionary`2<string,object> SnoRt::_vars
    ldarg.0
    callvirt   instance bool class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>::ContainsKey(!0)
    brfalse    FSAVE_NULL
    ldsfld     class [mscorlib]System.Collections.Generic.Dictionary`2<string,object> SnoRt::_vars
    ldarg.0
    callvirt   instance !1 class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>::get_Item(!0)
    stloc.1
    br         FSAVE_STORE
  FSAVE_NULL:
    ldstr      ""
    stloc.1
  FSAVE_STORE:
    ldloc.0
    ldarg.0
    ldloc.1
    callvirt   instance void class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>::set_Item(!0, !1)
  FSAVE_DONE:
    ret
  }
  //    frame_exit  -- pop top frame and restore all saved vars              
  .method public static void frame_exit() cil managed
  {
    .maxstack 4
    .locals init (class [mscorlib]System.Collections.Generic.Dictionary`2<string,object> frame,
                  class [mscorlib]System.Collections.Generic.List`1<string> klist,
                  int32 idx,
                  string k)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>> SnoRt::_frames
    callvirt   instance int32 class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>>::get_Count()
    ldc.i4.0
    ble        FEXIT_DONE
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>> SnoRt::_frames
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>>::Pop()
    stloc.0
    ldloc.0
    callvirt   instance class [mscorlib]System.Collections.Generic.Dictionary`2/KeyCollection<!0,!1> class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>::get_Keys()
    newobj     instance void class [mscorlib]System.Collections.Generic.List`1<string>::.ctor(class [mscorlib]System.Collections.Generic.IEnumerable`1<!0>)
    stloc.1
    ldc.i4.0
    stloc.2
  FEXIT_LOOP:
    ldloc.2
    ldloc.1
    callvirt   instance int32 class [mscorlib]System.Collections.Generic.List`1<string>::get_Count()
    bge        FEXIT_DONE
    ldloc.1
    ldloc.2
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.List`1<string>::get_Item(int32)
    stloc.3
    ldsfld     class [mscorlib]System.Collections.Generic.Dictionary`2<string,object> SnoRt::_vars
    ldloc.3
    ldloc.0
    ldloc.3
    callvirt   instance !1 class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>::get_Item(!0)
    callvirt   instance void class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>::set_Item(!0, !1)
    ldloc.2
    ldc.i4.1
    add
    stloc.2
    br         FEXIT_LOOP
  FEXIT_DONE:
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

  //    push_var   read variable by name (INPUT special-cased, else from _vars) 
  .method public static void push_var(string name) cil managed
  {
    .maxstack 3
    .locals init (string V_line, object V_val)
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
    ldnull
    stloc      V_val
    ldsfld     class [mscorlib]System.Collections.Generic.Dictionary`2<string,object> SnoRt::_vars
    ldarg.0
    ldloca     V_val
    callvirt   instance bool class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>::TryGetValue(!0, !1&)
    brfalse    PUSH_VAR_EMPTY
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_val
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  PUSH_VAR_EMPTY:
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      ""
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    store_var   pop TOS and assign to variable (OUTPUT special-cased)       
  .method public static void store_var(string name) cil managed
  {
    .maxstack 3
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
    ldsfld     class [mscorlib]System.Collections.Generic.Dictionary`2<string,object> SnoRt::_vars
    ldarg.0
    ldloc      V_val
    callvirt   instance void class [mscorlib]System.Collections.Generic.Dictionary`2<string,object>::set_Item(!0, !1)
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
    .maxstack 3
    .locals init (string V_0, string V_1)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       string SnoRt::_obj_to_str(object)
    stloc.0
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       string SnoRt::_obj_to_str(object)
    stloc.1
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc.1
    ldloc.0
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
    .maxstack 3
    .locals init (object V_v, float64 V_d, bool V_ok)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    stloc      V_v
    ldloc      V_v
    isinst     [mscorlib]System.String
    brfalse    COERCE_DONE
    ldc.r8     0.0
    stloc      V_d
    ldloc      V_v
    castclass  [mscorlib]System.String
    ldloca     V_d
    call       bool [mscorlib]System.Double::TryParse(string, float64&)
    pop
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
    .maxstack 2
    .locals init (object V_v)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance int32 class [mscorlib]System.Collections.Generic.Stack`1<object>::get_Count()
    ldc.i4.0
    ble        HALT_EMPTY
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    stloc      V_v
    ldloc      V_v
    call       string SnoRt::_obj_to_str(object)
    call       void [mscorlib]System.Console::WriteLine(string)
  HALT_EMPTY:
    ret
  }

  //    sno_call   invoke named SNOBOL4 built-in (8 minimal built-ins)
  //   Unknown names: set last_ok=false, pop nargs, push "".
  .method public static void sno_call(string name, int32 nargs) cil managed
  {
    .maxstack 3
    .locals init (int32 V_i)
    ldarg.0
    ldstr      "SIZE"
    call       bool [mscorlib]System.String::op_Equality(string, string)
    brfalse    CALL_NOT_SIZE
    call       void SnoRt::builtin_SIZE()
    ldc.i4.1
    stsfld     bool SnoRt::_last_ok
    ret
  CALL_NOT_SIZE:
    ldarg.0
    ldstr      "TRIM"
    call       bool [mscorlib]System.String::op_Equality(string, string)
    brfalse    CALL_NOT_TRIM
    call       void SnoRt::builtin_TRIM()
    ldc.i4.1
    stsfld     bool SnoRt::_last_ok
    ret
  CALL_NOT_TRIM:
    ldarg.0
    ldstr      "DUPL"
    call       bool [mscorlib]System.String::op_Equality(string, string)
    brfalse    CALL_NOT_DUPL
    call       void SnoRt::builtin_DUPL()
    ldc.i4.1
    stsfld     bool SnoRt::_last_ok
    ret
  CALL_NOT_DUPL:
    ldarg.0
    ldstr      "SUBSTR"
    call       bool [mscorlib]System.String::op_Equality(string, string)
    brfalse    CALL_NOT_SUBSTR
    call       void SnoRt::builtin_SUBSTR()
    ldc.i4.1
    stsfld     bool SnoRt::_last_ok
    ret
  CALL_NOT_SUBSTR:
    ldarg.0
    ldstr      "IDENT"
    call       bool [mscorlib]System.String::op_Equality(string, string)
    brfalse    CALL_NOT_IDENT
    call       void SnoRt::builtin_IDENT()
    ret
  CALL_NOT_IDENT:
    ldarg.0
    ldstr      "DIFFER"
    call       bool [mscorlib]System.String::op_Equality(string, string)
    brfalse    CALL_NOT_DIFFER
    call       void SnoRt::builtin_DIFFER()
    ret
  CALL_NOT_DIFFER:
    ldarg.0
    ldstr      "INTEGER"
    call       bool [mscorlib]System.String::op_Equality(string, string)
    brfalse    CALL_NOT_INTEGER
    call       void SnoRt::builtin_INTEGER()
    ret
  CALL_NOT_INTEGER:
    ldarg.0
    ldstr      "DATATYPE"
    call       bool [mscorlib]System.String::op_Equality(string, string)
    brfalse    CALL_NOT_DATATYPE
    call       void SnoRt::builtin_DATATYPE()
    ldc.i4.1
    stsfld     bool SnoRt::_last_ok
    ret
  CALL_NOT_DATATYPE:
    ldc.i4.0
    stloc      V_i
  CALL_POP_LOOP:
    ldloc      V_i
    ldarg.1
    bge        CALL_POP_DONE
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance int32 class [mscorlib]System.Collections.Generic.Stack`1<object>::get_Count()
    ldc.i4.0
    ble        CALL_POP_NEXT
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    pop
  CALL_POP_NEXT:
    ldloc      V_i
    ldc.i4.1
    add
    stloc      V_i
    br         CALL_POP_LOOP
  CALL_POP_DONE:
    ldc.i4.0
    stsfld     bool SnoRt::_last_ok
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      ""
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    do_return   function return (kind: 0=RETURN 1=FRETURN 2=NRETURN)
  //   kind==1 (FRETURN) sets last_ok=false; otherwise last_ok=true.
  .method public static void do_return(int32 kind, bool has_val) cil managed
  {
    .maxstack 2
    ldarg.0
    ldc.i4.1
    bne.un     DO_RET_OK
    ldc.i4.0
    stsfld     bool SnoRt::_last_ok
    ret
  DO_RET_OK:
    ldc.i4.1
    stsfld     bool SnoRt::_last_ok
    ret
  }

  //    SIZE(s) -> length of string                                              
  .method private static void builtin_SIZE() cil managed
  {
    .maxstack 2
    .locals init (string V_s)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       string SnoRt::_obj_to_str(object)
    stloc      V_s
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_s
    callvirt   instance int32 [mscorlib]System.String::get_Length()
    box        [mscorlib]System.Int32
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    TRIM(s) -> trailing-space trimmed string                                 
  .method private static void builtin_TRIM() cil managed
  {
    .maxstack 3
    .locals init (string V_s, int32 V_i)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       string SnoRt::_obj_to_str(object)
    stloc      V_s
    ldloc      V_s
    callvirt   instance int32 [mscorlib]System.String::get_Length()
    stloc      V_i
  TRIM_LOOP:
    ldloc      V_i
    ldc.i4.0
    ble        TRIM_DONE
    ldloc      V_s
    ldloc      V_i
    ldc.i4.1
    sub
    callvirt   instance char [mscorlib]System.String::get_Chars(int32)
    ldc.i4.s   32
    bne.un     TRIM_DONE
    ldloc      V_i
    ldc.i4.1
    sub
    stloc      V_i
    br         TRIM_LOOP
  TRIM_DONE:
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_s
    ldc.i4.0
    ldloc      V_i
    callvirt   instance string [mscorlib]System.String::Substring(int32, int32)
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    DUPL(s, n) -> s repeated n times                                         
  .method private static void builtin_DUPL() cil managed
  {
    .maxstack 3
    .locals init (object V_n, string V_s, int32 V_count, class [mscorlib]System.Text.StringBuilder V_sb)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    stloc      V_n
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       string SnoRt::_obj_to_str(object)
    stloc      V_s
    ldloc      V_n
    call       float64 SnoRt::_obj_to_dbl(object)
    conv.i4
    stloc      V_count
    newobj     instance void [mscorlib]System.Text.StringBuilder::.ctor()
    stloc      V_sb
  DUPL_LOOP:
    ldloc      V_count
    ldc.i4.0
    ble        DUPL_DONE
    ldloc      V_sb
    ldloc      V_s
    callvirt   instance class [mscorlib]System.Text.StringBuilder [mscorlib]System.Text.StringBuilder::Append(string)
    pop
    ldloc      V_count
    ldc.i4.1
    sub
    stloc      V_count
    br         DUPL_LOOP
  DUPL_DONE:
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_sb
    callvirt   instance string [mscorlib]System.Object::ToString()
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    SUBSTR(s, i, n) -> substring (1-based, SNOBOL4 convention)              
  .method private static void builtin_SUBSTR() cil managed
  {
    .maxstack 3
    .locals init (object V_n, object V_i, string V_s, int32 V_start, int32 V_len, int32 V_slen)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    stloc      V_n
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    stloc      V_i
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    call       string SnoRt::_obj_to_str(object)
    stloc      V_s
    ldloc      V_i
    call       float64 SnoRt::_obj_to_dbl(object)
    conv.i4
    ldc.i4.1
    sub
    stloc      V_start
    ldloc      V_n
    call       float64 SnoRt::_obj_to_dbl(object)
    conv.i4
    stloc      V_len
    ldloc      V_s
    callvirt   instance int32 [mscorlib]System.String::get_Length()
    stloc      V_slen
    ldloc      V_start
    ldc.i4.0
    bge        SUBSTR_CHK_LEN
    ldc.i4.0
    stloc      V_start
  SUBSTR_CHK_LEN:
    ldloc      V_start
    ldloc      V_len
    add
    ldloc      V_slen
    ble        SUBSTR_OK
    ldloc      V_slen
    ldloc      V_start
    sub
    stloc      V_len
  SUBSTR_OK:
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldloc      V_len
    ldc.i4.0
    bge        SUBSTR_PUSH
    ldstr      ""
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  SUBSTR_PUSH:
    ldloc      V_s
    ldloc      V_start
    ldloc      V_len
    callvirt   instance string [mscorlib]System.String::Substring(int32, int32)
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    IDENT(a, b) -> last_ok = (a string-equals b); pops 2 with no push        
  .method private static void builtin_IDENT() cil managed
  {
    .maxstack 2
    .locals init (string V_a, string V_b)
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
    call       bool [mscorlib]System.String::op_Equality(string, string)
    stsfld     bool SnoRt::_last_ok
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      ""
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    DIFFER(a, b) -> last_ok = (a string-not-equals b)                        
  .method private static void builtin_DIFFER() cil managed
  {
    .maxstack 2
    .locals init (string V_a, string V_b, bool V_eq)
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
    call       bool [mscorlib]System.String::op_Equality(string, string)
    stloc      V_eq
    ldloc      V_eq
    ldc.i4.0
    ceq
    stsfld     bool SnoRt::_last_ok
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      ""
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    INTEGER(x) -> last_ok = (x is integer or parseable as integer)          
  .method private static void builtin_INTEGER() cil managed
  {
    .maxstack 3
    .locals init (object V_v, int32 V_i, bool V_ok)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    stloc      V_v
    ldloc      V_v
    isinst     [mscorlib]System.Int32
    brfalse    INT_CHK_STR
    ldc.i4.1
    stsfld     bool SnoRt::_last_ok
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      ""
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  INT_CHK_STR:
    ldloc      V_v
    isinst     [mscorlib]System.String
    brfalse    INT_FAIL
    ldc.i4.0
    stloc      V_i
    ldloc      V_v
    castclass  [mscorlib]System.String
    ldloca     V_i
    call       bool [mscorlib]System.Int32::TryParse(string, int32&)
    stloc      V_ok
    ldloc      V_ok
    stsfld     bool SnoRt::_last_ok
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      ""
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  INT_FAIL:
    ldc.i4.0
    stsfld     bool SnoRt::_last_ok
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      ""
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  }

  //    DATATYPE(x) -> uppercase name of x's type (one4all convention)          
  .method private static void builtin_DATATYPE() cil managed
  {
    .maxstack 2
    .locals init (object V_v)
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    callvirt   instance !0 class [mscorlib]System.Collections.Generic.Stack`1<object>::Pop()
    stloc      V_v
    ldloc      V_v
    isinst     [mscorlib]System.Int32
    brfalse    DT_NOT_INT
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      "INTEGER"
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  DT_NOT_INT:
    ldloc      V_v
    isinst     [mscorlib]System.Double
    brfalse    DT_NOT_DBL
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      "REAL"
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
    ret
  DT_NOT_DBL:
    ldsfld     class [mscorlib]System.Collections.Generic.Stack`1<object> SnoRt::_stk
    ldstr      "STRING"
    callvirt   instance void class [mscorlib]System.Collections.Generic.Stack`1<object>::Push(!0)
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
    .maxstack 3
    .locals init (float64 V_d, bool V_ok)
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
    ldc.r8     0.0
    stloc      V_d
    ldarg.0
    castclass  [mscorlib]System.String
    ldloca     V_d
    call       bool [mscorlib]System.Double::TryParse(string, float64&)
    pop
    ldloc      V_d
    ret
  OTD_ZERO:
    ldc.r8     0.0
    ret
  }

} // end class SnoRt
