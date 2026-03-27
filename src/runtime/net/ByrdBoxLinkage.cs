// ByrdBoxLinkage.cs — Byrd box linkage for DESCR values on the .NET backend.
//
// Owns the γ (success) and ω (failure) ports of the Byrd box model.
// α (entry) and β (redo) are the platform's concern — the CLR call
// instruction and resolver handle those; this class does not know where
// a call came from or where it goes next.
//
// Connects SCRIP assemblies across .NET assembly boundaries.
// Since Action delegates are void, there is no return-value channel in the
// normal C# sense.  ByrdBoxLinkage.Result is the out-of-band thread-local
// "result register" — the .NET equivalent of rax before jmp [rdx] on x64.
//
// Usage pattern (callee side):
//   ByrdBoxLinkage.Succeed(new DESCR("hello"), gamma);   // γ port
//   ByrdBoxLinkage.Fail(omega);                           // ω port
//
// Usage pattern (caller side):
//   SNOBOL4_greet_lib.GREET(args, () => {
//       DESCR result = ByrdBoxLinkage.Result;   // read after γ fires
//       ...
//   }, omega);
//
// [ThreadStatic] gives each thread its own slot — zero allocation,
// zero GC pressure per call.  See ABI decisions ABI-001, ABI-002.
//
// Ref: ARCH-scrip-abi.md §4.2.
// Compile into snobol4lib.dll alongside DESCR.

public static class ByrdBoxLinkage {
    [System.ThreadStatic] public static DESCR Result;

    public static void Succeed(DESCR v, System.Action gamma) {
        Result = v;
        gamma();
    }

    public static void Fail(System.Action omega) {
        omega();
    }
}
