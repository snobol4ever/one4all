#include "sm_template_common.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* JVM inner helpers */
static void jvm_pat_str_push(FILE * out, int i, const char * tag, const char * method) {
    fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
    fprintf(out, "    dup\n    ifnonnull pat_%s_nn_%d\n    pop\n    ldc \"\"\n    goto pat_%s_done_%d\n", tag, i, tag, i);
    fprintf(out, "pat_%s_nn_%d:\n    invokevirtual java/lang/Object/toString()Ljava/lang/String;\n", tag, i);
    fprintf(out, "pat_%s_done_%d:\n    invokestatic rt/SnoPat/%s\n", tag, i, method);
    fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n");
}
static void jvm_pat_long_push(FILE * out, const char * method) {
    fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
    fprintf(out, "    invokestatic rt/SnoRt/coerce_to_long(Ljava/lang/Object;)J\n");
    fprintf(out, "    invokestatic rt/SnoPat/%s\n", method);
    fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n");
}
static void jvm_pat_noarg_push(FILE * out, const char * method) {
    fprintf(out, "    invokestatic rt/SnoPat/%s\n", method);
    fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n");
}
static void jvm_pat_pat_push(FILE * out, const char * method) {
    fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
    fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
    fprintf(out, "    invokestatic rt/SnoPat/%s\n", method);
    fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n");
}
static void jvm_pat_2pat_push(FILE * out, const char * method) {
    fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
    fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
    fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
    fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
    fprintf(out, "    swap\n");
    fprintf(out, "    invokestatic rt/SnoPat/%s\n", method);
    fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_lit(const SM_Instr * instr, FILE * out) {
    const char * s = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM)  { jvm_emit_ldc_string(out, s); fprintf(out, "    invokestatic rt/SnoPat/lit(Ljava/lang/String;)Lrt/SnoPat;\n    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return; }
    if (IS_JS)   { fprintf(out, "rt.pat_lit("); js_escape(out, s); fprintf(out, "); "); return; }
    if (IS_WASM) { int addr = wasm_intern_str(s); fprintf(out, "          (call $sno_pat_lit (i32.const 0x%x) (i32.const %d))\n", addr, (int)strlen(s)); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_any(const SM_Instr * instr, FILE * out) {
    (void)instr;
    if (IS_JVM)  { jvm_pat_str_push(out, 0, "any", "any(Ljava/lang/String;)Lrt/SnoPat;"); return; }
    if (IS_JS)   { fprintf(out, "rt.pat_any(); "); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_any)\n"); return; }
}
void sm_pat_any_i(const SM_Instr * instr, int i, FILE * out) {
    (void)instr;
    if (IS_JVM)  { jvm_pat_str_push(out, i, "any", "any(Ljava/lang/String;)Lrt/SnoPat;"); return; }
    if (IS_JS)   { fprintf(out, "rt.pat_any(); "); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_any)\n"); return; }
}
void sm_pat_notany(const SM_Instr * instr, int i, FILE * out) {
    (void)instr;
    if (IS_JVM)  { jvm_pat_str_push(out, i, "nany", "notany(Ljava/lang/String;)Lrt/SnoPat;"); return; }
    if (IS_JS)   { fprintf(out, "rt.pat_notany(); "); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_notany)\n"); return; }
}
void sm_pat_span(const SM_Instr * instr, int i, FILE * out) {
    (void)instr;
    if (IS_JVM)  { jvm_pat_str_push(out, i, "span", "span(Ljava/lang/String;)Lrt/SnoPat;"); return; }
    if (IS_JS)   { fprintf(out, "rt.pat_span(); "); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_span)\n"); return; }
}
void sm_pat_break(const SM_Instr * instr, int i, FILE * out) {
    (void)instr;
    if (IS_JVM)  { jvm_pat_str_push(out, i, "brk", "brk(Ljava/lang/String;)Lrt/SnoPat;"); return; }
    if (IS_JS)   { fprintf(out, "rt.pat_break(); "); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_break)\n"); return; }
}
void sm_pat_len  (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_long_push(out, "len(J)Lrt/SnoPat;");   if (IS_JS) fprintf(out, "rt.pat_len(); ");   if (IS_WASM) fprintf(out, "          (call $sno_pat_len)\n"); }
void sm_pat_pos  (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_long_push(out, "pos(J)Lrt/SnoPat;");   if (IS_JS) fprintf(out, "rt.pat_pos(); ");   if (IS_WASM) fprintf(out, "          (call $sno_pat_pos)\n"); }
void sm_pat_rpos (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_long_push(out, "rpos(J)Lrt/SnoPat;");  if (IS_JS) fprintf(out, "rt.pat_rpos(); ");  if (IS_WASM) fprintf(out, "          (call $sno_pat_rpos)\n"); }
void sm_pat_tab  (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_long_push(out, "tab(J)Lrt/SnoPat;");   if (IS_JS) fprintf(out, "rt.pat_tab(); ");   if (IS_WASM) fprintf(out, "          (call $sno_pat_tab)\n"); }
void sm_pat_rtab (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_long_push(out, "rtab(J)Lrt/SnoPat;");  if (IS_JS) fprintf(out, "rt.pat_rtab(); ");  if (IS_WASM) fprintf(out, "          (call $sno_pat_rtab)\n"); }
void sm_pat_arb  (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_noarg_push(out, "arb()Lrt/SnoPat;");    if (IS_JS) fprintf(out, "rt.pat_arb(); ");   if (IS_WASM) fprintf(out, "          (call $sno_pat_arb)\n"); }
void sm_pat_rem  (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_noarg_push(out, "rem()Lrt/SnoPat;");    if (IS_JS) fprintf(out, "rt.pat_rem(); ");   if (IS_WASM) fprintf(out, "          (call $sno_pat_rem)\n"); }
void sm_pat_bal  (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_noarg_push(out, "bal()Lrt/SnoPat;");    if (IS_JS) fprintf(out, "rt.pat_bal(); ");   if (IS_WASM) fprintf(out, "          (call $sno_pat_bal)\n"); }
void sm_pat_fence0(const SM_Instr * instr, FILE * out){ (void)instr; if (IS_JVM) jvm_pat_noarg_push(out, "fence0()Lrt/SnoPat;"); if (IS_JS) fprintf(out, "rt.pat_fence(); ");  if (IS_WASM) fprintf(out, "          (call $sno_pat_fence)\n"); }
void sm_pat_abort(const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_noarg_push(out, "abort_()Lrt/SnoPat;"); if (IS_JS) fprintf(out, "rt.pat_abort(); ");  if (IS_WASM) fprintf(out, "          (call $sno_pat_abort)\n"); }
void sm_pat_fail (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_noarg_push(out, "fail_()Lrt/SnoPat;");  if (IS_JS) fprintf(out, "rt.pat_fail(); ");  if (IS_WASM) fprintf(out, "          (call $sno_pat_fail)\n"); }
void sm_pat_succeed(const SM_Instr * instr, FILE * out){(void)instr; if (IS_JVM) jvm_pat_noarg_push(out, "succeed_()Lrt/SnoPat;");if (IS_JS) fprintf(out, "rt.pat_succeed(); ");if (IS_WASM) fprintf(out, "          (call $sno_pat_succeed)\n"); }
void sm_pat_eps  (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_noarg_push(out, "eps()Lrt/SnoPat;");    if (IS_JS) fprintf(out, "rt.pat_eps(); ");   if (IS_WASM) fprintf(out, "          (call $sno_pat_eps)\n"); }
void sm_pat_deref(const SM_Instr * instr, FILE * out) {
    (void)instr;
    if (IS_JVM)  { fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n    invokestatic rt/SnoPat/deref(Ljava/lang/Object;)Lrt/SnoPat;\n    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return; }
    if (IS_JS)   { fprintf(out, "rt.pat_deref(); "); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_deref)\n"); return; }
}
void sm_pat_arbno (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_pat_push(out, "arbno(Lrt/SnoPat;)Lrt/SnoPat;");  if (IS_JS) fprintf(out, "rt.pat_arbno(); "); if (IS_WASM) fprintf(out, "          (call $sno_pat_arbno)\n"); }
void sm_pat_fence1(const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_pat_push(out, "fence1(Lrt/SnoPat;)Lrt/SnoPat;");  if (IS_WASM) fprintf(out, "          (call $sno_pat_fence)\n"); }
void sm_pat_cat   (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_2pat_push(out, "cat(Lrt/SnoPat;Lrt/SnoPat;)Lrt/SnoPat;"); if (IS_JS) fprintf(out, "rt.pat_cat(); "); if (IS_WASM) fprintf(out, "          (call $sno_pat_cat)\n"); }
void sm_pat_alt   (const SM_Instr * instr, FILE * out) { (void)instr; if (IS_JVM) jvm_pat_2pat_push(out, "alt(Lrt/SnoPat;Lrt/SnoPat;)Lrt/SnoPat;"); if (IS_JS) fprintf(out, "rt.pat_alt(); "); if (IS_WASM) fprintf(out, "          (call $sno_pat_alt)\n"); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_refname(const SM_Instr * instr, FILE * out) {
    const char * s = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM)  { jvm_emit_ldc_string(out, s); fprintf(out, "    invokestatic rt/SnoPat/refname(Ljava/lang/String;)Lrt/SnoPat;\n    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return; }
    if (IS_JS)   { fprintf(out, "rt.pat_refname("); js_escape(out, s); fprintf(out, "); "); return; }
    if (IS_WASM) { int addr = wasm_intern_name(s); fprintf(out, "          (call $sno_pat_refname (i32.const 0x%x) (i32.const %lld))\n", addr, (long long)instr->a[1].i); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_capture(const SM_Instr * instr, FILE * out) {
    const char * s = instr->a[0].s ? instr->a[0].s : ""; int kind = (int)instr->a[1].i;
    if (IS_JVM) {
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        jvm_emit_ldc_string(out, s); jvm_push_int2(out, kind);
        fprintf(out, "    invokestatic rt/SnoPat/capture(Lrt/SnoPat;Ljava/lang/String;I)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JS)   { fprintf(out, "rt.pat_capture("); js_escape(out, s); fprintf(out, ", %d); ", kind); return; }
    if (IS_WASM) { int addr = wasm_intern_name(s); fprintf(out, "          (call $sno_pat_capture (i32.const 0x%x) (i32.const %lld) (i32.const %lld))\n", addr, (long long)instr->a[1].i, (long long)instr->a[2].i); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_capture_fn(const SM_Instr * instr, FILE * out) {
    const char * fname = instr->a[0].s ? instr->a[0].s : ""; const char * namelist = instr->a[2].s ? instr->a[2].s : "";
    if (IS_JVM) {
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n");
        jvm_emit_ldc_string(out, fname); jvm_emit_ldc_string(out, namelist);
        fprintf(out, "    invokestatic rt/SnoPat/captureFn(Lrt/SnoPat;Ljava/lang/String;Ljava/lang/String;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JS)   { fprintf(out, "rt.pat_capture_fn("); js_escape(out, fname); fprintf(out, ", %lld, ", instr->a[1].i); js_escape(out, namelist); fprintf(out, "); "); return; }
    if (IS_WASM) { fprintf(out, "          ;; SM_PAT_CAPTURE_FN not yet implemented\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_capture_fn_args(const SM_Instr * instr, FILE * out) {
    const char * fname = instr->a[0].s ? instr->a[0].s : ""; int nargs = (int)instr->a[2].i;
    if (IS_JVM) {
        fprintf(out, "    bipush %d\n    anewarray java/lang/Object\n", nargs);
        for (int k = nargs - 1; k >= 0; k--) { fprintf(out, "    dup\n    bipush %d\n", k); fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n    aastore\n"); }
        fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n");
        fprintf(out, "    invokestatic rt/SnoRt/coerce_to_pat(Ljava/lang/Object;)Lrt/SnoPat;\n    swap\n");
        jvm_emit_ldc_string(out, fname);
        fprintf(out, "    swap\n    invokestatic rt/SnoPat/captureFnArgs(Lrt/SnoPat;Ljava/lang/String;[Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JS)   { fprintf(out, "rt.pat_capture_fn_args("); js_escape(out, fname); fprintf(out, ", %lld, %lld); ", instr->a[1].i, (long long)nargs); return; }
    if (IS_WASM) { fprintf(out, "          ;; SM_PAT_CAPTURE_FN_ARGS not yet implemented\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_usercall(const SM_Instr * instr, FILE * out) {
    const char * fname = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM)  { jvm_emit_ldc_string(out, fname); fprintf(out, "    invokestatic rt/SnoPat/usercall(Ljava/lang/String;)Lrt/SnoPat;\n    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return; }
    if (IS_JS)   { fprintf(out, "rt.pat_usercall("); js_escape(out, fname); fprintf(out, "); "); return; }
    if (IS_WASM) { fprintf(out, "          ;; SM_PAT_USERCALL not yet implemented\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_usercall_args(const SM_Instr * instr, FILE * out) {
    const char * fname = instr->a[0].s ? instr->a[0].s : ""; int nargs = (int)instr->a[1].i;
    if (IS_JVM) {
        fprintf(out, "    bipush %d\n    anewarray java/lang/Object\n", nargs);
        for (int k = nargs - 1; k >= 0; k--) { fprintf(out, "    dup\n    bipush %d\n", k); fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n    aastore\n"); }
        jvm_emit_ldc_string(out, fname);
        fprintf(out, "    swap\n    invokestatic rt/SnoPat/usercallArgs(Ljava/lang/String;[Ljava/lang/Object;)Lrt/SnoPat;\n");
        fprintf(out, "    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return;
    }
    if (IS_JS)   { fprintf(out, "rt.pat_usercall_args("); js_escape(out, fname); fprintf(out, ", %d); ", nargs); return; }
    if (IS_WASM) { fprintf(out, "          ;; SM_PAT_USERCALL_ARGS not yet implemented\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_exec_stmt(const SM_Instr * instr, FILE * out) {
    if (IS_JS)   { fprintf(out, "rt.exec_stmt("); js_escape(out, instr->a[0].s ? instr->a[0].s : ""); fprintf(out, ", %lld); ", instr->a[1].i); return; }
    if (IS_WASM) { fprintf(out, "          (call $sno_exec_stmt (i32.const 0) (i32.const 0) (i32.const 0))\n"); return; }
}
