// test_snort_gate.il — SN4-NET-1 gate: SnoRt push_str + halt_tos must print the string
// Assemble: ilasm test_snort_gate.il && mono test_snort_gate.exe
// Expected output: hello world

.assembly extern mscorlib {}
.assembly test_snort_gate {}
.module test_snort_gate.exe

// ── Include SnoRt inline (since SnoRt.il is not a separate DLL) ──
// We paste the SnoRt class here via ilasm include pattern
// Actually ilasm has no #include; compile both files together:
//   ilasm /output:test_snort_gate.exe test_snort_gate.il SnoRt.il
// This file is the main entry point only.

.class public auto ansi beforefieldinit Prog
       extends [mscorlib]System.Object
{
  .method public static void Main() cil managed
  {
    .entrypoint
    .maxstack 8
    call       void SnoRt::_init()
    ldstr      "hello world"
    ldc.i4     11
    call       void SnoRt::push_str(string, int32)
    call       void SnoRt::halt_tos()
    ret
  }
}
