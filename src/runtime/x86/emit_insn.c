#include "emit_insn.h"
#include "emit_bb_gen.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_eax_imm32(uint32_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("mov", "eax, %u", imm); return; }
    bb_emit_byte(0xB8); bb_emit_u32(imm);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_rax_imm64(uint64_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("mov", "rax, 0x%llx", (unsigned long long)imm); return; }
    bb_emit_byte(0x48); bb_emit_byte(0xB8); bb_emit_u64(imm);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_ret(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("ret", NULL); return; }
    bb_emit_byte(0xC3);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_nop(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("nop", NULL); return; }
    bb_emit_byte(0x90);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_call_rax(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("call", "rax"); return; }
    bb_emit_byte(0xFF); bb_emit_byte(0xD0);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jmp_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_jmp("jmp", target->name); return; }
    bb_emit_byte(0xEB); bb_emit_patch_rel8(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jmp_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_jmp("jmp", target->name); return; }
    bb_emit_byte(0xE9); bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jl_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_jmp("jl", target->name); return; }
    bb_emit_byte(0x7C); bb_emit_patch_rel8(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jge_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_jmp("jge", target->name); return; }
    bb_emit_byte(0x7D); bb_emit_patch_rel8(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_je_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_jmp("je", target->name); return; }
    bb_emit_byte(0x74); bb_emit_patch_rel8(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jne_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_jmp("jne", target->name); return; }
    bb_emit_byte(0x75); bb_emit_patch_rel8(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jne_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_jmp("jne", target->name); return; }
    bb_emit_byte(0x0F); bb_emit_byte(0x85); bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_je_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_jmp("je", target->name); return; }
    bb_emit_byte(0x0F); bb_emit_byte(0x84); bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jl_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_jmp("jl", target->name); return; }
    bb_emit_byte(0x0F); bb_emit_byte(0x8C); bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jge_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_jmp("jge", target->name); return; }
    bb_emit_byte(0x0F); bb_emit_byte(0x8D); bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jg_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_jmp("jg", target->name); return; }
    bb_emit_byte(0x0F); bb_emit_byte(0x8F); bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_cmp_esi_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("cmp", "esi, %u", (unsigned)imm); return; }
    bb_emit_byte(0x83); bb_emit_byte(0xFE); bb_emit_byte(imm);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_cmp_esi_imm32(uint32_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("cmp", "esi, %u", imm); return; }
    bb_emit_byte(0x81); bb_emit_byte(0xFE); bb_emit_u32(imm);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_movzx_eax_rdi_off8(uint8_t off)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("movzx", "eax, byte [rdi + %u]", (unsigned)off); return; }
    bb_emit_byte(0x0F); bb_emit_byte(0xB6); bb_emit_byte(0x47); bb_emit_byte(off);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_cmp_al_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("cmp", "al, %u", (unsigned)imm); return; }
    bb_emit_byte(0x3C); bb_emit_byte(imm);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_xor_eax_eax(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("xor", "eax, eax"); return; }
    bb_emit_byte(0x31); bb_emit_byte(0xC0);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_push_rbp(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("push", "rbp"); return; }
    bb_emit_byte(0x55);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_pop_rbp(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("pop", "rbp"); return; }
    bb_emit_byte(0x5D);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_rbp_rsp(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("mov", "rbp, rsp"); return; }
    bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0xE5);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_sub_rsp_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("sub", "rsp, %u", (unsigned)imm); return; }
    bb_emit_byte(0x48); bb_emit_byte(0x83); bb_emit_byte(0xEC); bb_emit_byte(imm);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_add_rsp_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("add", "rsp, %u", (unsigned)imm); return; }
    bb_emit_byte(0x48); bb_emit_byte(0x83); bb_emit_byte(0xC4); bb_emit_byte(imm);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_ret(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(0xC3); return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", "ret", ""); return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_push_r10(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x41); bb_emit_byte(0x52); return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) { fmt_body_append("push", "r10"); return; }
        bb3c_format(emit_outf(), "", "push", "r10"); return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_pop_r10(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x41); bb_emit_byte(0x5A); return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) { fmt_body_append("pop", "r10"); return; }
        bb3c_format(emit_outf(), "", "pop", "r10"); return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_test_rax_rax(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x48); bb_emit_byte(0x85); bb_emit_byte(0xC0); return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", "test", "rax, rax"); return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_test_eax_eax(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x85); bb_emit_byte(0xC0); return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) { fmt_body_append("test", "eax, eax"); return; }
        bb3c_format(emit_outf(), "", "test", "eax, eax"); return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_mov_rdi_imm64(uint64_t val)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED: {
        bb_emit_byte(0x48); bb_emit_byte(0xBF);
        bb_emit_byte((uint8_t)(val      )); bb_emit_byte((uint8_t)(val >>  8));
        bb_emit_byte((uint8_t)(val >> 16)); bb_emit_byte((uint8_t)(val >> 24));
        bb_emit_byte((uint8_t)(val >> 32)); bb_emit_byte((uint8_t)(val >> 40));
        bb_emit_byte((uint8_t)(val >> 48)); bb_emit_byte((uint8_t)(val >> 56));
        return;
    }
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        char args[64];
        snprintf(args, sizeof(args), "rdi, 0x%llx", (unsigned long long)val);
        bb3c_format(emit_outf(), "", "mov", args); return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_call_sym_plt(const char *sym, uint64_t fn_fallback)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED: {
        bb_emit_byte(0x48); bb_emit_byte(0xB8);
        bb_emit_byte((uint8_t)(fn_fallback      )); bb_emit_byte((uint8_t)(fn_fallback >>  8));
        bb_emit_byte((uint8_t)(fn_fallback >> 16)); bb_emit_byte((uint8_t)(fn_fallback >> 24));
        bb_emit_byte((uint8_t)(fn_fallback >> 32)); bb_emit_byte((uint8_t)(fn_fallback >> 40));
        bb_emit_byte((uint8_t)(fn_fallback >> 48)); bb_emit_byte((uint8_t)(fn_fallback >> 56));
        bb_emit_byte(0xFF); bb_emit_byte(0xD0);
        return;
    }
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        char args[80];
        snprintf(args, sizeof(args), "%s@PLT", sym ? sym : "??sym??");
        if (emit_bb_is_format_mode()) { fmt_body_append("call", args); return; }
        bb3c_format(emit_outf(), "", "call", args); return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_mov_esi_imm32(int val)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED: {
        uint32_t u = (uint32_t)val;
        bb_emit_byte(0xBE);
        bb_emit_byte((uint8_t)(u      )); bb_emit_byte((uint8_t)(u >>  8));
        bb_emit_byte((uint8_t)(u >> 16)); bb_emit_byte((uint8_t)(u >> 24));
        return;
    }
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;
        char args[32]; snprintf(args, sizeof(args), "esi, %d", val);
        bb3c_format(emit_outf(), "", "mov", args); return;
    }
    case EMIT_MACRO_DEF:
        bb3c_format(emit_outf(), "", "mov", "esi, \\n"); return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_add_delta_imm(int v)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x41); bb_emit_byte(0x8B); bb_emit_byte(0x02);
        bb_emit_byte(0x05);
        bb_emit_byte((uint8_t)((uint32_t)v      )); bb_emit_byte((uint8_t)((uint32_t)v >>  8));
        bb_emit_byte((uint8_t)((uint32_t)v >> 16)); bb_emit_byte((uint8_t)((uint32_t)v >> 24));
        bb_emit_byte(0x41); bb_emit_byte(0x89); bb_emit_byte(0x02);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[32]; snprintf(args, sizeof(args), "eax, %d", v);
        if (emit_bb_is_format_mode()) {
            fmt_body_append("mov", "eax, [r10]");
            fmt_body_append("add", args);
            fmt_body_append("mov", "[r10], eax");
            return;
        }
        FILE *f = emit_outf();
        bb3c_format(f, "", "mov", "eax, [r10]");
        bb3c_format(f, "", "add", args);
        bb3c_format(f, "", "mov", "[r10], eax");
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_sub_delta_imm(int v)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x41); bb_emit_byte(0x8B); bb_emit_byte(0x02);
        bb_emit_byte(0x2D);
        bb_emit_byte((uint8_t)((uint32_t)v      )); bb_emit_byte((uint8_t)((uint32_t)v >>  8));
        bb_emit_byte((uint8_t)((uint32_t)v >> 16)); bb_emit_byte((uint8_t)((uint32_t)v >> 24));
        bb_emit_byte(0x41); bb_emit_byte(0x89); bb_emit_byte(0x02);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[32]; snprintf(args, sizeof(args), "eax, %d", v);
        if (emit_bb_is_format_mode()) {
            fmt_body_append("mov", "eax, [r10]");
            fmt_body_append("sub", args);
            fmt_body_append("mov", "[r10], eax");
            return;
        }
        FILE *f = emit_outf();
        bb3c_format(f, "", "mov", "eax, [r10]");
        bb3c_format(f, "", "sub", args);
        bb3c_format(f, "", "mov", "[r10], eax");
        return;
    }
    }
}
