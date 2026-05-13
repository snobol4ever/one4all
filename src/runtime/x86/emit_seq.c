/* emit_seq.c — RW-2: compound BB helper sequences, new names.
 *
 * No if-statements in any body — only insn_* / emit_jmp calls.
 * Mode dispatch lives entirely inside insn_* and emit_jmp.
 * RW-6: emit_bb_seq.c deleted (content was already in emit_seq.c).
 * EM-SNOCONE-PREP: bb3c_format → emit_text_3col throughout.
 */

#include "emit_seq.h"
#include "insn.h"
#include "emit_mode.h"
#include "emit_text.h"
#include <stdio.h>
#include <string.h>

/*========================================================================*/
/* Frame / prologue / epilogue                                            */
/*========================================================================*/

void emit_seq_frame_enter(void) {
    insn_push_rbp();
    insn_mov_rbp_rsp();
    insn_sub_rsp_i8(8);
}

void emit_seq_frame_leave(void) {
    insn_mov_rsp_rbp();
    insn_pop_rbp();
    insn_ret();
}

void emit_seq_brokered_enter(void) {
    insn_push_rbp();
    insn_mov_rbp_rsp();
}

void emit_seq_brokered_leave(int result) {
    insn_mov_eax_i32((uint32_t)result);
    insn_pop_rbp();
    insn_ret();
}

/*========================================================================*/
/* Register-load (lea rXX,[rip+sym] / movabs rXX,ptr)                    */
/*========================================================================*/

void emit_seq_lea_rdi_sym(const char *sym, uint64_t ptr) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF) {
            emit_text_3col(emit_outf(), "", "lea", "rdi, [rip + \\lbl]");
            return;
        }
        char args[80]; snprintf(args, sizeof(args), "rdi, [rip + %s]", sym ? sym : "??");
        emit_text_3col(emit_outf(), "", "lea", args);
        return;
    }
    insn_mov_rdi_i64(ptr);
}

void emit_seq_lea_rsi_sym(const char *sym, uint64_t ptr) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF) {
            emit_text_3col(emit_outf(), "", "lea", "rcx, [rip + \\namelist_lbl]");
            emit_text_3col(emit_outf(), "", "mov", "rsi, rcx");
            return;
        }
        char args[80]; snprintf(args, sizeof(args), "rcx, [rip + %s]", sym ? sym : "??");
        if (emit_bb_is_format_mode()) {
            fmt_body_append("lea", args);
            fmt_body_append("mov", "rsi, rcx");
            return;
        }
        emit_text_3col(emit_outf(), "", "lea", args);
        emit_text_3col(emit_outf(), "", "mov", "rsi, rcx");
        return;
    }
    insn_mov_rsi_i64(ptr);
}

void emit_seq_lea_rdx_sym(const char *sym, uint64_t ptr) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF) {
            emit_text_3col(emit_outf(), "", "lea", "rdx, [rip + \\namelist_lbl]");
            return;
        }
        char args[80]; snprintf(args, sizeof(args), "rdx, [rip + %s]", sym ? sym : "??");
        emit_text_3col(emit_outf(), "", "lea", args);
        return;
    }
    insn_mov_rdx_i64(ptr);
}

void emit_seq_movabs_rdi(uint64_t ptr) {
    insn_mov_rdi_i64(ptr);
}

/*========================================================================*/
/* Immediate-to-register                                                  */
/*========================================================================*/

void emit_seq_mov_edx_i32(int val) {
    if (bb_emit_mode == EMIT_MACRO_DEF) {
        emit_text_3col(emit_outf(), "", "mov", "edx, \\nargs");
        return;
    }
    insn_mov_edx_i32((uint32_t)val);
}

void emit_seq_mov_edi_i32(int val) {
    if (bb_emit_mode == EMIT_MACRO_DEF) {
        emit_text_3col(emit_outf(), "", "mov", "edi, \\kind");
        return;
    }
    insn_mov_edi_i32((uint32_t)val);
}

/*========================================================================*/
/* Cursor / r13                                                           */
/*========================================================================*/

void emit_seq_inc_r13(uint8_t disp) {
    insn_inc_r13_disp8(disp);
}

/*========================================================================*/
/* Cursor / bounds                                                        */
/*========================================================================*/

void emit_seq_cmp_delta_i(int n, bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    insn_mov_eax_r10mem();
    insn_cmp_eax_i32((uint32_t)n);
    emit_jmp(lbl_fail, JMP_JNE);
    emit_jmp(lbl_succ, JMP_JMP);
}

void emit_seq_cmp_siglen_delta(int n, uint64_t siglen_addr,
                               bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    insn_mov_rcx_i64(siglen_addr);
    insn_mov_eax_rcxmem();
    insn_sub_eax_i32((uint32_t)n);
    insn_mov_ecx_eax();
    insn_mov_eax_r10mem();
    insn_cmp_eax_ecx();
    emit_jmp(lbl_fail, JMP_JNE);
    emit_jmp(lbl_succ, JMP_JMP);
}

void emit_seq_sigma_delta_rdi(uint64_t sigma_addr, uint64_t siglen_addr) {
    (void)siglen_addr;
    insn_mov_rcx_i64(sigma_addr);
    insn_mov_rax_rcxmem();
    insn_movsxd_rcx_r10mem();
    insn_lea_rax_rax_rcx();
    insn_mov_rdi_rax();
}

void emit_seq_bounds_len(int len, uint64_t siglen_addr, bb_label_t *lbl_fail) {
    insn_mov_eax_r10mem();
    insn_add_eax_i32((uint32_t)len);
    insn_mov_rcx_i64(siglen_addr);
    insn_cmp_eax_rcxmem();
    emit_jmp(lbl_fail, JMP_JG);
}

/*========================================================================*/
/* Return-skip / label                                                    */
/*========================================================================*/

void emit_seq_jz_retskip(int pc) {
    /* TEXT: jz .Lretskip_N  BIN: nop (retskip not needed in binary path) */
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF) {
            emit_text_3col(emit_outf(), "", "jz", ".Lretskip_\\pc\\()");
            return;
        }
        char args[40]; snprintf(args, sizeof(args), ".Lretskip_%d", pc);
        emit_text_3col(emit_outf(), "", "jz", args);
        return;
    }
    insn_nop();
}

void emit_seq_retskip_label(int pc) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF) {
            fprintf(emit_outf(), ".Lretskip_\\pc\\():\n");
            return;
        }
        fprintf(emit_outf(), ".Lretskip_%d:\n", pc);
    }
}

/*========================================================================*/
/* BB wiring                                                              */
/*========================================================================*/

void emit_seq_zeta_rdi(uint64_t ptr, const char *sym) {
    /* TEXT: lea rdi,[rip+sym]  BIN: movabs rdi,ptr */
    if (IS_TEXT) {
        char args[128]; snprintf(args, sizeof(args), "rdi, [rip + %s]", sym ? sym : "0");
        emit_text_3col(emit_outf(), "", "lea", args);
        return;
    }
    insn_mov_rdi_i64(ptr);
}

void emit_seq_dispatch_jne_jmp(bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    insn_test_rax_rax();
    emit_jmp(lbl_succ, JMP_JNE);
    emit_jmp(lbl_fail, JMP_JMP);
}

/*========================================================================*/
/* Call                                                                   */
/*========================================================================*/

void emit_seq_call_tgt(const char *sym_or_param) {
    if (!IS_TEXT) return;
    if (bb_emit_mode == EMIT_MACRO_DEF) {
        emit_text_3col(emit_outf(), "", "call", "\\tgt");
        return;
    }
    emit_text_3col(emit_outf(), "", "call", sym_or_param ? sym_or_param : "??");
}

void emit_seq_noop_macro(const char *macro_name) {
    if (IS_TEXT) emit_text_3col(emit_outf(), "", macro_name, "");
}

void emit_seq_port_call(uint64_t zeta_ptr, const char *fn_name, uint64_t fn_fallback,
                        int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode()) {
            char frag[128]; snprintf(frag, sizeof(frag), "call %s@PLT", fn_name ? fn_name : "??");
            fmt_body_append(frag, "");
            char jne[128]; snprintf(jne, sizeof(jne), "jne %s", lbl_succ->name);
            fmt_body_append(jne, "");
            emit_jmp(lbl_fail, JMP_JMP);
            return;
        }
        emit_text_3col(emit_outf(), "", "push", "r10");
        { char a[80]; snprintf(a,sizeof(a),"rdi, [rip + %s]", fn_name?fn_name:"??");
          emit_text_3col(emit_outf(),"","lea",a); }
        { char a[16]; snprintf(a,sizeof(a),"esi, %d",port);
          emit_text_3col(emit_outf(),"","mov",a); }
        { char a[80]; snprintf(a,sizeof(a),"%s@PLT",fn_name?fn_name:"??");
          emit_text_3col(emit_outf(),"","call",a); }
        emit_text_3col(emit_outf(), "", "pop", "r10");
        insn_test_rax_rax();
        emit_jmp(lbl_succ, JMP_JNE);
        emit_jmp(lbl_fail, JMP_JMP);
        return;
    }
    insn_push_r12();
    insn_mov_rdi_i64(zeta_ptr);
    insn_mov_esi_i32(port);
    insn_call_plt(fn_name, fn_fallback);
    insn_pop_r12();
    insn_test_rax_rax();
    emit_jmp(lbl_succ, JMP_JNE);
    emit_jmp(lbl_fail, JMP_JMP);
}

void emit_seq_port_call_rip(uint64_t zeta_ptr, const char *zeta_label,
                             const char *fn_name, uint64_t fn_fallback,
                             int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode()) {
            char frag[128]; snprintf(frag,sizeof(frag),"call %s@PLT",fn_name?fn_name:"??");
            fmt_body_append(frag,"");
            char jne[128]; snprintf(jne,sizeof(jne),"jne %s",lbl_succ->name);
            fmt_body_append(jne,"");
            emit_jmp(lbl_fail, JMP_JMP);
            return;
        }
        insn_push_r10();
        { char a[80]; snprintf(a,sizeof(a),"rdi, [rip + %s]",zeta_label?zeta_label:"??");
          emit_text_3col(emit_outf(),"","lea",a); }
        { char a[16]; snprintf(a,sizeof(a),"esi, %d",port);
          emit_text_3col(emit_outf(),"","mov",a); }
        { char a[80]; snprintf(a,sizeof(a),"%s@PLT",fn_name?fn_name:"??");
          emit_text_3col(emit_outf(),"","call",a); }
        insn_pop_r10();
        insn_test_rax_rax();
        emit_jmp(lbl_succ, JMP_JNE);
        emit_jmp(lbl_fail, JMP_JMP);
        return;
    }
    (void)zeta_label;
    insn_push_r10();
    insn_mov_rdi_i64(zeta_ptr);
    insn_mov_esi_i32(port);
    insn_call_plt(fn_name, fn_fallback);
    insn_pop_r10();
    insn_test_rax_rax();
    emit_jmp(lbl_succ, JMP_JNE);
    emit_jmp(lbl_fail, JMP_JMP);
}
