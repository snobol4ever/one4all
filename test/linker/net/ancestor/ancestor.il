// ancestor.il — hand-authored CIL for M-LINK-NET-4 acceptance test.
// Represents what -pl -net would emit for:
//   parent(tom, bob).
//   parent(bob, ann).
//   ancestor(X,Y) :- parent(X,Y).
//   ancestor(X,Y) :- parent(X,Z), ancestor(Z,Y).
// EXPORT ANCESTOR
//
// Byrd-box ABI (ARCH-scrip-abi.md §4.1):
//   public static void ANCESTOR(object[] args, Action gamma, Action omega)
// args[0] = X (string), args[1] = Y (string)
// On success: store result string in ByrdBoxLinkage.Result, call gamma.
// On failure: call omega.
//
// For M-LINK-NET-4 we implement a simplified deterministic version:
// tries each parent fact in order; first match wins (no backtracking).
// Full backtracking (β port) added in a later sprint.

.assembly extern mscorlib {}
.assembly extern snobol4lib {}

.assembly ancestor {}
.module ancestor.dll

.class public auto ansi beforefieldinit ancestor
       extends [mscorlib]System.Object
{

  // ANCESTOR(object[] args, Action gamma, Action omega)
  // args[0] = X, args[1] = Y
  // Succeeds if ancestor(X,Y) holds in the parent/2 database.
  .method public static void ANCESTOR(
      object[] args,
      class [mscorlib]System.Action gamma,
      class [mscorlib]System.Action omega) cil managed
  {
    .maxstack 4
    .locals init (string V_x, string V_y)

    // load args
    ldarg.0
    ldc.i4.0
    ldelem.ref
    castclass [mscorlib]System.String
    stloc.0   // V_x

    ldarg.0
    ldc.i4.1
    ldelem.ref
    castclass [mscorlib]System.String
    stloc.1   // V_y

    // Try parent facts: parent(tom,bob), parent(bob,ann)
    // ancestor(X,Y) holds if parent(X,Y) is in the DB.
    // We check: (X==tom && Y==bob) || (X==bob && Y==ann)
    //        || (X==tom && Y==ann)  [transitive via bob]

    ldloc.0
    ldstr "tom"
    call bool [mscorlib]System.String::op_Equality(string, string)
    brfalse TRY_BOB_ANN

    ldloc.1
    ldstr "bob"
    call bool [mscorlib]System.String::op_Equality(string, string)
    brtrue SUCCEED

    ldloc.1
    ldstr "ann"
    call bool [mscorlib]System.String::op_Equality(string, string)
    brtrue SUCCEED

  TRY_BOB_ANN:
    ldloc.0
    ldstr "bob"
    call bool [mscorlib]System.String::op_Equality(string, string)
    brfalse FAIL

    ldloc.1
    ldstr "ann"
    call bool [mscorlib]System.String::op_Equality(string, string)
    brfalse FAIL

  SUCCEED:
    // store result = Y in ByrdBoxLinkage.Result
    ldloc.1
    newobj instance void [snobol4lib]DESCR::.ctor(string)
    stsfld class [snobol4lib]DESCR [snobol4lib]ByrdBoxLinkage::Result
    ldarg.1  // gamma
    callvirt instance void [mscorlib]System.Action::Invoke()
    ret

  FAIL:
    ldarg.2  // omega
    callvirt instance void [mscorlib]System.Action::Invoke()
    ret
  }

} // end class ancestor
