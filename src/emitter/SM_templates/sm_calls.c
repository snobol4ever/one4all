/* sm_calls.c — SM_CALL_FN and SM_SUSPEND_VALUE templates (EC-UNI-13(b)).
   Bodies are the verbatim union of per-backend arms pulled from:
     x86  : emit_sm.c::emit_sm_call_dispatch              (strtab_label + emit_sm_lbl_int32)
     JVM  : emit_core.c::emit_jvm_one_instr case SM_CALL_FN/SM_SUSPEND_VALUE
     JS   : emit_core.c::emit_js_from_sm   case SM_CALL_FN; case SM_SUSPEND_VALUE
     NET  : emit_core.c::emit_net_from_sm  case SM_SUSPEND_VALUE: case SM_CALL_FN:
     WASM : emit_core.c::emit_wasm_from_sm case SM_CALL_FN: case SM_SUSPEND_VALUE:
   No refactor.  No helper extraction.  Wrapping `if (IS_<BE>)` is the only addition.
   Return: 1 when the arm produced a terminal jump that consumes the per-iteration
   fallthrough (WASM has_jump / NET has_continue / JS has_continue semantics);
   0 otherwise.  Callers in silo walkers OR the bit into their has_jump/has_continue. */
#include "sm_template_common.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_call_fn(void) {
    const SM_t * instr = g_emit.instr;
    FILE *       out   = g_emit.out;
    int          i     = g_emit.i;
    int          n     = g_emit.n;
    const char ** fn_names = g_emit.fn_names;
    int          fn_count  = g_emit.fn_count;
    if (IS_X86) return emit_sm_call_dispatch(out, instr, i);
    if (IS_JVM) {
        const char * cname = instr->a[0].s ? instr->a[0].s : "";
        if (!cname[0]) {
            jvm_push_int2(out, 0); jvm_push_int2(out, 1);
            emit_textf("    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
            emit_textf("    invokestatic rt/SnoRt/fn_return_push()V\n    return\n");
            return 0;
        }
        int entry_pc = -1;
        for (int k = 0; k < fn_count; k++) if (fn_names[k] && strcmp(fn_names[k], cname) == 0) { entry_pc = k; break; }
        if (entry_pc >= 0) {
            char mname[256]; jvm_sanitize_name(mname, sizeof mname, cname);
            jvm_emit_ldc_string(out, cname); jvm_push_int2(out, (long)instr->a[1].i);
            emit_textf("    invokestatic rt/SnoRt/bind_params(Ljava/lang/String;I)V\n");
            emit_textf("    invokestatic Prog/sno_fn_%s()V\n", mname);
        } else {
            jvm_emit_ldc_string(out, cname); jvm_push_int2(out, (long)instr->a[1].i);
            emit_textf("    invokestatic rt/SnoRt/call(Ljava/lang/String;I)V\n");
        }
        return 0;
    }
    if (IS_JS) {
        emit_textf("{ let _r = rt.call_or_jump("); js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
        emit_textf(", %lld, %d); if (_r >= 0) { _pc = _r; continue; } } ", (long long)instr->a[1].i, i + 1);
        return 0;
    }
    if (IS_NET) {
        const char * cname = instr->a[0].s ? instr->a[0].s : "";
        const int *  fn_pcs = g_emit.fn_pcs;
        int entry_pc = -1;
        for (int k = 0; k < fn_count; k++) if (fn_names[k] && strcmp(fn_names[k], cname) == 0) { entry_pc = fn_pcs ? fn_pcs[k] : -1; break; }
        if (entry_pc >= 0) {
            net_push_i4(out, i + 1); emit_textf("    call       void SnoRt::push_ret_pc(int32)\n");
            net_push_i4(out, entry_pc); emit_textf("    stloc      _pc\n    br         NET_DISPATCH\n");
            return 1;
        } else if (cname[0] == '\0' && g_emit.pc_to_fn && i >= 0 && i < n && g_emit.pc_to_fn[i] >= 0) {
            int fk = g_emit.pc_to_fn[i]; const char * fname = (fk >= 0 && fk < fn_count) ? fn_names[fk] : NULL;
            if (fname) { net_escape_ldstr(out, fname); emit_textf("    call       void SnoRt::push_var(string)\n"); }
            else emit_textf("    call       void SnoRt::push_null()\n");
            emit_textf("    call       void SnoRt::frame_exit()\n");
            net_push_i4(out, 0); net_push_i4(out, 1);
            emit_textf("    call       void SnoRt::do_return(int32, bool)\n");
            emit_textf("    call       int32 SnoRt::pop_ret_pc()\n    stloc      _pc\n    br         NET_DISPATCH\n");
            return 1;
        } else {
            net_escape_ldstr(out, cname); net_push_i4(out, (int)instr->a[1].i);
            emit_textf("    call       void SnoRt::sno_call(string, int32)\n");
            return 0;
        }
    }
    if (IS_WASM) {
        const char * cname = instr->a[0].s; int nargs = (int)instr->a[1].i;
        if (cname && cname[0]) {
            WasmUserFn * fn = wasm_userfn_find(cname);
            if (fn) {
                int na = wasm_intern_name(fn->name); int nl = (int)strlen(fn->name);
                emit_textf("          (local.set $fr (call $sno_call_frame_push (i32.const %d) (i32.const 0x%x) (i32.const %d)))\n", i + 1, na, nl);
                emit_textf("          (call $sno_save_var (local.get $fr) (i32.const 0x%x) (i32.const %d))\n", na, nl);
                for (int k = 0; k < fn->nparams; k++) { int pa = wasm_intern_name(fn->params[k]); int pl = (int)strlen(fn->params[k]); emit_textf("          (call $sno_save_var (local.get $fr) (i32.const 0x%x) (i32.const %d))\n", pa, pl); }
                emit_textf("          (call $sno_clear_var (i32.const 0x%x) (i32.const %d))\n", na, nl);
                int nbind = (nargs < fn->nparams) ? nargs : fn->nparams;
                for (int k = nbind - 1; k >= 0; k--) { int pa = wasm_intern_name(fn->params[k]); int pl = (int)strlen(fn->params[k]); emit_textf("          (call $sno_set_var_from_tos (i32.const 0x%x) (i32.const %d))\n", pa, pl); }
                for (int k = fn->nparams; k < nargs; k++) emit_textf("          (call $sno_pop_to_null)\n");
                emit_textf("          (call $sno_call_frame_close)\n");
                emit_textf("          (i32.const %d) (local.set $pc) (br $lp)\n", fn->entry_pc);
                return 1;
            }
            int addr = wasm_intern_name(cname); int len = (int)strlen(cname);
            emit_textf("          (call $sno_call (i32.const 0x%x) (i32.const %d) (i32.const %d))\n", addr, len, nargs);
            return 0;
        } else {
            emit_textf("          (local.set $tmp (call $sno_fn_return (i32.const 0) (i32.const 0)))\n");
            emit_textf("          (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))\n");
            return 1;
        }
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_suspend_value(void) {
    const SM_t * instr = g_emit.instr;
    FILE *       out   = g_emit.out;
    int          i     = g_emit.i;
    int          n     = g_emit.n;
    const char ** fn_names = g_emit.fn_names;
    int          fn_count  = g_emit.fn_count;
    if (IS_X86) return emit_sm_call_dispatch(out, instr, i);
    if (IS_JVM) {
        const char * cname = instr->a[0].s ? instr->a[0].s : "";
        if (!cname[0]) {
            jvm_push_int2(out, 0); jvm_push_int2(out, 1);
            emit_textf("    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
            emit_textf("    invokestatic rt/SnoRt/fn_return_push()V\n    return\n");
            return 0;
        }
        int entry_pc = -1;
        for (int k = 0; k < fn_count; k++) if (fn_names[k] && strcmp(fn_names[k], cname) == 0) { entry_pc = k; break; }
        if (entry_pc >= 0) {
            char mname[256]; jvm_sanitize_name(mname, sizeof mname, cname);
            jvm_emit_ldc_string(out, cname); jvm_push_int2(out, (long)instr->a[1].i);
            emit_textf("    invokestatic rt/SnoRt/bind_params(Ljava/lang/String;I)V\n");
            emit_textf("    invokestatic Prog/sno_fn_%s()V\n", mname);
        } else {
            jvm_emit_ldc_string(out, cname); jvm_push_int2(out, (long)instr->a[1].i);
            emit_textf("    invokestatic rt/SnoRt/call(Ljava/lang/String;I)V\n");
        }
        return 0;
    }
    if (IS_JS) {
        emit_textf("{ let _r = rt.call_or_jump("); js_escape_string(out, instr->a[0].s ? instr->a[0].s : "");
        emit_textf(", %lld, %d); if (_r >= 0) { _pc = _r; continue; } } ", (long long)instr->a[1].i, i + 1);
        emit_textf("rt.set_last_ok(!rt._is_fail(rt._peek())); ");
        return 0;
    }
    if (IS_NET) {
        const char * cname = instr->a[0].s ? instr->a[0].s : "";
        const int *  fn_pcs = g_emit.fn_pcs;
        int entry_pc = -1;
        for (int k = 0; k < fn_count; k++) if (fn_names[k] && strcmp(fn_names[k], cname) == 0) { entry_pc = fn_pcs ? fn_pcs[k] : -1; break; }
        if (entry_pc >= 0) {
            net_push_i4(out, i + 1); emit_textf("    call       void SnoRt::push_ret_pc(int32)\n");
            net_push_i4(out, entry_pc); emit_textf("    stloc      _pc\n    br         NET_DISPATCH\n");
            return 1;
        } else if (cname[0] == '\0' && g_emit.pc_to_fn && i >= 0 && i < n && g_emit.pc_to_fn[i] >= 0) {
            int fk = g_emit.pc_to_fn[i]; const char * fname = (fk >= 0 && fk < fn_count) ? fn_names[fk] : NULL;
            if (fname) { net_escape_ldstr(out, fname); emit_textf("    call       void SnoRt::push_var(string)\n"); }
            else emit_textf("    call       void SnoRt::push_null()\n");
            emit_textf("    call       void SnoRt::frame_exit()\n");
            net_push_i4(out, 0); net_push_i4(out, 1);
            emit_textf("    call       void SnoRt::do_return(int32, bool)\n");
            emit_textf("    call       int32 SnoRt::pop_ret_pc()\n    stloc      _pc\n    br         NET_DISPATCH\n");
            return 1;
        } else {
            net_escape_ldstr(out, cname); net_push_i4(out, (int)instr->a[1].i);
            emit_textf("    call       void SnoRt::sno_call(string, int32)\n");
            return 0;
        }
    }
    if (IS_WASM) {
        const char * cname = instr->a[0].s; int nargs = (int)instr->a[1].i;
        if (cname && cname[0]) {
            WasmUserFn * fn = wasm_userfn_find(cname);
            if (fn) {
                int na = wasm_intern_name(fn->name); int nl = (int)strlen(fn->name);
                emit_textf("          (local.set $fr (call $sno_call_frame_push (i32.const %d) (i32.const 0x%x) (i32.const %d)))\n", i + 1, na, nl);
                emit_textf("          (call $sno_save_var (local.get $fr) (i32.const 0x%x) (i32.const %d))\n", na, nl);
                for (int k = 0; k < fn->nparams; k++) { int pa = wasm_intern_name(fn->params[k]); int pl = (int)strlen(fn->params[k]); emit_textf("          (call $sno_save_var (local.get $fr) (i32.const 0x%x) (i32.const %d))\n", pa, pl); }
                emit_textf("          (call $sno_clear_var (i32.const 0x%x) (i32.const %d))\n", na, nl);
                int nbind = (nargs < fn->nparams) ? nargs : fn->nparams;
                for (int k = nbind - 1; k >= 0; k--) { int pa = wasm_intern_name(fn->params[k]); int pl = (int)strlen(fn->params[k]); emit_textf("          (call $sno_set_var_from_tos (i32.const 0x%x) (i32.const %d))\n", pa, pl); }
                for (int k = fn->nparams; k < nargs; k++) emit_textf("          (call $sno_pop_to_null)\n");
                emit_textf("          (call $sno_call_frame_close)\n");
                emit_textf("          (i32.const %d) (local.set $pc) (br $lp)\n", fn->entry_pc);
                return 1;
            }
            int addr = wasm_intern_name(cname); int len = (int)strlen(cname);
            emit_textf("          (call $sno_call (i32.const 0x%x) (i32.const %d) (i32.const %d))\n", addr, len, nargs);
            return 0;
        } else {
            emit_textf("          (local.set $tmp (call $sno_fn_return (i32.const 0) (i32.const 0)))\n");
            emit_textf("          (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))\n");
            return 1;
        }
    }
    return 0;
}
