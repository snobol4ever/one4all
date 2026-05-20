#include "sm_template_common.h"
#include "emit_sm.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_jump — SM_JUMP: unconditional branch to target PC. */
int sm_jump(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    if (IS_X86_TEXT) return emit_sm_jump_line(out, instr, 0);
    int target = (int)instr->a[0].i;
    if (IS_JVM) {
        const char * end_lbl = ctx->in_body ? "sm_pc_body_end" : "sm_pc_fn_end";
        if (target >= 0 && target < ctx->n && ctx->in_my_method && ctx->in_my_method[target])
            fprintf(out, "    goto_w sm_pc_%d\n", target);
        else if (target >= 0 && target < ctx->n) {
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\n");
        } else fprintf(out, "    goto_w %s\n", end_lbl);
        return 0;
    }
    if (IS_JS)   { fprintf(out, "_pc = %lld; continue; ", (long long)instr->a[0].i); return 1; }
    if (IS_NET)  { fprintf(out, "    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", (long long)instr->a[0].i); return 1; }
    if (IS_WASM) { fprintf(out, "          (i32.const %lld) (local.set $pc) (br $lp)\n", (long long)instr->a[0].i); return 1; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_jump_s — SM_JUMP_S: branch to target if last_ok==true, else fall through. */
int sm_jump_s(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    if (IS_X86_TEXT) return emit_sm_jump_s_line(out, instr, 0);
    int target = (int)instr->a[0].i; int i = ctx->i;
    if (IS_JVM) {
        if (target >= 0 && target < ctx->n && ctx->in_my_method && ctx->in_my_method[target]) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifeq sm_pc_%d_skip\n", i);
            fprintf(out, "    goto_w sm_pc_%d\n", target);
            fprintf(out, "sm_pc_%d_skip:\n", i);
        } else if (target >= 0 && target < ctx->n) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifeq sm_pc_%d_skip\n", i);
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\nsm_pc_%d_skip:\n", i);
        }
        return 0;
    }
    if (IS_JS)   { fprintf(out, "if (rt.last_ok()) _pc = %lld; else _pc = %d; continue; ", (long long)instr->a[0].i, i + 1); return 1; }
    if (IS_NET)  { fprintf(out, "    call       bool SnoRt::last_ok()\n    brfalse    NET_L%d\n    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", i + 1, (long long)instr->a[0].i); return 1; }
    if (IS_WASM) { fprintf(out, "          (if (call $sno_last_ok)\n            (then (i32.const %lld) (local.set $pc))\n            (else (i32.const %d)   (local.set $pc)))\n          (br $lp)\n", (long long)instr->a[0].i, i + 1); return 1; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_jump_f — SM_JUMP_F: branch to target if last_ok==false, else fall through. */
int sm_jump_f(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    if (IS_X86_TEXT) return emit_sm_jump_f_line(out, instr, 0);
    int target = (int)instr->a[0].i; int i = ctx->i;
    if (IS_JVM) {
        if (target >= 0 && target < ctx->n && ctx->in_my_method && ctx->in_my_method[target]) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifne sm_pc_%d_skip\n", i);
            fprintf(out, "    goto_w sm_pc_%d\n", target);
            fprintf(out, "sm_pc_%d_skip:\n", i);
        } else if (target >= 0 && target < ctx->n) {
            fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n");
            fprintf(out, "    ifne sm_pc_%d_skip\n", i);
            fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n");
            fprintf(out, "    iconst_0\n    invokestatic java/lang/System/exit(I)V\n");
            fprintf(out, "    return\nsm_pc_%d_skip:\n", i);
        }
        return 0;
    }
    if (IS_JS)   { fprintf(out, "if (!rt.last_ok()) _pc = %lld; else _pc = %d; continue; ", (long long)instr->a[0].i, i + 1); return 1; }
    if (IS_NET)  { fprintf(out, "    call       bool SnoRt::last_ok()\n    brtrue     NET_L%d\n    ldc.i4     %lld\n    stloc      _pc\n    br         NET_DISPATCH\n", i + 1, (long long)instr->a[0].i); return 1; }
    if (IS_WASM) { fprintf(out, "          (if (i32.eqz (call $sno_last_ok))\n            (then (i32.const %lld) (local.set $pc))\n            (else (i32.const %d)   (local.set $pc)))\n          (br $lp)\n", (long long)instr->a[0].i, i + 1); return 1; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_halt — SM_HALT: end execution. */
int sm_halt(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    (void)instr;
    if (IS_X86_TEXT) return emit_halt_line(out, 0);
    if (IS_JVM) {
        const char * end_lbl = ctx->in_body ? "sm_pc_body_end" : "sm_pc_fn_end";
        fprintf(out, "    invokestatic rt/SnoRt/halt_tos()V\n    goto_w %s\n", end_lbl);
        return 0;
    }
    if (IS_JS)   { fprintf(out, "break loop; "); return 1; }
    if (IS_NET)  { fprintf(out, "    call       void SnoRt::halt_tos()\n    br         NET_DONE\n"); return 1; }
    if (IS_WASM) { fprintf(out, "          (call $sno_halt_tos)\n          (br $done)\n"); return 1; }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* jvm_ret_guard — emit JVM last_ok guard for _S/_F variant. */
static void jvm_ret_guard(int op_s, int op_f, int op, int i, const char * sfx, FILE * out) {
    if (op == op_s) { fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n    ifeq sm_pc_%d_%s_skip\n", i, sfx); }
    if (op == op_f) { fprintf(out, "    invokestatic rt/SnoRt/last_ok()Z\n    ifne sm_pc_%d_%s_skip\n", i, sfx); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* net_ret_guard — emit NET last_ok guard for _S/_F variant. */
static void net_ret_guard(int op_s, int op_f, int op, int i, FILE * out) {
    if (op == op_s) { fprintf(out, "    call       bool SnoRt::last_ok()\n    brfalse    NET_L%d\n", i + 1); }
    if (op == op_f) { fprintf(out, "    call       bool SnoRt::last_ok()\n    brtrue     NET_L%d\n",  i + 1); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_return — SM_RETURN/S/F: successful return from user function. */
int sm_return(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    if (IS_X86_TEXT) return emit_sm_return_template(out, instr);
    int op = (int)instr->op, i = ctx->i;
    if (IS_JVM) {
        jvm_ret_guard(SM_RETURN_S, SM_RETURN_F, op, i, "rs", out);
        jvm_push_int2(out, 0); jvm_push_int2(out, 1);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n    return\n");
        if (op == SM_RETURN_S) fprintf(out, "sm_pc_%d_rs_skip:\n", i);
        if (op == SM_RETURN_F) fprintf(out, "sm_pc_%d_rf_skip:\n", i);
        return 0;
    }
    if (IS_JS) {
        if (op == SM_RETURN)   { fprintf(out, "{ let _r = rt.fn_return(0, 0); if (_r === -2) { break loop; } _pc = _r; continue; } "); return 1; }
        if (op == SM_RETURN_S) { fprintf(out, "{ let _r = rt.fn_return(0, 1); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
        if (op == SM_RETURN_F) { fprintf(out, "{ let _r = rt.fn_return(0, 2); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
    }
    if (IS_NET) {
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
    if (IS_WASM) {
        if (op == SM_RETURN)   { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 0) (i32.const 0)))\n          (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))\n"); return 1; }
        if (op == SM_RETURN_S) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 0) (i32.const 1)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
        if (op == SM_RETURN_F) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 0) (i32.const 2)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_freturn — SM_FRETURN/S/F: failure return from user function. */
int sm_freturn(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    if (IS_X86_TEXT) return emit_sm_return_template(out, instr);
    int op = (int)instr->op, i = ctx->i;
    if (IS_JVM) {
        jvm_ret_guard(SM_FRETURN_S, SM_FRETURN_F, op, i, "fs", out);
        jvm_push_int2(out, 1); jvm_push_int2(out, 0);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n    return\n");
        if (op == SM_FRETURN_S) fprintf(out, "sm_pc_%d_fs_skip:\n", i);
        if (op == SM_FRETURN_F) fprintf(out, "sm_pc_%d_ff_skip:\n", i);
        return 0;
    }
    if (IS_JS) {
        if (op == SM_FRETURN)   { fprintf(out, "{ let _r = rt.fn_return(1, 0); if (_r === -2) { break loop; } _pc = _r; continue; } "); return 1; }
        if (op == SM_FRETURN_S) { fprintf(out, "{ let _r = rt.fn_return(1, 1); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
        if (op == SM_FRETURN_F) { fprintf(out, "{ let _r = rt.fn_return(1, 2); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
    }
    if (IS_NET) {
        net_ret_guard(SM_FRETURN_S, SM_FRETURN_F, op, i, out);
        fprintf(out, "    call       void SnoRt::push_null()\n    call       void SnoRt::frame_exit()\n");
        net_push_i4(out, 1); net_push_i4(out, 0);
        fprintf(out, "    call       void SnoRt::do_return(int32, bool)\n    call       int32 SnoRt::pop_ret_pc()\n    stloc      _pc\n    br         NET_DISPATCH\n");
        return 1;
    }
    if (IS_WASM) {
        if (op == SM_FRETURN)   { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 1) (i32.const 0)))\n          (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))\n"); return 1; }
        if (op == SM_FRETURN_S) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 1) (i32.const 1)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
        if (op == SM_FRETURN_F) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 1) (i32.const 2)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* sm_nreturn — SM_NRETURN/S/F: name-return from user function. */
int sm_nreturn(const SM_Instr * instr, const sm_ctx_t * ctx, FILE * out) {
    if (IS_X86_TEXT) return emit_sm_return_template(out, instr);
    int op = (int)instr->op, i = ctx->i;
    if (IS_JVM) {
        jvm_ret_guard(SM_NRETURN_S, SM_NRETURN_F, op, i, "ns", out);
        jvm_push_int2(out, 2); jvm_push_int2(out, 0);
        fprintf(out, "    invokestatic rt/SnoRt/do_return(II)I\n    pop\n");
        fprintf(out, "    invokestatic rt/SnoRt/fn_return_push()V\n    return\n");
        if (op == SM_NRETURN_S) fprintf(out, "sm_pc_%d_ns_skip:\n", i);
        if (op == SM_NRETURN_F) fprintf(out, "sm_pc_%d_nf_skip:\n", i);
        return 0;
    }
    if (IS_JS) {
        if (op == SM_NRETURN)   { fprintf(out, "{ let _r = rt.fn_return(2, 0); if (_r === -2) { break loop; } _pc = _r; continue; } "); return 1; }
        if (op == SM_NRETURN_S) { fprintf(out, "{ let _r = rt.fn_return(2, 1); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
        if (op == SM_NRETURN_F) { fprintf(out, "{ let _r = rt.fn_return(2, 2); if (_r === -1) { _pc = %d; continue; } if (_r === -2) { break loop; } _pc = _r; continue; } ", i + 1); return 1; }
    }
    if (IS_NET) {
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
    if (IS_WASM) {
        if (op == SM_NRETURN)   { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 2) (i32.const 0)))\n          (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))\n"); return 1; }
        if (op == SM_NRETURN_S) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 2) (i32.const 1)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
        if (op == SM_NRETURN_F) { fprintf(out, "          (local.set $tmp (call $sno_fn_return (i32.const 2) (i32.const 2)))\n          (if (i32.eq (local.get $tmp) (i32.const -1)) (then (i32.const %d) (local.set $pc) (br $lp)) (else (if (i32.eq (local.get $tmp) (i32.const -2)) (then (br $done)) (else (local.set $pc (local.get $tmp)) (br $lp)))))\n", i + 1); return 1; }
    }
    return 0;
}
