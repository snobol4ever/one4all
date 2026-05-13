/* emit_bb_seq.c — L3: compound BB helper sequences.
 *
 * Combines L0–L2 primitives into multi-instruction sequences used by
 * BB box templates (L4).  Each function handles all four emit modes.
 * EM-RAW-PURGE: all raw bb_emit_byte calls replaced with bb_insn_* calls.
 */

#include "emit_bb_seq.h"
#include "emit_defs.h"
#include "emit_buf.h"
#include "emit_form.h"
#include "emit_text3c.h"
#include "emit_insn.h"
#include "emit_mode.h"
#include "emit_label.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_inc_mem_r13_disp8(uint8_t disp)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_inc_r13_disp8(disp);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[64];
        snprintf(args, sizeof(args), "dword ptr [r13 + %u]", (unsigned)disp);
        bb3c_format(emit_outf(), "", "inc", args);
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_push_rbp_frame(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_push_rbp();
        bb_insn_mov_rbp_rsp();
        bb_insn_sub_rsp_imm8(8);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", "push", "rbp");
        bb3c_format(emit_outf(), "", "mov",  "rbp, rsp");
        bb3c_format(emit_outf(), "", "sub",  "rsp, 8");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_pop_rbp_frame_ret(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_mov_rsp_rbp();
        bb_insn_pop_rbp();
        emit_ret();
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", "mov", "rsp, rbp");
        bb3c_format(emit_outf(), "", "pop", "rbp");
        bb3c_format(emit_outf(), "", "ret", "");
        return;
    }
}

/* EM-BB-PURGE-1 / EDP-6 — C-ABI wrapper helpers for brokered blobs. */

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_brokered_prologue(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_BROKERED:
    case EMIT_BINARY_WIRED:
        bb_insn_push_rbp();
        bb_insn_mov_rbp_rsp();
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", "push", "rbp");
        bb3c_format(emit_outf(), "", "mov",  "rbp, rsp");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_brokered_epilogue_ret(int result)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_BROKERED:
    case EMIT_BINARY_WIRED:
        bb_insn_mov_eax_imm32((uint32_t)result);
        bb_insn_pop_rbp();
        emit_ret();
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        FILE *f = emit_outf();
        char arg[32]; snprintf(arg, sizeof(arg), "eax, %d", result);
        bb3c_format(f, "", "mov", arg);
        bb3c_format(f, "", "pop", "rbp");
        bb3c_format(f, "", "ret", "");
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_lea_rdi_strtab_sym(const char *sym_label, uint64_t in_proc_ptr)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        emit_mov_rdi_imm64(in_proc_ptr);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[80];
        snprintf(args, sizeof(args), "rdi, [rip + %s]",
                 sym_label ? sym_label : "??sym??");
        bb3c_format(emit_outf(), "", "lea", args);
        return;
    }
    case EMIT_MACRO_DEF: {
        bb3c_format(emit_outf(), "", "lea", "rdi, [rip + \\lbl]");
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_lea_rdx_strtab_sym(const char *sym_label, uint64_t in_proc_ptr)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_mov_rdx_imm64(in_proc_ptr);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[80];
        snprintf(args, sizeof(args), "rdx, [rip + %s]",
                 sym_label ? sym_label : "??sym??");
        bb3c_format(emit_outf(), "", "lea", args);
        return;
    }
    case EMIT_MACRO_DEF:
        bb3c_format(emit_outf(), "", "lea", "rdx, [rip + \\namelist_lbl]");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_mov_edx_imm32(int val)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_mov_edx_imm32((uint32_t)val);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[32];
        snprintf(args, sizeof(args), "edx, %d", val);
        bb3c_format(emit_outf(), "", "mov", args);
        return;
    }
    case EMIT_MACRO_DEF:
        bb3c_format(emit_outf(), "", "mov", "edx, \\nargs");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_mov_edi_imm32(int val)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_mov_edi_imm32((uint32_t)val);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[32];
        snprintf(args, sizeof(args), "edi, %d", val);
        bb3c_format(emit_outf(), "", "mov", args);
        return;
    }
    case EMIT_MACRO_DEF:
        bb3c_format(emit_outf(), "", "mov", "edi, \\kind");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_jz_retskip(int pc)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_nop();
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[40];
        snprintf(args, sizeof(args), ".Lretskip_%d", pc);
        bb3c_format(emit_outf(), "", "jz", args);
        return;
    }
    case EMIT_MACRO_DEF:
        bb3c_format(emit_outf(), "", "jz", ".Lretskip_\\pc\\()");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_retskip_label(int pc)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        FILE *f = emit_outf();
        fprintf(f, ".Lretskip_%d:\n", pc);
        return;
    }
    case EMIT_MACRO_DEF: {
        FILE *f = emit_outf();
        fputs(".Lretskip_\\pc\\():\n", f);
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_movabs_rdi_entry(uint64_t entry_ptr)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        emit_mov_rdi_imm64(entry_ptr);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[32];
        snprintf(args, sizeof(args), "rdi, 0x%llx", (unsigned long long)entry_ptr);
        bb3c_format(emit_outf(), "", "movabs", args);
        return;
    }
    case EMIT_MACRO_DEF:
        bb3c_format(emit_outf(), "", "movabs", "rdi, \\entry");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_call_sym_param(const char *sym_or_param)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        bb3c_format(emit_outf(), "", "call", sym_or_param ? sym_or_param : "??tgt??");
        return;
    }
    case EMIT_MACRO_DEF:
        bb3c_format(emit_outf(), "", "call", "\\tgt");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_noop_macro(const char *macro_name)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", macro_name, "");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_port_call(uint64_t zeta_ptr, const char *fn_name, uint64_t fn_fallback,
                    int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail)
{
    if (emit_bb_is_format_mode()) {
        char call_frag[BB_LABEL_NAME_MAX + 32];
        snprintf(call_frag, sizeof(call_frag), "call %s@PLT", fn_name);
        fmt_body_append(call_frag, "");
        char jne[BB_LABEL_NAME_MAX + 8];
        snprintf(jne, sizeof(jne), "jne %s", lbl_succ->name);
        fmt_body_append(jne, "");
        emit_jmp(lbl_fail, JMP_JMP);
        return;
    }
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_push_r12();
        break;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", "push", "r10");
        break;
    }
    emit_mov_rdi_imm64(zeta_ptr);
    emit_mov_esi_imm32(port);
    emit_call_sym_plt(fn_name, fn_fallback);
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_pop_r12();
        break;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", "pop", "r10");
        break;
    }
    emit_test_rax_rax();
    emit_jmp(lbl_succ, JMP_JNE);
    emit_jmp(lbl_fail, JMP_JMP);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_port_call_rip(uint64_t zeta_ptr, const char *zeta_label,
                        const char *fn_name, uint64_t fn_fallback,
                        int port, bb_label_t *lbl_succ, bb_label_t *lbl_fail)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        emit_bb_port_call(zeta_ptr, fn_name, fn_fallback, port, lbl_succ, lbl_fail);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        break;
    }
    if (emit_bb_is_format_mode()) {
        char call_frag[BB_LABEL_NAME_MAX + 32];
        snprintf(call_frag, sizeof(call_frag), "call %s@PLT", fn_name ? fn_name : "??fn??");
        fmt_body_append(call_frag, "");
        char jne[BB_LABEL_NAME_MAX + 8];
        snprintf(jne, sizeof(jne), "jne %s", lbl_succ->name);
        fmt_body_append(jne, "");
        emit_jmp(lbl_fail, JMP_JMP);
        return;
    }
    FILE *f = emit_outf();
    bb3c_format(f, "", "push", "r10");
    { char args[80]; snprintf(args, sizeof(args), "rdi, [rip + %s]", zeta_label ? zeta_label : "??zeta??");
      bb3c_format(f, "", "lea", args); }
    { char args[16]; snprintf(args, sizeof(args), "esi, %d", port);
      bb3c_format(f, "", "mov", args); }
    { char sym[80]; snprintf(sym, sizeof(sym), "%s@PLT", fn_name ? fn_name : "??fn??");
      bb3c_format(f, "", "call", sym); }
    bb3c_format(f, "", "pop", "r10");
    bb3c_format(f, "", "test", "rax, rax");
    emit_jmp(lbl_succ, JMP_JNE);
    emit_jmp(lbl_fail, JMP_JMP);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_load_delta_cmp_imm(int n, bb_label_t *lbl_succ, bb_label_t *lbl_fail)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_mov_eax_r10mem();
        bb_insn_cmp_eax_imm32((uint32_t)n);
        emit_jmp(lbl_fail, JMP_JNE);
        emit_jmp(lbl_succ, JMP_JMP);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) {
            char args[32]; snprintf(args, sizeof(args), "eax, %d", n);
            fmt_body_append("mov", "eax, [r10]");
            fmt_body_append("cmp", args);
            char jne[BB_LABEL_NAME_MAX + 8];
            snprintf(jne, sizeof(jne), "jne %s", lbl_fail->name);
            fmt_body_append(jne, "");
            emit_jmp(lbl_succ, JMP_JMP);
            return;
        }
        FILE *f = emit_outf();
        bb3c_format(f, "", "mov", "eax, [r10]");
        char args[32]; snprintf(args, sizeof(args), "eax, %d", n);
        bb3c_format(f, "", "cmp", args);
        emit_jmp(lbl_fail, JMP_JNE);
        emit_jmp(lbl_succ, JMP_JMP);
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_load_siglen_sub_cmp_delta(int n, uint64_t siglen_addr,
                                 bb_label_t *lbl_succ, bb_label_t *lbl_fail)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_mov_rcx_imm64(siglen_addr);
        bb_insn_mov_eax_mem_rcx();
        bb_insn_sub_eax_imm32((uint32_t)n);
        bb_insn_mov_ecx_eax();
        bb_insn_mov_eax_r10mem();
        bb_insn_cmp_eax_ecx();
        emit_jmp(lbl_fail, JMP_JNE);
        emit_jmp(lbl_succ, JMP_JMP);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) {
            char args[32]; snprintf(args, sizeof(args), "eax, %d", n);
            fmt_body_append("lea", "rcx, [rip + \xCE\xA3len]");
            fmt_body_append("mov", "eax, [rcx]");
            fmt_body_append("sub", args);
            fmt_body_append("mov", "ecx, eax");
            fmt_body_append("mov", "eax, [r10]");
            fmt_body_append("cmp", "eax, ecx");
            char jne[BB_LABEL_NAME_MAX + 8];
            snprintf(jne, sizeof(jne), "jne %s", lbl_fail->name);
            fmt_body_append(jne, "");
            emit_jmp(lbl_succ, JMP_JMP);
            return;
        }
        FILE *f = emit_outf();
        char args[64];
        bb3c_format(f, "", "lea", "rcx, [rip + \xCE\xA3len]");
        bb3c_format(f, "", "mov", "eax, [rcx]");
        snprintf(args, sizeof(args), "eax, %d", n);
        bb3c_format(f, "", "sub", args);
        bb3c_format(f, "", "mov", "ecx, eax");
        bb3c_format(f, "", "mov", "eax, [r10]");
        bb3c_format(f, "", "cmp", "eax, ecx");
        emit_jmp(lbl_fail, JMP_JNE);
        emit_jmp(lbl_succ, JMP_JMP);
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_lea_rsi_strtab_sym(const char *sym_label, uint64_t in_proc_ptr)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_mov_rsi_imm64(in_proc_ptr);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[80];
        snprintf(args, sizeof(args), "rcx, [rip + %s]",
                 sym_label ? sym_label : "??sym??");
        if (emit_bb_is_format_mode()) {
            fmt_body_append("lea", args);
            fmt_body_append("mov", "rsi, rcx");
            return;
        }
        FILE *f = emit_outf();
        bb3c_format(f, "", "lea", args);
        bb3c_format(f, "", "mov", "rsi, rcx");
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_sigma_plus_delta_to_rdi(uint64_t sigma_addr, uint64_t siglen_addr)
{
    (void)siglen_addr;
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_mov_rcx_imm64(sigma_addr);
        bb_insn_mov_rax_mem_rcx();
        bb_insn_movsxd_rcx_r10mem();
        bb_insn_lea_rax_rax_rcx();
        bb_insn_mov_rdi_rax();
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) {
            fmt_body_append("lea", "rcx, [rip + \xCE\xA3]");
            fmt_body_append("mov", "rax, [rcx]");
            fmt_body_append("movsxd", "rcx, [r10]");
            fmt_body_append("lea", "rax, [rax+rcx]");
            fmt_body_append("mov", "rdi, rax");
            return;
        }
        FILE *f = emit_outf();
        bb3c_format(f, "", "lea", "rcx, [rip + \xCE\xA3]");
        bb3c_format(f, "", "mov", "rax, [rcx]");
        bb3c_format(f, "", "movsxd", "rcx, [r10]");
        bb3c_format(f, "", "lea", "rax, [rax+rcx]");
        bb3c_format(f, "", "mov", "rdi, rax");
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bounds_check_delta_plus_len(int len, uint64_t siglen_addr, bb_label_t *lbl_fail)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_insn_mov_eax_r10mem();
        bb_insn_add_eax_imm32((uint32_t)len);
        bb_insn_mov_rcx_imm64(siglen_addr);
        bb_insn_cmp_eax_mem_rcx();
        emit_jmp(lbl_fail, JMP_JG);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) {
            char add_args[32]; snprintf(add_args, sizeof(add_args), "eax, %d", len);
            fmt_body_append("mov", "eax, [r10]");
            fmt_body_append("add", add_args);
            fmt_body_append("lea", "rcx, [rip + \xCE\xA3len]");
            fmt_body_append("cmp", "eax, [rcx]");
            char jg[BB_LABEL_NAME_MAX + 8];
            snprintf(jg, sizeof(jg), "jg %s", lbl_fail->name);
            fmt_body_append(jg, "");
            return;
        }
        FILE *f = emit_outf();
        bb3c_format(f, "", "mov", "eax, [r10]");
        char args[32]; snprintf(args, sizeof(args), "eax, %d", len);
        bb3c_format(f, "", "add", args);
        bb3c_format(f, "", "lea", "rcx, [rip + \xCE\xA3len]");
        bb3c_format(f, "", "cmp", "eax, [rcx]");
        emit_jmp(lbl_fail, JMP_JG);
        return;
    }
    }
}
