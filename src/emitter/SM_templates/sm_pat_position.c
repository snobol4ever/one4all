#include "sm_template_common.h"
#include "emit_sm.h"
/* EC-UNI-8.3-fixup: every fn carries the full backend × mode matrix; NET PAT
 * is a known stub so IS_NET_* cells are n/a sentinels. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_len(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_len_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_long_push(out, "len(J)Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_len(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_len)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_pos(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_pos_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_long_push(out, "pos(J)Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_pos(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_pos)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_rpos(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_rpos_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_long_push(out, "rpos(J)Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_rpos(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_rpos)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_tab(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_tab_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_long_push(out, "tab(J)Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_tab(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_tab)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_rtab(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_rtab_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_long_push(out, "rtab(J)Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_rtab(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_rtab)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_rem(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_rem_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_noarg_push(out, "rem()Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_rem(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_rem)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_bal(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_bal_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_noarg_push(out, "bal()Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_bal(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_bal)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_eps(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_eps_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_noarg_push(out, "eps()Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_eps(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_eps)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
