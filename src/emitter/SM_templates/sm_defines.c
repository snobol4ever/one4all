/* sm_defines.c — SM_DEFINE_ENTRY and SM_DEFINE templates (EC-UNI-13(c)).
   Bodies are the verbatim union of per-backend arms pulled from:
     x86  : emit_sm.c::emit_sm_define_entry_dispatch + emit_sm_define_dispatch
            (noop+annotation; SM_DEFINE_ENTRY additionally emits `push rbp / mov rbp,rsp`
             and sets g_in_define_body = 1)
     JVM  : emit_core.c::emit_jvm_one_instr  case SM_DEFINE_ENTRY: case SM_DEFINE: break;
     JS   : emit_core.c::emit_js_from_sm     case SM_DEFINE_ENTRY: break;  (SM_DEFINE falls
            through to `default: break;` — both arms are no-ops in JS)
     NET  : emit_core.c::emit_net_from_sm    case SM_DEFINE_ENTRY: case SM_DEFINE:
            case SM_EXEC_STMT: { ... }  — shared heavy block (pop/pop/pop, push_var subj,
            set_omega / set_sigma / set_delta / set_last_ok = 0)
     WASM : emit_core.c::emit_wasm_from_sm   case SM_EXEC_STMT: case SM_DEFINE_ENTRY:
            case SM_DEFINE: sm_exec_stmt(); break;  — routes to sm_exec_stmt() template
   No refactor.  No helper extraction.  Wrapping `if (IS_<BE>)` is the only addition.
   Return: 0 in every arm (no terminal jump consumed); convention reserved (matches
   sm_call_fn signature). */
#include "sm_template_common.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_define_entry(void) {
    const SM_t *               instr = g_emit.instr;
    FILE *                     out   = g_emit.out;
    int                        i     = g_emit.i;
    const SM_sequence_t *      prog  = g_emit.prog;
    (void)i;
    if (IS_X86) { emit_sm_define_entry_dispatch(out, instr, g_emit.i, prog); return 0; }
    if (IS_JVM) { return 0; }
    if (IS_JS)  { return 0; }
    if (IS_NET) {
        int has_repl = (int)instr->a[1].i; const char * subj_name = instr->a[0].s ? instr->a[0].s : "";
        if (has_repl) emit_textf("    pop\n");
        emit_textf("    pop\n    pop\n");
        net_escape_ldstr(out, subj_name);
        emit_textf("    call       void SnoRt::push_var(string)\n    dup\n");
        emit_textf("    call       int32 [mscorlib]System.String::get_Length()\n");
        emit_textf("    call       void SnoRt::set_omega(int32)\n    call       void SnoRt::set_sigma(string)\n");
        emit_textf("    ldc.i4.0\n    call       void SnoRt::set_delta(int32)\n");
        emit_textf("    ldc.i4.0\n    call       void SnoRt::set_last_ok(bool)\n");
        return 0;
    }
    if (IS_WASM) { sm_exec_stmt(); return 0; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_define(void) {
    const SM_t *               instr = g_emit.instr;
    FILE *                     out   = g_emit.out;
    int                        i     = g_emit.i;
    (void)i;
    if (IS_X86) { emit_sm_define_dispatch(out, instr, g_emit.i); return 0; }
    if (IS_JVM) { return 0; }
    if (IS_JS)  { return 0; }
    if (IS_NET) {
        int has_repl = (int)instr->a[1].i; const char * subj_name = instr->a[0].s ? instr->a[0].s : "";
        if (has_repl) emit_textf("    pop\n");
        emit_textf("    pop\n    pop\n");
        net_escape_ldstr(out, subj_name);
        emit_textf("    call       void SnoRt::push_var(string)\n    dup\n");
        emit_textf("    call       int32 [mscorlib]System.String::get_Length()\n");
        emit_textf("    call       void SnoRt::set_omega(int32)\n    call       void SnoRt::set_sigma(string)\n");
        emit_textf("    ldc.i4.0\n    call       void SnoRt::set_delta(int32)\n");
        emit_textf("    ldc.i4.0\n    call       void SnoRt::set_last_ok(bool)\n");
        return 0;
    }
    if (IS_WASM) { sm_exec_stmt(); return 0; }
    return 0;
}
