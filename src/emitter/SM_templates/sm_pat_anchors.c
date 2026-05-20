#include "sm_template_common.h"
#include "emit_sm.h"
/* EC-UNI-8.3-fixup: NET PAT is a known stub (per goal), so every IS_NET_* cell
 * carries an explicit n/a sentinel.  Future EC-UNI work may wire NET pattern
 * emission; for now the gate accepts these as documented absences. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_lit(const SM_t * instr, FILE * out) {
    if (IS_X86) { emit_sm_pat_lit_template(out, instr); return; }
    const char * s = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM) { jvm_emit_ldc_string(out, s); fprintf(out, "    invokestatic rt/SnoPat/lit(Ljava/lang/String;)Lrt/SnoPat;\n    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_lit("); js_escape(out, s); fprintf(out, "); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { int addr = wasm_intern_str(s); fprintf(out, "          (call $sno_pat_lit (i32.const 0x%x) (i32.const %d))\n", addr, (int)strlen(s)); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_any(const SM_t * instr, FILE * out) {
    if (IS_X86) { emit_sm_pat_any_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_str_push(out, 0, "any", "any(Ljava/lang/String;)Lrt/SnoPat;"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_any(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_any)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_any_i(const SM_t * instr, int i, FILE * out) {
    if (IS_X86) { emit_sm_pat_any_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_str_push(out, i, "any", "any(Ljava/lang/String;)Lrt/SnoPat;"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_any(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_any)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_notany(const SM_t * instr, int i, FILE * out) {
    if (IS_X86) { emit_sm_pat_notany_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_str_push(out, i, "nany", "notany(Ljava/lang/String;)Lrt/SnoPat;"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_notany(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_notany)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_span(const SM_t * instr, int i, FILE * out) {
    if (IS_X86) { emit_sm_pat_span_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_str_push(out, i, "span", "span(Ljava/lang/String;)Lrt/SnoPat;"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_span(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_span)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_break(const SM_t * instr, int i, FILE * out) {
    if (IS_X86) { emit_sm_pat_break_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_str_push(out, i, "brk", "brk(Ljava/lang/String;)Lrt/SnoPat;"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_break(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_break)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_refname(const SM_t * instr, FILE * out) {
    if (IS_X86) { emit_sm_pat_refname_template(out, instr); return; }
    const char * s = instr->a[0].s ? instr->a[0].s : "";
    if (IS_JVM) { jvm_emit_ldc_string(out, s); fprintf(out, "    invokestatic rt/SnoPat/refname(Ljava/lang/String;)Lrt/SnoPat;\n    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_refname("); js_escape(out, s); fprintf(out, "); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { int addr = wasm_intern_name(s); fprintf(out, "          (call $sno_pat_refname (i32.const 0x%x) (i32.const %lld))\n", addr, (long long)instr->a[1].i); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_deref(const SM_t * instr, FILE * out) {
    if (IS_X86) { emit_sm_pat_deref_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { fprintf(out, "    invokestatic rt/SnoRt/pop_obj()Ljava/lang/Object;\n    invokestatic rt/SnoPat/deref(Ljava/lang/Object;)Lrt/SnoPat;\n    invokestatic rt/SnoRt/push_obj(Ljava/lang/Object;)V\n"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_deref(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_deref)\n"); return; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_pat_arb(const SM_t * instr, FILE * out) {
    if (IS_X86) { emit_sm_pat_arb_dispatch(out, 0); return; }
    (void)instr;
    if (IS_JVM) { jvm_pat_noarg_push(out, "arb()Lrt/SnoPat;"); return; }
    if (IS_JS) { fprintf(out, "rt.pat_arb(); "); return; }
    /* IS_NET: n/a — NET PAT stub */
    if (IS_WASM) { fprintf(out, "          (call $sno_pat_arb)\n"); return; }
}
