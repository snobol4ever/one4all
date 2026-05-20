#include "sm_template_common.h"
#include "emit_sm.h"
/* EC-UNI-8.3-fixup: every fn carries the full backend × mode matrix; NET PAT
 * is a known stub so IS_NET_* cells are n/a sentinels. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_fence0(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_fence0_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_noarg_push(out, "fence0()Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_fence(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_fence)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_fence1(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_fence1_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_pat_push(out, "fence1(Lrt/SnoPat;)Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    /* IS_JS_TEXT: n/a — fence1 has no JS arm in original */
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_fence)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_abort(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_abort_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_noarg_push(out, "abort_()Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_abort(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_abort)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_fail(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_fail_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_noarg_push(out, "fail_()Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_fail(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_fail)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_succeed(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_succeed_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_noarg_push(out, "succeed_()Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_succeed(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_succeed)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_arbno(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_arbno_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_pat_push(out, "arbno(Lrt/SnoPat;)Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_arbno(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_arbno)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
