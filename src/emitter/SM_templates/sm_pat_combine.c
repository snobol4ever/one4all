#include "sm_template_common.h"
#include "emit_sm.h"
/* EC-UNI-8.3-fixup: every fn carries the full backend × mode matrix; NET PAT
 * is a known stub so IS_NET_* cells are n/a sentinels. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_cat(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_cat_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_2pat_push(out, "cat(Lrt/SnoPat;Lrt/SnoPat;)Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_cat(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_cat)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_alt(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_alt_dispatch(out, 0); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    (void)instr;
    if (IS_JVM_TEXT) { jvm_pat_2pat_push(out, "alt(Lrt/SnoPat;Lrt/SnoPat;)Lrt/SnoPat;"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_alt(); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_pat_alt)\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_capture(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_capture_template(out, instr); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    const char * s = instr->a[0].s ? instr->a[0].s : ""; int kind = (int)instr->a[1].i;
    if (IS_JVM_TEXT) {
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        jvm_emit_ldc_string(out, s); jvm_push_int2(out, kind);
        fprintf(out, "    invokestatic rt/SnoPat/capture(Lrt/SnoPat;Ljava/lang/String;I)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_capture("); js_escape(out, s); fprintf(out, ", %d); ", kind); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { int addr = wasm_intern_name(s); fprintf(out, "          (call $sno_pat_capture (i32.const 0x%x) (i32.const %lld) (i32.const %lld))\n", addr, (long long)instr->a[1].i, (long long)instr->a[2].i); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_capture_fn(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_capture_fn_template(out, instr); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    const char * fname = instr->a[0].s ? instr->a[0].s : ""; const char * namelist = instr->a[2].s ? instr->a[2].s : "";
    if (IS_JVM_TEXT) {
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        jvm_emit_ldc_string(out, fname); jvm_emit_ldc_string(out, namelist);
        fprintf(out, "    invokestatic rt/SnoPat/captureFn(Lrt/SnoPat;Ljava/lang/String;Ljava/lang/String;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_capture_fn("); js_escape(out, fname); fprintf(out, ", %lld, ", instr->a[1].i); js_escape(out, namelist); fprintf(out, "); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          ;; SM_PAT_CAPTURE_FN not yet implemented\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_capture_fn_args(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_capture_fn_args_template(out, instr); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    const char * fname = instr->a[0].s ? instr->a[0].s : ""; int nargs = (int)instr->a[2].i;
    if (IS_JVM_TEXT) {
        fprintf(out, "    bipush %d\n    anewarray java/lang/Object\n", nargs);
        for (int k = nargs - 1; k >= 0; k--) { fprintf(out, "    dup\n    bipush %d\n", k); fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n    aastore\n"); }
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n    swap\n");
        jvm_emit_ldc_string(out, fname);
        fprintf(out, "    swap\n    invokestatic rt/SnoPat/captureFnArgs(Lrt/SnoPat;Ljava/lang/String;[Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_capture_fn_args("); js_escape(out, fname); fprintf(out, ", %lld, %lld); ", instr->a[1].i, (long long)nargs); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          ;; SM_PAT_CAPTURE_FN_ARGS not yet implemented\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_usercall(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_usercall_template(out, instr); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    const char * fname = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM_TEXT) { jvm_emit_ldc_string(out, fname); fprintf(out, "    invokestatic rt/SnoPat/usercall(Ljava/lang/String;)Lrt/SnoPat;\n    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return; }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_usercall("); js_escape(out, fname); fprintf(out, "); "); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          ;; SM_PAT_USERCALL not yet implemented\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_usercall_args(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_pat_usercall_args_template(out, instr); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    const char * fname = instr->a[0].s ? instr->a[0].s : ""; int nargs = (int)instr->a[1].i;
    if (IS_JVM_TEXT) {
        fprintf(out, "    bipush %d\n    anewarray java/lang/Object\n", nargs);
        for (int k = nargs - 1; k >= 0; k--) { fprintf(out, "    dup\n    bipush %d\n", k); fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n    aastore\n"); }
        jvm_emit_ldc_string(out, fname);
        fprintf(out, "    swap\n    invokestatic rt/SnoPat/usercallArgs(Ljava/lang/String;[Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) { fprintf(out, "rt.pat_usercall_args("); js_escape(out, fname); fprintf(out, ", %d); ", nargs); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          ;; SM_PAT_USERCALL_ARGS not yet implemented\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_exec_stmt(const SM_Instr * instr, FILE * out) {
    if (IS_X86_TEXT) { emit_sm_exec_stmt_template(out, instr); return; }
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return; }
    /* IS_JVM_TEXT: n/a — SM_EXEC_STMT has no JVM arm */
    /* IS_JVM_BIN: n/a — SM_EXEC_STMT has no JVM arm */
    if (IS_JS_TEXT) { fprintf(out, "rt.exec_stmt("); js_escape(out, instr->a[0].s ? instr->a[0].s : ""); fprintf(out, ", %lld); ", instr->a[1].i); return; }
    /* IS_JS_BIN: n/a — JS has no binary form */
    /* IS_NET_TEXT: n/a — NET PAT stub */
    /* IS_NET_BIN: n/a — NET PAT stub */
    if (IS_WASM_TEXT) { fprintf(out, "          (call $sno_exec_stmt (i32.const 0) (i32.const 0) (i32.const 0))\n"); return; }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
}
