.assembly extern mscorlib {}
.assembly extern snobol4lib {}
.assembly coverage_net_gaps {}
.module coverage_net_gaps.dll

.class public auto ansi beforefieldinit coverage_net_gaps
       extends [mscorlib]System.Object
{

  .method private static void pn_cl_arith_0(object[] args, object[] vars, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    .locals init (object[] V_0, object[] V_1)
    ldarg.0
    stloc.0
    ldc.i4  6
    newarr     [mscorlib]System.Object
    stloc.1
    ldloc.1
    ldc.i4  0
    ldloc.0
    ldc.i4  0
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  1
    ldloc.0
    ldc.i4  1
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  2
    ldloc.0
    ldc.i4  2
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  3
    ldloc.0
    ldc.i4  3
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  4
    ldloc.0
    ldc.i4  4
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  5
    ldloc.0
    ldc.i4  5
    ldelem.ref
    stelem.ref
    ldc.i4  2
    newarr     [mscorlib]System.Object
    dup
    ldc.i4  0
    ldloc.1
    ldc.i4  2
    ldelem.ref
    stelem.ref
    dup
    ldc.i4  1
    ldnull
    stelem.ref
    ldnull
    ldftn      void coverage_net_gaps::pn_gamma_6()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    ldnull
    ldftn      void coverage_net_gaps::pn_omega_6()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::is(object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pn_flag_6
    brfalse    CL0_omega
    ldc.i4  2
    newarr     [mscorlib]System.Object
    dup
    ldc.i4  0
    ldloc.1
    ldc.i4  3
    ldelem.ref
    stelem.ref
    dup
    ldc.i4  1
    ldnull
    stelem.ref
    ldnull
    ldftn      void coverage_net_gaps::pn_gamma_7()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    ldnull
    ldftn      void coverage_net_gaps::pn_omega_7()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::is(object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pn_flag_7
    brfalse    CL0_omega
    ldc.i4  2
    newarr     [mscorlib]System.Object
    dup
    ldc.i4  0
    ldloc.1
    ldc.i4  4
    ldelem.ref
    stelem.ref
    dup
    ldc.i4  1
    ldnull
    stelem.ref
    ldnull
    ldftn      void coverage_net_gaps::pn_gamma_8()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    ldnull
    ldftn      void coverage_net_gaps::pn_omega_8()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::is(object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pn_flag_8
    brfalse    CL0_omega
    ldc.i4  2
    newarr     [mscorlib]System.Object
    dup
    ldc.i4  0
    ldloc.1
    ldc.i4  5
    ldelem.ref
    stelem.ref
    dup
    ldc.i4  1
    ldnull
    stelem.ref
    ldnull
    ldftn      void coverage_net_gaps::pn_gamma_9()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    ldnull
    ldftn      void coverage_net_gaps::pn_omega_9()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::is(object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pn_flag_9
    brfalse    CL0_omega
    ldloc.1
    ldc.i4  5
    ldelem.ref
    castclass [mscorlib]System.String
    newobj     instance void [snobol4lib]DESCR::.ctor(string)
    stsfld     class [snobol4lib]DESCR [snobol4lib]ByrdBoxLinkage::Result
    ldarg.2
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  CL0_omega:
    ldarg.3
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  }

  .method private static void ARITH(object[] args, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    ldarg.0
    ldnull
    ldarg.1
    ldarg.2
    call       void coverage_net_gaps::pn_cl_arith_0(object[], object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ret
  }

  .method private static void pn_cl_float_check_0(object[] args, object[] vars, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    .locals init (object[] V_0, object[] V_1)
    ldarg.0
    stloc.0
    ldc.i4  1
    newarr     [mscorlib]System.Object
    stloc.1
    ldloc.1
    ldc.i4  0
    ldloc.0
    ldc.i4  0
    ldelem.ref
    stelem.ref
    ldc.i4  2
    newarr     [mscorlib]System.Object
    dup
    ldc.i4  0
    ldloc.1
    ldc.i4  0
    ldelem.ref
    stelem.ref
    dup
    ldc.i4  1
    ldnull
    stelem.ref
    ldnull
    ldftn      void coverage_net_gaps::pn_gamma_11()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    ldnull
    ldftn      void coverage_net_gaps::pn_omega_11()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::is(object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pn_flag_11
    brfalse    CL0_omega
    ldloc.1
    ldc.i4  0
    ldelem.ref
    castclass [mscorlib]System.String
    newobj     instance void [snobol4lib]DESCR::.ctor(string)
    stsfld     class [snobol4lib]DESCR [snobol4lib]ByrdBoxLinkage::Result
    ldarg.2
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  CL0_omega:
    ldarg.3
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  }

  .method private static void FLOAT_CHECK(object[] args, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    ldarg.0
    ldnull
    ldarg.1
    ldarg.2
    call       void coverage_net_gaps::pn_cl_float_check_0(object[], object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ret
  }

  .method private static void pn_cl_max_0(object[] args, object[] vars, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    .locals init (object[] V_0, object[] V_1)
    ldarg.0
    stloc.0
    ldc.i4  2
    newarr     [mscorlib]System.Object
    stloc.1
    ldloc.1
    ldc.i4  0
    ldloc.0
    ldc.i4  0
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  1
    ldloc.0
    ldc.i4  1
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  0
    ldloc.0
    ldc.i4  2
    ldelem.ref
    stelem.ref
    ldc.i4  2
    newarr     [mscorlib]System.Object
    dup
    ldc.i4  0
    ldloc.1
    ldc.i4  0
    ldelem.ref
    stelem.ref
    dup
    ldc.i4  1
    ldloc.1
    ldc.i4  1
    ldelem.ref
    stelem.ref
    ldnull
    ldftn      void coverage_net_gaps::pn_gamma_15()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    ldnull
    ldftn      void coverage_net_gaps::pn_omega_15()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::>=(object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pn_flag_15
    brfalse    CL0_omega
    ldloc.1
    ldc.i4  2
    ldelem.ref
    castclass [mscorlib]System.String
    newobj     instance void [snobol4lib]DESCR::.ctor(string)
    stsfld     class [snobol4lib]DESCR [snobol4lib]ByrdBoxLinkage::Result
    ldarg.2
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  CL0_omega:
    ldarg.3
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  }

  .method private static void pn_cl_max_1(object[] args, object[] vars, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    .locals init (object[] V_0, object[] V_1)
    ldarg.0
    stloc.0
    ldc.i4  1
    newarr     [mscorlib]System.Object
    stloc.1
    ldloc.1
    ldc.i4  0
    ldloc.0
    ldc.i4  1
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  0
    ldloc.0
    ldc.i4  2
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  2
    ldelem.ref
    castclass [mscorlib]System.String
    newobj     instance void [snobol4lib]DESCR::.ctor(string)
    stsfld     class [snobol4lib]DESCR [snobol4lib]ByrdBoxLinkage::Result
    ldarg.2
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  CL1_omega:
    ldarg.3
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  }

  .method private static void MAX(object[] args, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    ldarg.0
    ldnull
    ldarg.1
    ldnull
    ldftn      void coverage_net_gaps::pd_omega_20()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::pn_cl_max_0(object[], object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pd_flag_20
    brtrue     Pd_exit_19
    ldarg.0
    ldnull
    ldarg.1
    ldnull
    ldftn      void coverage_net_gaps::pd_omega_21()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::pn_cl_max_1(object[], object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pd_flag_21
    brtrue     Pd_exit_19
    ldarg.2
    callvirt   instance void [mscorlib]System.Action::Invoke()
  Pd_exit_19:
    ret
  }

  .method private static void pn_cl_unify_test_0(object[] args, object[] vars, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    .locals init (object[] V_0, object[] V_1)
    ldarg.0
    stloc.0
    ldc.i4  1
    newarr     [mscorlib]System.Object
    stloc.1
    ldloc.1
    ldc.i4  0
    ldloc.0
    ldc.i4  0
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  0
    ldloc.0
    ldc.i4  1
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  1
    ldelem.ref
    castclass [mscorlib]System.String
    newobj     instance void [snobol4lib]DESCR::.ctor(string)
    stsfld     class [snobol4lib]DESCR [snobol4lib]ByrdBoxLinkage::Result
    ldarg.2
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  CL0_omega:
    ldarg.3
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  }

  .method private static void UNIFY_TEST(object[] args, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    ldarg.0
    ldnull
    ldarg.1
    ldarg.2
    call       void coverage_net_gaps::pn_cl_unify_test_0(object[], object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ret
  }

  .method private static void pn_cl_member_0(object[] args, object[] vars, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    .locals init (object[] V_0, object[] V_1)
    ldarg.0
    stloc.0
    ldc.i4  1
    newarr     [mscorlib]System.Object
    stloc.1
    ldloc.1
    ldc.i4  0
    ldloc.0
    ldc.i4  0
    ldelem.ref
    stelem.ref
    ldloc.1
    ldc.i4  1
    ldelem.ref
    castclass [mscorlib]System.String
    newobj     instance void [snobol4lib]DESCR::.ctor(string)
    stsfld     class [snobol4lib]DESCR [snobol4lib]ByrdBoxLinkage::Result
    ldarg.2
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  CL0_omega:
    ldarg.3
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  }

  .method private static void pn_cl_member_1(object[] args, object[] vars, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    .locals init (object[] V_0, object[] V_1)
    ldarg.0
    stloc.0
    ldc.i4  2
    newarr     [mscorlib]System.Object
    stloc.1
    ldloc.1
    ldc.i4  0
    ldloc.0
    ldc.i4  0
    ldelem.ref
    stelem.ref
    ldc.i4  2
    newarr     [mscorlib]System.Object
    dup
    ldc.i4  0
    ldloc.1
    ldc.i4  0
    ldelem.ref
    stelem.ref
    dup
    ldc.i4  1
    ldloc.1
    ldc.i4  1
    ldelem.ref
    stelem.ref
    ldnull
    ldftn      void coverage_net_gaps::pn_gamma_28()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    ldnull
    ldftn      void coverage_net_gaps::pn_omega_28()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::member(object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pn_flag_28
    brfalse    CL1_omega
    ldloc.1
    ldc.i4  1
    ldelem.ref
    castclass [mscorlib]System.String
    newobj     instance void [snobol4lib]DESCR::.ctor(string)
    stsfld     class [snobol4lib]DESCR [snobol4lib]ByrdBoxLinkage::Result
    ldarg.2
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  CL1_omega:
    ldarg.3
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  }

  .method private static void MEMBER(object[] args, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    ldarg.0
    ldnull
    ldarg.1
    ldnull
    ldftn      void coverage_net_gaps::pd_omega_30()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::pn_cl_member_0(object[], object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pd_flag_30
    brtrue     Pd_exit_29
    ldarg.0
    ldnull
    ldarg.1
    ldnull
    ldftn      void coverage_net_gaps::pd_omega_31()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::pn_cl_member_1(object[], object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pd_flag_31
    brtrue     Pd_exit_29
    ldarg.2
    callvirt   instance void [mscorlib]System.Action::Invoke()
  Pd_exit_29:
    ret
  }

  .method private static void pn_cl_main_0(object[] args, object[] vars, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    .locals init (object[] V_0, object[] V_1)
    ldarg.0
    stloc.0
    ldc.i4  8
    newarr     [mscorlib]System.Object
    stloc.1
    ldc.i4  2
    newarr     [mscorlib]System.Object
    dup
    ldc.i4  0
    ldnull
    stelem.ref
    dup
    ldc.i4  1
    ldstr      "true"
    stelem.ref
    ldnull
    ldftn      void coverage_net_gaps::pn_gamma_32()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    ldnull
    ldftn      void coverage_net_gaps::pn_omega_32()
    newobj     instance void [mscorlib]System.Action::.ctor(object, native int)
    call       void coverage_net_gaps::;(object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ldsfld     int32 coverage_net_gaps::pn_flag_32
    brfalse    CL0_omega
    ldarg.2
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  CL0_omega:
    ldarg.3
    callvirt   instance void [mscorlib]System.Action::Invoke()
    ret
  }

  .method private static void MAIN(object[] args, class [mscorlib]System.Action gamma, class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 8
    ldarg.0
    ldnull
    ldarg.1
    ldarg.2
    call       void coverage_net_gaps::pn_cl_main_0(object[], object[], class [mscorlib]System.Action, class [mscorlib]System.Action)
    ret
  }

  .field private static int32 pn_flag_6
  .method private static void pn_gamma_6() cil managed
  {
    .maxstack 1
    ldc.i4.1
    stsfld int32 coverage_net_gaps::pn_flag_6
    ret
  }
  .method private static void pn_omega_6() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pn_flag_6
    ret
  }
  .field private static int32 pn_flag_7
  .method private static void pn_gamma_7() cil managed
  {
    .maxstack 1
    ldc.i4.1
    stsfld int32 coverage_net_gaps::pn_flag_7
    ret
  }
  .method private static void pn_omega_7() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pn_flag_7
    ret
  }
  .field private static int32 pn_flag_8
  .method private static void pn_gamma_8() cil managed
  {
    .maxstack 1
    ldc.i4.1
    stsfld int32 coverage_net_gaps::pn_flag_8
    ret
  }
  .method private static void pn_omega_8() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pn_flag_8
    ret
  }
  .field private static int32 pn_flag_9
  .method private static void pn_gamma_9() cil managed
  {
    .maxstack 1
    ldc.i4.1
    stsfld int32 coverage_net_gaps::pn_flag_9
    ret
  }
  .method private static void pn_omega_9() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pn_flag_9
    ret
  }
  .field private static int32 pn_flag_11
  .method private static void pn_gamma_11() cil managed
  {
    .maxstack 1
    ldc.i4.1
    stsfld int32 coverage_net_gaps::pn_flag_11
    ret
  }
  .method private static void pn_omega_11() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pn_flag_11
    ret
  }
  .field private static int32 pn_flag_15
  .method private static void pn_gamma_15() cil managed
  {
    .maxstack 1
    ldc.i4.1
    stsfld int32 coverage_net_gaps::pn_flag_15
    ret
  }
  .method private static void pn_omega_15() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pn_flag_15
    ret
  }
  .field private static int32 pd_flag_20
  .method private static void pd_omega_20() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pd_flag_20
    ret
  }
  .field private static int32 pd_flag_21
  .method private static void pd_omega_21() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pd_flag_21
    ret
  }
  .field private static int32 pn_flag_28
  .method private static void pn_gamma_28() cil managed
  {
    .maxstack 1
    ldc.i4.1
    stsfld int32 coverage_net_gaps::pn_flag_28
    ret
  }
  .method private static void pn_omega_28() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pn_flag_28
    ret
  }
  .field private static int32 pd_flag_30
  .method private static void pd_omega_30() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pd_flag_30
    ret
  }
  .field private static int32 pd_flag_31
  .method private static void pd_omega_31() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pd_flag_31
    ret
  }
  .field private static int32 pn_flag_32
  .method private static void pn_gamma_32() cil managed
  {
    .maxstack 1
    ldc.i4.1
    stsfld int32 coverage_net_gaps::pn_flag_32
    ret
  }
  .method private static void pn_omega_32() cil managed
  {
    .maxstack 1
    ldc.i4.0
    stsfld int32 coverage_net_gaps::pn_flag_32
    ret
  }
} // end class coverage_net_gaps
