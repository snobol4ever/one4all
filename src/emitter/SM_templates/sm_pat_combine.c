#include "sm_template_common.h"
#include "emit_sm.h"
/* EC-UNI-8.3-fixup: every fn carries the full backend × mode matrix; NET PAT
 * is a known stub so IS_NET_* cells are n/a sentinels. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_cat(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_cat_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_2pat_push(out, "cat(Lrt/SnoPat;Lrt/SnoPat;)Lrt/SnoPat;"); return; }
    if (IS_JS) { emit_textf("rt.pat_cat(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { emit_textf("          (call $sno_pat_cat)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_alt(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_alt_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_2pat_push(out, "alt(Lrt/SnoPat;Lrt/SnoPat;)Lrt/SnoPat;"); return; }
    if (IS_JS) { emit_textf("rt.pat_alt(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { emit_textf("          (call $sno_pat_alt)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_capture(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_capture_template(out, instr); return; }
    const char * s = instr->a[0].s ? instr->a[0].s : ""; int kind = (int)instr->a[1].i;
    if (IS_JVM) {
        emit_textf("    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        emit_textf("    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        jvm_emit_ldc_string(out, s); jvm_push_int2(out, kind);
        emit_textf("    invokestatic rt/SnoPat/capture(Lrt/SnoPat;Ljava/lang/String;I)Lrt/SnoPat;\n");
        emit_textf("    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JS) { emit_textf("rt.pat_capture("); js_escape(out, s); emit_textf(", %d); ", kind); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { int addr = wasm_intern_name(s); emit_textf("          (call $sno_pat_capture (i32.const 0x%x) (i32.const %lld) (i32.const %lld))\n", addr, (long long)instr->a[1].i, (long long)instr->a[2].i); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_capture_fn(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_capture_fn_template(out, instr); return; }
    const char * fname = instr->a[0].s ? instr->a[0].s : ""; const char * namelist = instr->a[2].s ? instr->a[2].s : "";
    if (IS_JVM) {
        emit_textf("    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        emit_textf("    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        jvm_emit_ldc_string(out, fname); jvm_emit_ldc_string(out, namelist);
        emit_textf("    invokestatic rt/SnoPat/captureFn(Lrt/SnoPat;Ljava/lang/String;Ljava/lang/String;)Lrt/SnoPat;\n");
        emit_textf("    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JS) { emit_textf("rt.pat_capture_fn("); js_escape(out, fname); emit_textf(", %lld, ", instr->a[1].i); js_escape(out, namelist); emit_textf("); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { emit_textf("          ;; SM_PAT_CAPTURE_FN not yet implemented\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_capture_fn_args(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_capture_fn_args_template(out, instr); return; }
    const char * fname = instr->a[0].s ? instr->a[0].s : ""; int nargs = (int)instr->a[2].i;
    if (IS_JVM) {
        emit_textf("    bipush %d\n    anewarray java/lang/Object\n", nargs);
        for (int k = nargs - 1; k >= 0; k--) { emit_textf("    dup\n    bipush %d\n", k); emit_textf("    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n    aastore\n"); }
        emit_textf("    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        emit_textf("    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n    swap\n");
        jvm_emit_ldc_string(out, fname);
        emit_textf("    swap\n    invokestatic rt/SnoPat/captureFnArgs(Lrt/SnoPat;Ljava/lang/String;[Ljava/lang/Object;)Lrt/SnoPat;\n");
        emit_textf("    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JS) { emit_textf("rt.pat_capture_fn_args("); js_escape(out, fname); emit_textf(", %lld, %lld); ", instr->a[1].i, (long long)nargs); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { emit_textf("          ;; SM_PAT_CAPTURE_FN_ARGS not yet implemented\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_usercall(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_usercall_template(out, instr); return; }
    const char * fname = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM) { jvm_emit_ldc_string(out, fname); emit_textf("    invokestatic rt/SnoPat/usercall(Ljava/lang/String;)Lrt/SnoPat;\n    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return; }
    if (IS_JS) { emit_textf("rt.pat_usercall("); js_escape(out, fname); emit_textf("); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { emit_textf("          ;; SM_PAT_USERCALL not yet implemented\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_usercall_args(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_pat_usercall_args_template(out, instr); return; }
    const char * fname = instr->a[0].s ? instr->a[0].s : ""; int nargs = (int)instr->a[1].i;
    if (IS_JVM) {
        emit_textf("    bipush %d\n    anewarray java/lang/Object\n", nargs);
        for (int k = nargs - 1; k >= 0; k--) { emit_textf("    dup\n    bipush %d\n", k); emit_textf("    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n    aastore\n"); }
        jvm_emit_ldc_string(out, fname);
        emit_textf("    swap\n    invokestatic rt/SnoPat/usercallArgs(Ljava/lang/String;[Ljava/lang/Object;)Lrt/SnoPat;\n");
        emit_textf("    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JS) { emit_textf("rt.pat_usercall_args("); js_escape(out, fname); emit_textf(", %d); ", nargs); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { emit_textf("          ;; SM_PAT_USERCALL_ARGS not yet implemented\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_exec_stmt(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_exec_stmt_template(out, instr); return; }
    /* IS_JVM: n/a — SM_EXEC_STMT has no JVM arm */
    if (IS_JS) { emit_textf("rt.exec_stmt("); js_escape(out, instr->a[0].s ? instr->a[0].s : ""); emit_textf(", %lld); ", instr->a[1].i); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { emit_textf("          (call $sno_exec_stmt (i32.const 0) (i32.const 0) (i32.const 0))\n"); return; }
}
