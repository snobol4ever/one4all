#include "sm_template_common.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_return(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    if (IS_X86_TEXT) return emit_sm_return_template(out, instr);
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return 0; }
    int op = (int)instr->op, i = ctx->i;
    if (IS_JVM_TEXT) {
        jvm_ret_guard(SM_RETURN_S, SM_RETURN_F, op, i, "rs", out);
        jvm_push_int2(out, 0); jvm_push_int2(out, 1);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n    return\n");
        if (op == SM_RETURN_S) fprintf(out, "sm_pc_%d_rs_skip:\n", i);
        if (op == SM_RETURN_F) fprintf(out, "sm_pc_%d_rf_skip:\n", i);
        return 0;
    }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) {
        if (op == SM_RETURN)   { fprintf(out, "{ let _r = rt.fn_return(0, 0); if (_r === -2) { break loop; } _pc = _r; continue; } "); return 1; }
        if (op == SM_RETURN_S) { fprintf(out, "{ let _r = rt.fn_return(0, 1); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
        if (op == SM_RETURN_F) { fprintf(out, "{ let _r = rt.fn_return(0, 2); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
    }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_NET_TEXT) {
        int fk = (i >= 0 && i < ctx->n && ctx->pc_to_fn) ? ctx->pc_to_fn[i] : -1;
        const char * fname = (fk >= 0 && ctx->fn_names) ? ctx->fn_names[fk] : NULL;
        net_ret_guard(SM_RETURN_S, SM_RETURN_F, op, i, out);
        if (fname) { net_escape_ldstr(out, fname); fprintf(out, "    call       void SnoRt::push_var(string)\n"); }
        else fprintf(out, "    call       void SnoRt::push_null()\n");
        fprintf(out, "    call       void SnoRt::frame_exit()\n");
        net_push_i4(out, 0); net_push_i4(out, 1);
        fprintf(out, "    call       void SnoRt::do_return(int32, bool)\n    call       int32 SnoRt::pop_ret_pc()\n    stloc      _pc\n    br         NET_DISPATCH\n");
        return 1;
    }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }
    if (IS_WASM_TEXT) {
        if (op == SM_RETURN)   { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 0) (i32.const 0)))\n          (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))\n"); return 1; }
        if (op == SM_RETURN_S) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 0) (i32.const 1)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
        if (op == SM_RETURN_F) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 0) (i32.const 2)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
    }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_freturn(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    if (IS_X86_TEXT) return emit_sm_return_template(out, instr);
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return 0; }
    int op = (int)instr->op, i = ctx->i;
    if (IS_JVM_TEXT) {
        jvm_ret_guard(SM_FRETURN_S, SM_FRETURN_F, op, i, "fs", out);
        jvm_push_int2(out, 1); jvm_push_int2(out, 0);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n    return\n");
        if (op == SM_FRETURN_S) fprintf(out, "sm_pc_%d_fs_skip:\n", i);
        if (op == SM_FRETURN_F) fprintf(out, "sm_pc_%d_ff_skip:\n", i);
        return 0;
    }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) {
        if (op == SM_FRETURN)   { fprintf(out, "{ let _r = rt.fn_return(1, 0); if (_r === -2) { break loop; } _pc = _r; continue; } "); return 1; }
        if (op == SM_FRETURN_S) { fprintf(out, "{ let _r = rt.fn_return(1, 1); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
        if (op == SM_FRETURN_F) { fprintf(out, "{ let _r = rt.fn_return(1, 2); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
    }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_NET_TEXT) {
        net_ret_guard(SM_FRETURN_S, SM_FRETURN_F, op, i, out);
        fprintf(out, "    call       void SnoRt::push_null()\n    call       void SnoRt::frame_exit()\n");
        net_push_i4(out, 1); net_push_i4(out, 0);
        fprintf(out, "    call       void SnoRt::do_return(int32, bool)\n    call       int32 SnoRt::pop_ret_pc()\n    stloc      _pc\n    br         NET_DISPATCH\n");
        return 1;
    }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }
    if (IS_WASM_TEXT) {
        if (op == SM_FRETURN)   { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 1) (i32.const 0)))\n          (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))\n"); return 1; }
        if (op == SM_FRETURN_S) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 1) (i32.const 1)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
        if (op == SM_FRETURN_F) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 1) (i32.const 2)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
    }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sm_nreturn(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    if (IS_X86_TEXT) return emit_sm_return_template(out, instr);
    if (IS_X86_BIN)  { /* EC-UNI-6 owed: wired binary path; legacy emit_walk_codegen handles today */ return 0; }
    int op = (int)instr->op, i = ctx->i;
    if (IS_JVM_TEXT) {
        jvm_ret_guard(SM_NRETURN_S, SM_NRETURN_F, op, i, "ns", out);
        jvm_push_int2(out, 2); jvm_push_int2(out, 0);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n    return\n");
        if (op == SM_NRETURN_S) fprintf(out, "sm_pc_%d_ns_skip:\n", i);
        if (op == SM_NRETURN_F) fprintf(out, "sm_pc_%d_nf_skip:\n", i);
        return 0;
    }
    if (IS_JVM_BIN)  { /* EC-UNI-7 owed: binary .class bytes */ return; }
    if (IS_JS_TEXT) {
        if (op == SM_NRETURN)   { fprintf(out, "{ let _r = rt.fn_return(2, 0); if (_r === -2) { break loop; } _pc = _r; continue; } "); return 1; }
        if (op == SM_NRETURN_S) { fprintf(out, "{ let _r = rt.fn_return(2, 1); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
        if (op == SM_NRETURN_F) { fprintf(out, "{ let _r = rt.fn_return(2, 2); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
    }
    /* IS_JS_BIN: n/a — JS has no binary form */
    if (IS_NET_TEXT) {
        int fk = (i >= 0 && i < ctx->n && ctx->pc_to_fn) ? ctx->pc_to_fn[i] : -1;
        const char * fname = (fk >= 0 && ctx->fn_names) ? ctx->fn_names[fk] : NULL;
        net_ret_guard(SM_NRETURN_S, SM_NRETURN_F, op, i, out);
        if (fname) { net_escape_ldstr(out, fname); fprintf(out, "    call       void SnoRt::push_var(string)\n"); }
        else fprintf(out, "    call       void SnoRt::push_null()\n");
        fprintf(out, "    call       void SnoRt::frame_exit()\n");
        net_push_i4(out, 2); net_push_i4(out, 0);
        fprintf(out, "    call       void SnoRt::do_return(int32, bool)\n    call       int32 SnoRt::pop_ret_pc()\n    stloc      _pc\n    br         NET_DISPATCH\n");
        return 1;
    }
    if (IS_NET_BIN)  { /* EC-UNI-7 owed: binary .NET IL bytes */ return; }
    if (IS_WASM_TEXT) {
        if (op == SM_NRETURN)   { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 2) (i32.const 0)))\n          (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))\n"); return 1; }
        if (op == SM_NRETURN_S) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 2) (i32.const 1)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
        if (op == SM_NRETURN_F) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 2) (i32.const 2)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
    }
    if (IS_WASM_BIN) { /* EC-UNI-7 owed: binary WASM bytes */ return; }
    return 0;
}
