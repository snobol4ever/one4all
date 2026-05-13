
#include "emit_bb_gen.h"
#include "emit_label.h"
#include "emit_text3c.h"
#include "emit_buf.h"
#include "emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

bb_emit_mode_t  bb_emit_mode = EMIT_TEXT;
FILE           *bb_emit_out  = NULL;


int g_bb_emit_format = 0;

int emit_bb_is_format_mode(void) {
    return g_bb_emit_format &&
           (bb_emit_mode == EMIT_TEXT || bb_emit_mode == EMIT_TEXT_INLINE);
}


static char g_fmt_label[BB_LABEL_NAME_MAX + 4];
static char g_fmt_body[512];

static void fmt_label_save(bb_label_t *lbl) {
    if (lbl && lbl->name[0]) snprintf(g_fmt_label, sizeof(g_fmt_label), "%s:", lbl->name);
    else g_fmt_label[0] = '\0';
}
void fmt_body_append(const char *instr, const char *operands) {
    char frag[128];
    if (operands && operands[0]) snprintf(frag, sizeof(frag), "%s %s", instr, operands);
    else                          snprintf(frag, sizeof(frag), "%s", instr);
    if (g_fmt_body[0]) { strncat(g_fmt_body, " ; ", sizeof(g_fmt_body) - strlen(g_fmt_body) - 1); }
    strncat(g_fmt_body, frag, sizeof(g_fmt_body) - strlen(g_fmt_body) - 1);
}
static void fmt_flush_jmp(const char *mn, bb_label_t *target) {

    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    char jmp_part[BB_LABEL_NAME_MAX + 16];
    snprintf(jmp_part, sizeof(jmp_part), "%s %s", mn, target->name);
    char col3[640];
    if (g_fmt_body[0]) snprintf(col3, sizeof(col3), "%s ; %s", g_fmt_body, jmp_part);
    else                snprintf(col3, sizeof(col3), "%s", jmp_part);
    bb3c_format(f, g_fmt_label, "", col3);
    g_fmt_label[0] = '\0';
    g_fmt_body[0]  = '\0';
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_format_port(bb_label_t *lbl_entry, const char *macro_name, const char *args)
{
    if (!emit_bb_is_format_mode()) return;
    char lbl_str[BB_LABEL_NAME_MAX + 2] = "";
    if (lbl_entry && lbl_entry->name[0]) {
        snprintf(lbl_str, sizeof(lbl_str), "%s:", lbl_entry->name);
    }
    bb3c_format(bb_emit_out ? bb_emit_out : stdout,
                lbl_str,
                macro_name ? macro_name : "",
                args ? args : "");
}




int g_in_text_macro_body = 0;

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_mode_set(bb_emit_mode_t m, FILE *out)
{
    bb_emit_mode = m;
    bb_emit_out  = out;
}

/* Each helper below contains the full three-way decision.  Templates call */

/*----------------------------------------------------------------------------------------------------------------------------------------*/
/* Each instruction helper renders the same x86 effect three ways. */

FILE *emit_outf(void) { return bb_emit_out ? bb_emit_out : stdout; }

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_inc_mem_r13_disp8(uint8_t disp)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:   {
        bb_emit_byte(0x41); bb_emit_byte(0xFF);
        bb_emit_byte(0x45); bb_emit_byte(disp);
        return;
    }
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
        bb_emit_byte(0x55);
        bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0xE5);
        bb_emit_byte(0x48); bb_emit_byte(0x83); bb_emit_byte(0xEC); bb_emit_byte(0x08);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
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
        bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0xEC);
        bb_emit_byte(0x5D);
        bb_emit_byte(0xC3);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
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
        bb_emit_byte(0x55);
        bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0xE5);
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
    case EMIT_BINARY_WIRED: {
        bb_emit_byte(0xB8);
        bb_emit_byte((uint8_t)((uint32_t)result      ));
        bb_emit_byte((uint8_t)((uint32_t)result >>  8));
        bb_emit_byte((uint8_t)((uint32_t)result >> 16));
        bb_emit_byte((uint8_t)((uint32_t)result >> 24));
        bb_emit_byte(0x5D);
        bb_emit_byte(0xC3);
        return;
    }
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
void emit_pad_to_blob_size(void)
{

    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_macro_begin(const char *name, const char *const *params, int nparams)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        return;
    case EMIT_TEXT_INLINE: return;
    case EMIT_TEXT: {
        bb3c_flush_pending_cjmp_only();
        FILE *f = emit_outf();
        fprintf(f, "    %s", name ? name : "?");
        for (int i = 0; i < nparams; i++) {
            fprintf(f, "%s%s", (i == 0 ? " " : ", "),
                    params && params[i] ? params[i] : "?");
        }
        fputc('\n', f);
        g_in_text_macro_body = 1;
        return;
    }
    case EMIT_MACRO_DEF: {
        bb3c_flush_pending_cjmp_only();
        FILE *f = emit_outf();
        fprintf(f, ".macro %s", name ? name : "?");
        for (int i = 0; i < nparams; i++) {
            fprintf(f, "%s%s", (i == 0 ? " " : ", "),
                    params && params[i] ? params[i] : "?");
        }
        fputc('\n', f);
        g_in_text_macro_body = 1;
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_macro_end(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        return;
    case EMIT_TEXT_INLINE: return;
    case EMIT_TEXT:
        g_in_text_macro_body = 0;
        return;
    case EMIT_MACRO_DEF:
        bb3c_flush_pending_cjmp_only();
        fputs(".endm\n", emit_outf());
        g_in_text_macro_body = 0;
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_jmp(bb_label_t *target, jmp_kind_t kind)
{
    static const char *const mn_tab[] = { "jmp", "je", "jne", "jl", "jge", "jg" };
    const char *mn = ((unsigned)kind < 6) ? mn_tab[kind] : "jmp";
    if (emit_bb_is_format_mode()) { fmt_flush_jmp(mn, target); return; }
    switch (bb_emit_mode) {
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_emit_jmp(emit_outf(), mn, target->name);
        return;
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        switch (kind) {
        case JMP_JMP: bb_insn_jmp_rel32(target);  return;
        case JMP_JE:  bb_insn_je_rel32(target);   return;
        case JMP_JNE: bb_insn_jne_rel32(target);  return;
        case JMP_JL:  bb_insn_jl_rel32(target);   return;
        case JMP_JGE: bb_insn_jge_rel32(target);  return;
        case JMP_JG:  bb_insn_jg_rel32(target);   return;
        }
        return;
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
    case EMIT_BINARY_BROKERED:   {
        uint64_t v = in_proc_ptr;
        bb_emit_byte(0x48); bb_emit_byte(0xBA);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        bb_emit_byte((uint8_t)(v >> 32)); bb_emit_byte((uint8_t)(v >> 40));
        bb_emit_byte((uint8_t)(v >> 48)); bb_emit_byte((uint8_t)(v >> 56));
        return;
    }
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
    case EMIT_BINARY_BROKERED:   {
        uint32_t u = (uint32_t)val;
        bb_emit_byte(0xBA);
        bb_emit_byte((uint8_t)(u      )); bb_emit_byte((uint8_t)(u >>  8));
        bb_emit_byte((uint8_t)(u >> 16)); bb_emit_byte((uint8_t)(u >> 24));
        return;
    }
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
    case EMIT_BINARY_BROKERED:   {
        uint32_t u = (uint32_t)val;
        bb_emit_byte(0xBF);
        bb_emit_byte((uint8_t)(u      ));
        bb_emit_byte((uint8_t)(u >>  8));
        bb_emit_byte((uint8_t)(u >> 16));
        bb_emit_byte((uint8_t)(u >> 24));
        return;
    }
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

        bb_emit_byte(0x90);
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
    case EMIT_BINARY_BROKERED:   {
        uint64_t v = entry_ptr;
        bb_emit_byte(0x48); bb_emit_byte(0xBF);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        bb_emit_byte((uint8_t)(v >> 32)); bb_emit_byte((uint8_t)(v >> 40));
        bb_emit_byte((uint8_t)(v >> 48)); bb_emit_byte((uint8_t)(v >> 56));
        return;
    }
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
/*----------------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb3c_op(const char *mn, const char *fmt, ...)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    char argbuf[256];
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(argbuf, sizeof(argbuf), fmt, ap);
        va_end(ap);
    } else {
        argbuf[0] = '\0';
    }
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    bb3c_format(f, "", mn ? mn : "", argbuf);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb3c_jmp(const char *mn, const char *target)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    bb3c_emit_jmp(f, mn ? mn : "", target ? target : "");
}



























/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_label_define(bb_label_t *lbl)
{
    if (emit_bb_is_format_mode()) { fmt_label_save(lbl); return; }
    bb_label_define(lbl);
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
        bb_emit_byte(0x41); bb_emit_byte(0x52);
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
        bb_emit_byte(0x41); bb_emit_byte(0x5A);
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
        bb_emit_byte(0x41); bb_emit_byte(0x8B); bb_emit_byte(0x02);
        bb_emit_byte(0x3D);
        bb_emit_byte((uint8_t)((uint32_t)n      ));
        bb_emit_byte((uint8_t)((uint32_t)n >>  8));
        bb_emit_byte((uint8_t)((uint32_t)n >> 16));
        bb_emit_byte((uint8_t)((uint32_t)n >> 24));
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
        bb_emit_byte(0x48); bb_emit_byte(0xB9);
        bb_emit_byte((uint8_t)(siglen_addr      ));
        bb_emit_byte((uint8_t)(siglen_addr >>  8));
        bb_emit_byte((uint8_t)(siglen_addr >> 16));
        bb_emit_byte((uint8_t)(siglen_addr >> 24));
        bb_emit_byte((uint8_t)(siglen_addr >> 32));
        bb_emit_byte((uint8_t)(siglen_addr >> 40));
        bb_emit_byte((uint8_t)(siglen_addr >> 48));
        bb_emit_byte((uint8_t)(siglen_addr >> 56));
        bb_emit_byte(0x8B); bb_emit_byte(0x01);
        bb_emit_byte(0x2D);
        bb_emit_byte((uint8_t)((uint32_t)n      ));
        bb_emit_byte((uint8_t)((uint32_t)n >>  8));
        bb_emit_byte((uint8_t)((uint32_t)n >> 16));
        bb_emit_byte((uint8_t)((uint32_t)n >> 24));
        bb_emit_byte(0x89); bb_emit_byte(0xC1);
        bb_emit_byte(0x41); bb_emit_byte(0x8B); bb_emit_byte(0x02);
        bb_emit_byte(0x39); bb_emit_byte(0xC8);
        emit_jmp(lbl_fail, JMP_JNE);
        emit_jmp(lbl_succ, JMP_JMP);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) {
            char args[32]; snprintf(args, sizeof(args), "eax, %d", n);
            fmt_body_append("lea", "rcx, [rip + Σlen]");
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
        bb3c_format(f, "", "lea", "rcx, [rip + Σlen]");
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
        bb_emit_byte(0x48); bb_emit_byte(0xBE);
        bb_emit_byte((uint8_t)(in_proc_ptr      ));
        bb_emit_byte((uint8_t)(in_proc_ptr >>  8));
        bb_emit_byte((uint8_t)(in_proc_ptr >> 16));
        bb_emit_byte((uint8_t)(in_proc_ptr >> 24));
        bb_emit_byte((uint8_t)(in_proc_ptr >> 32));
        bb_emit_byte((uint8_t)(in_proc_ptr >> 40));
        bb_emit_byte((uint8_t)(in_proc_ptr >> 48));
        bb_emit_byte((uint8_t)(in_proc_ptr >> 56));
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
        bb_emit_byte(0x48); bb_emit_byte(0xB9);
        bb_emit_byte((uint8_t)(sigma_addr      ));
        bb_emit_byte((uint8_t)(sigma_addr >>  8));
        bb_emit_byte((uint8_t)(sigma_addr >> 16));
        bb_emit_byte((uint8_t)(sigma_addr >> 24));
        bb_emit_byte((uint8_t)(sigma_addr >> 32));
        bb_emit_byte((uint8_t)(sigma_addr >> 40));
        bb_emit_byte((uint8_t)(sigma_addr >> 48));
        bb_emit_byte((uint8_t)(sigma_addr >> 56));
        bb_emit_byte(0x48); bb_emit_byte(0x8B); bb_emit_byte(0x01);
        bb_emit_byte(0x49); bb_emit_byte(0x63); bb_emit_byte(0x0A);
        bb_emit_byte(0x48); bb_emit_byte(0x8D); bb_emit_byte(0x04); bb_emit_byte(0x08);
        bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0xC7);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) {
            fmt_body_append("lea", "rcx, [rip + Σ]");
            fmt_body_append("mov", "rax, [rcx]");
            fmt_body_append("movsxd", "rcx, [r10]");
            fmt_body_append("lea", "rax, [rax+rcx]");
            fmt_body_append("mov", "rdi, rax");
            return;
        }
        FILE *f = emit_outf();
        char args[80];
        (void)args;
        bb3c_format(f, "", "lea", "rcx, [rip + Σ]");
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
        bb_emit_byte(0x41); bb_emit_byte(0x8B); bb_emit_byte(0x02);
        bb_emit_byte(0x05);
        bb_emit_byte((uint8_t)((uint32_t)len      ));
        bb_emit_byte((uint8_t)((uint32_t)len >>  8));
        bb_emit_byte((uint8_t)((uint32_t)len >> 16));
        bb_emit_byte((uint8_t)((uint32_t)len >> 24));
        bb_emit_byte(0x48); bb_emit_byte(0xB9);
        bb_emit_byte((uint8_t)(siglen_addr      ));
        bb_emit_byte((uint8_t)(siglen_addr >>  8));
        bb_emit_byte((uint8_t)(siglen_addr >> 16));
        bb_emit_byte((uint8_t)(siglen_addr >> 24));
        bb_emit_byte((uint8_t)(siglen_addr >> 32));
        bb_emit_byte((uint8_t)(siglen_addr >> 40));
        bb_emit_byte((uint8_t)(siglen_addr >> 48));
        bb_emit_byte((uint8_t)(siglen_addr >> 56));
        bb_emit_byte(0x3B); bb_emit_byte(0x01);
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
            fmt_body_append("lea", "rcx, [rip + Σlen]");
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
        bb3c_format(f, "", "lea", "rcx, [rip + Σlen]");
        bb3c_format(f, "", "cmp", "eax, [rcx]");
        emit_jmp(lbl_fail, JMP_JG);
        return;
    }
    }
}










