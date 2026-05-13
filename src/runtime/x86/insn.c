/* insn.c — RW-1: leaf x86 instruction emitters.
 *
 * Each function: if (IS_TEXT) { text; return; } / binary bytes below.
 * IS_TEXT is the only mode check at this layer — never in callers above.
 * Written alongside old emit_insn.c — no callers changed in RW-1.
 */

#include "insn.h"
#include "emit_buf.h"
#include "emit_text3c.h"
#include "emit_mode.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* Shorthand for raw byte emission — binary only, guarded by IS_BIN callers */
#define B(b)       bb_emit_byte((uint8_t)(b))
#define U32(v)     bb_emit_u32((uint32_t)(v))
#define U64(v)     bb_emit_u64((uint64_t)(v))

/* TEXT-mode 3-col helpers */
#define T3C(mn,fmt,...)  bb3c_format(emit_outf(),"",mn,fmt)
static void t3(const char *mn, const char *args) {
    bb3c_format(emit_outf(), "", mn, args);
}
static void tf(const char *mn, const char *fmt, ...) {
    char buf[64]; va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap);
    bb3c_format(emit_outf(), "", mn, buf);
}
static void tj(const char *mn, const char *target) {
    bb3c_emit_jmp(emit_outf(), mn, target);
}

/*========================================================================*/
/* mov family                                                             */
/*========================================================================*/

void insn_mov_eax_i32(uint32_t v) {
    if (IS_TEXT) { tf("mov","eax, %u",v); return; }
    B(0xB8); U32(v);
}
void insn_mov_rax_i64(uint64_t v) {
    if (IS_TEXT) { tf("mov","rax, 0x%llx",(unsigned long long)v); return; }
    B(0x48); B(0xB8); U64(v);
}
void insn_mov_rcx_i64(uint64_t v) {
    if (IS_TEXT) { tf("mov","rcx, 0x%llx",(unsigned long long)v); return; }
    B(0x48); B(0xB9); U64(v);
}
void insn_mov_rdx_i64(uint64_t v) {
    if (IS_TEXT) { tf("mov","rdx, 0x%llx",(unsigned long long)v); return; }
    B(0x48); B(0xBA); U64(v);
}
void insn_mov_rsi_i64(uint64_t v) {
    if (IS_TEXT) { tf("mov","rsi, 0x%llx",(unsigned long long)v); return; }
    B(0x48); B(0xBE); U64(v);
}
void insn_mov_rdi_i64(uint64_t v) {
    if (IS_TEXT) { tf("mov","rdi, 0x%llx",(unsigned long long)v); return; }
    B(0x48); B(0xBF); U64(v);
}
void insn_mov_edx_i32(uint32_t v) {
    if (IS_TEXT) { tf("mov","edx, %u",v); return; }
    B(0xBA); U32(v);
}
void insn_mov_edi_i32(uint32_t v) {
    if (IS_TEXT) { tf("mov","edi, %u",v); return; }
    B(0xBF); U32(v);
}
void insn_mov_esi_i32(int v) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF) { t3("mov","esi, \\n"); return; }
        tf("mov","esi, %d",v); return;
    }
    B(0xBE); U32((uint32_t)v);
}
void insn_mov_rbp_rsp(void) {
    if (IS_TEXT) { t3("mov","rbp, rsp"); return; }
    B(0x48); B(0x89); B(0xE5);
}
void insn_mov_rsp_rbp(void) {
    if (IS_TEXT) { t3("mov","rsp, rbp"); return; }
    B(0x48); B(0x89); B(0xEC);
}
void insn_mov_ecx_eax(void) {
    if (IS_TEXT) { t3("mov","ecx, eax"); return; }
    B(0x89); B(0xC1);
}
void insn_mov_rdi_rax(void) {
    if (IS_TEXT) { t3("mov","rdi, rax"); return; }
    B(0x48); B(0x89); B(0xC7);
}
void insn_mov_eax_r10mem(void) {
    if (IS_TEXT) { t3("mov","eax, [r10]"); return; }
    B(0x41); B(0x8B); B(0x02);
}
void insn_mov_eax_rcxmem(void) {
    if (IS_TEXT) { t3("mov","eax, [rcx]"); return; }
    B(0x8B); B(0x01);
}
void insn_mov_rax_rcxmem(void) {
    if (IS_TEXT) { t3("mov","rax, [rcx]"); return; }
    B(0x48); B(0x8B); B(0x01);
}

/*========================================================================*/
/* lea                                                                    */
/*========================================================================*/

void insn_lea_rcx_rip_sym(const char *sym, uint64_t addr) {
    if (IS_TEXT) { tf("lea","rcx, [rip + %s]", sym ? sym : "??"); return; }
    B(0x48); B(0xB9); U64(addr);   /* binary: movabs rcx, addr */
}
void insn_lea_r10_rip_sym(const char *sym, uint64_t addr) {
    if (IS_TEXT) { tf("lea","r10, [rip + %s]", sym ? sym : "??"); return; }
    B(0x49); B(0xBA); U64(addr);   /* binary: movabs r10, addr */
}
void insn_lea_rax_rax_rcx(void) {
    if (IS_TEXT) { t3("lea","rax, [rax+rcx]"); return; }
    B(0x48); B(0x8D); B(0x04); B(0x08);
}

/*========================================================================*/
/* movzx / movsxd                                                         */
/*========================================================================*/

void insn_movzx_eax_rdi_off8(uint8_t off) {
    if (IS_TEXT) { tf("movzx","eax, byte [rdi + %u]",(unsigned)off); return; }
    B(0x0F); B(0xB6); B(0x47); B(off);
}
void insn_movsxd_rcx_r10mem(void) {
    if (IS_TEXT) { t3("movsxd","rcx, dword [r10]"); return; }
    B(0x49); B(0x63); B(0x0A);
}

/*========================================================================*/
/* cmp                                                                    */
/*========================================================================*/

void insn_cmp_esi_i8(uint8_t v) {
    if (IS_TEXT) { tf("cmp","esi, %u",(unsigned)v); return; }
    B(0x83); B(0xFE); B(v);
}
void insn_cmp_esi_i32(uint32_t v) {
    if (IS_TEXT) { tf("cmp","esi, %u",v); return; }
    B(0x81); B(0xFE); U32(v);
}
void insn_cmp_al_i8(uint8_t v) {
    if (IS_TEXT) { tf("cmp","al, %u",(unsigned)v); return; }
    B(0x3C); B(v);
}
void insn_cmp_eax_i32(uint32_t v) {
    if (IS_TEXT) { tf("cmp","eax, %u",v); return; }
    B(0x3D); U32(v);
}
void insn_cmp_eax_ecx(void) {
    if (IS_TEXT) { t3("cmp","eax, ecx"); return; }
    B(0x39); B(0xC8);
}
void insn_cmp_eax_rcxmem(void) {
    if (IS_TEXT) { t3("cmp","eax, [rcx]"); return; }
    B(0x3B); B(0x01);
}

/*========================================================================*/
/* add / sub                                                              */
/*========================================================================*/

void insn_add_rsp_i8(uint8_t v) {
    if (IS_TEXT) { tf("add","rsp, %u",(unsigned)v); return; }
    B(0x48); B(0x83); B(0xC4); B(v);
}
void insn_sub_rsp_i8(uint8_t v) {
    if (IS_TEXT) { tf("sub","rsp, %u",(unsigned)v); return; }
    B(0x48); B(0x83); B(0xEC); B(v);
}
void insn_add_eax_i32(uint32_t v) {
    if (IS_TEXT) { tf("add","eax, %u",v); return; }
    B(0x05); U32(v);
}
void insn_sub_eax_i32(uint32_t v) {
    if (IS_TEXT) { tf("sub","eax, %u",v); return; }
    B(0x2D); U32(v);
}
void insn_add_delta_i(int v) {
    /* mov eax,[r10] ; add eax,v ; mov [r10],eax */
    if (IS_TEXT) {
        char a[32]; snprintf(a,sizeof(a),"eax, %d",v);
        if (emit_bb_is_format_mode()) {
            fmt_body_append("mov","eax, [r10]");
            fmt_body_append("add",a);
            fmt_body_append("mov","[r10], eax");
            return;
        }
        t3("mov","eax, [r10]"); t3("add",a); t3("mov","[r10], eax"); return;
    }
    B(0x41); B(0x8B); B(0x02);
    B(0x05); U32((uint32_t)v);
    B(0x41); B(0x89); B(0x02);
}
void insn_sub_delta_i(int v) {
    /* mov eax,[r10] ; sub eax,v ; mov [r10],eax */
    if (IS_TEXT) {
        char a[32]; snprintf(a,sizeof(a),"eax, %d",v);
        if (emit_bb_is_format_mode()) {
            fmt_body_append("mov","eax, [r10]");
            fmt_body_append("sub",a);
            fmt_body_append("mov","[r10], eax");
            return;
        }
        t3("mov","eax, [r10]"); t3("sub",a); t3("mov","[r10], eax"); return;
    }
    B(0x41); B(0x8B); B(0x02);
    B(0x2D); U32((uint32_t)v);
    B(0x41); B(0x89); B(0x02);
}

/*========================================================================*/
/* test / xor                                                             */
/*========================================================================*/

void insn_test_rax_rax(void) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode()) { fmt_body_append("test","rax, rax"); return; }
        t3("test","rax, rax"); return;
    }
    B(0x48); B(0x85); B(0xC0);
}
void insn_test_eax_eax(void) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode()) { fmt_body_append("test","eax, eax"); return; }
        t3("test","eax, eax"); return;
    }
    B(0x85); B(0xC0);
}
void insn_xor_eax_eax(void) {
    if (IS_TEXT) { t3("xor","eax, eax"); return; }
    B(0x31); B(0xC0);
}

/*========================================================================*/
/* inc                                                                    */
/*========================================================================*/

void insn_inc_r13_disp8(uint8_t disp) {
    if (IS_TEXT) { tf("inc","dword ptr [r13 + %u]",(unsigned)disp); return; }
    B(0x41); B(0xFF); B(0x45); B(disp);
}

/*========================================================================*/
/* push / pop (X-group)                                                   */
/*========================================================================*/

/* INSN_PUSH_POP(name, push_bytes..., pop_bytes..., text_reg) */
void insn_push_rbp(void) {
    if (IS_TEXT) { t3("push","rbp"); return; }
    B(0x55);
}
void insn_pop_rbp(void) {
    if (IS_TEXT) { t3("pop","rbp"); return; }
    B(0x5D);
}
void insn_push_r10(void) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode()) { fmt_body_append("push","r10"); return; }
        t3("push","r10"); return;
    }
    B(0x41); B(0x52);
}
void insn_pop_r10(void) {
    if (IS_TEXT) {
        if (emit_bb_is_format_mode()) { fmt_body_append("pop","r10"); return; }
        t3("pop","r10"); return;
    }
    B(0x41); B(0x5A);
}
void insn_push_r12(void) {
    if (IS_TEXT) { t3("push","r12"); return; }
    B(0x41); B(0x54);
}
void insn_pop_r12(void) {
    if (IS_TEXT) { t3("pop","r12"); return; }
    B(0x41); B(0x5C);
}

/*========================================================================*/
/* call / ret / nop                                                       */
/*========================================================================*/

void insn_ret(void) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        t3("ret",""); return;
    }
    B(0xC3);
}
void insn_nop(void) {
    if (IS_TEXT) { t3("nop",""); return; }
    B(0x90);
}
void insn_call_rax(void) {
    if (IS_TEXT) { t3("call","rax"); return; }
    B(0xFF); B(0xD0);
}
void insn_call_plt(const char *sym, uint64_t fn_fallback) {
    if (IS_TEXT) {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        char args[80]; snprintf(args,sizeof(args),"%s@PLT",sym?sym:"??");
        if (emit_bb_is_format_mode()) { fmt_body_append("call",args); return; }
        t3("call",args); return;
    }
    B(0x48); B(0xB8); U64(fn_fallback);
    B(0xFF); B(0xD0);
}

/*========================================================================*/
/* jcc / jmp (X-group)                                                    */
/*========================================================================*/

/* INSN_JCC macro: text uses tj(); binary emits opcode(s) + patch */
#define INSN_JMP_R8(fn, op) \
    void fn(bb_label_t *t) { \
        if (IS_TEXT) { tj(#op, t->name); return; } \
        B(0x##op); bb_emit_patch_rel8(t); \
    }
#define INSN_JCC_R8(fn, mn, op) \
    void fn(bb_label_t *t) { \
        if (IS_TEXT) { tj(mn, t->name); return; } \
        B(0x##op); bb_emit_patch_rel8(t); \
    }
#define INSN_JCC_R32(fn, mn, op1, op2) \
    void fn(bb_label_t *t) { \
        if (IS_TEXT) { tj(mn, t->name); return; } \
        B(0x0F); B(0x##op1); bb_emit_patch_rel32(t); \
    }

void insn_jmp_r8(bb_label_t *t) {
    if (IS_TEXT) { tj("jmp", t->name); return; }
    B(0xEB); bb_emit_patch_rel8(t);
}
void insn_jmp_r32(bb_label_t *t) {
    if (IS_TEXT) { tj("jmp", t->name); return; }
    B(0xE9); bb_emit_patch_rel32(t);
}
void insn_je_r8(bb_label_t *t) {
    if (IS_TEXT) { tj("je", t->name); return; }
    B(0x74); bb_emit_patch_rel8(t);
}
void insn_je_r32(bb_label_t *t) {
    if (IS_TEXT) { tj("je", t->name); return; }
    B(0x0F); B(0x84); bb_emit_patch_rel32(t);
}
void insn_jne_r8(bb_label_t *t) {
    if (IS_TEXT) { tj("jne", t->name); return; }
    B(0x75); bb_emit_patch_rel8(t);
}
void insn_jne_r32(bb_label_t *t) {
    if (IS_TEXT) { tj("jne", t->name); return; }
    B(0x0F); B(0x85); bb_emit_patch_rel32(t);
}
void insn_jl_r8(bb_label_t *t) {
    if (IS_TEXT) { tj("jl", t->name); return; }
    B(0x7C); bb_emit_patch_rel8(t);
}
void insn_jl_r32(bb_label_t *t) {
    if (IS_TEXT) { tj("jl", t->name); return; }
    B(0x0F); B(0x8C); bb_emit_patch_rel32(t);
}
void insn_jge_r8(bb_label_t *t) {
    if (IS_TEXT) { tj("jge", t->name); return; }
    B(0x7D); bb_emit_patch_rel8(t);
}
void insn_jge_r32(bb_label_t *t) {
    if (IS_TEXT) { tj("jge", t->name); return; }
    B(0x0F); B(0x8D); bb_emit_patch_rel32(t);
}
void insn_jg_r32(bb_label_t *t) {
    if (IS_TEXT) { tj("jg", t->name); return; }
    B(0x0F); B(0x8F); bb_emit_patch_rel32(t);
}
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
/* EM-RAW-PURGE: additional insn helpers to replace raw bytes in emit_bb_seq.c */
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_rsp_rbp(void)          /* mov rsp, rbp  (48 89 EC) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0xEC); return;
    default: bb3c_format(emit_outf(), "", "mov", "rsp, rbp"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_rdx_imm64(uint64_t v)  /* movabs rdx, imm64  (48 BA imm64) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x48); bb_emit_byte(0xBA);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        bb_emit_byte((uint8_t)(v >> 32)); bb_emit_byte((uint8_t)(v >> 40));
        bb_emit_byte((uint8_t)(v >> 48)); bb_emit_byte((uint8_t)(v >> 56));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"rdx, 0x%llx",(unsigned long long)v);
               bb3c_format(emit_outf(),"","mov",a); return; }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_edx_imm32(uint32_t v)  /* mov edx, imm32  (BA imm32) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0xBA);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"edx, %u",v);
               bb3c_format(emit_outf(),"","mov",a); return; }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_edi_imm32(uint32_t v)  /* mov edi, imm32  (BF imm32) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0xBF);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"edi, %u",v);
               bb3c_format(emit_outf(),"","mov",a); return; }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_rsi_imm64(uint64_t v)  /* movabs rsi, imm64  (48 BE imm64) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x48); bb_emit_byte(0xBE);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        bb_emit_byte((uint8_t)(v >> 32)); bb_emit_byte((uint8_t)(v >> 40));
        bb_emit_byte((uint8_t)(v >> 48)); bb_emit_byte((uint8_t)(v >> 56));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"rsi, 0x%llx",(unsigned long long)v);
               bb3c_format(emit_outf(),"","mov",a); return; }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_push_r12(void)             /* push r12  (41 54) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x41); bb_emit_byte(0x54); return;
    default: bb3c_format(emit_outf(), "", "push", "r12"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_pop_r12(void)              /* pop r12  (41 5C) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x41); bb_emit_byte(0x5C); return;
    default: bb3c_format(emit_outf(), "", "pop", "r12"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_inc_r13_disp8(uint8_t disp) /* inc dword [r13+disp]  (41 FF 45 disp) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x41); bb_emit_byte(0xFF);
        bb_emit_byte(0x45); bb_emit_byte(disp);
        return;
    default: { char a[40]; snprintf(a,sizeof(a),"dword ptr [r13 + %u]",(unsigned)disp);
               bb3c_format(emit_outf(),"","inc",a); return; }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_rcx_imm64(uint64_t v)  /* movabs rcx, imm64  (48 B9 imm64) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x48); bb_emit_byte(0xB9);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        bb_emit_byte((uint8_t)(v >> 32)); bb_emit_byte((uint8_t)(v >> 40));
        bb_emit_byte((uint8_t)(v >> 48)); bb_emit_byte((uint8_t)(v >> 56));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"rcx, 0x%llx",(unsigned long long)v);
               bb3c_format(emit_outf(),"","mov",a); return; }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_eax_r10mem(void)        /* mov eax, [r10]  (41 8B 02) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x41); bb_emit_byte(0x8B); bb_emit_byte(0x02); return;
    default: bb3c_format(emit_outf(), "", "mov", "eax, [r10]"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_cmp_eax_imm32(uint32_t v)  /* cmp eax, imm32  (3D imm32) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x3D);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"eax, %u",v);
               bb3c_format(emit_outf(),"","cmp",a); return; }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_eax_mem_rcx(void)       /* mov eax, [rcx]  (8B 01) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x8B); bb_emit_byte(0x01); return;
    default: bb3c_format(emit_outf(), "", "mov", "eax, [rcx]"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_sub_eax_imm32(uint32_t v)  /* sub eax, imm32  (2D imm32) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x2D);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"eax, %u",v);
               bb3c_format(emit_outf(),"","sub",a); return; }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_ecx_eax(void)           /* mov ecx, eax  (89 C1) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x89); bb_emit_byte(0xC1); return;
    default: bb3c_format(emit_outf(), "", "mov", "ecx, eax"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_cmp_eax_ecx(void)           /* cmp eax, ecx  (39 C8) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x39); bb_emit_byte(0xC8); return;
    default: bb3c_format(emit_outf(), "", "cmp", "eax, ecx"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_rax_mem_rcx(void)       /* mov rax, [rcx]  (48 8B 01) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x48); bb_emit_byte(0x8B); bb_emit_byte(0x01); return;
    default: bb3c_format(emit_outf(), "", "mov", "rax, [rcx]"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_movsxd_rcx_r10mem(void)     /* movsxd rcx, dword [r10]  (49 63 0A) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x49); bb_emit_byte(0x63); bb_emit_byte(0x0A); return;
    default: bb3c_format(emit_outf(), "", "movsxd", "rcx, dword [r10]"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_lea_rax_rax_rcx(void)       /* lea rax, [rax+rcx]  (48 8D 04 08) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x48); bb_emit_byte(0x8D);
        bb_emit_byte(0x04); bb_emit_byte(0x08); return;
    default: bb3c_format(emit_outf(), "", "lea", "rax, [rax+rcx]"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_rdi_rax(void)           /* mov rdi, rax  (48 89 C7) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0xC7); return;
    default: bb3c_format(emit_outf(), "", "mov", "rdi, rax"); return;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_add_eax_imm32(uint32_t v)  /* add eax, imm32  (05 imm32) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x05);
        bb_emit_byte((uint8_t)(v      )); bb_emit_byte((uint8_t)(v >>  8));
        bb_emit_byte((uint8_t)(v >> 16)); bb_emit_byte((uint8_t)(v >> 24));
        return;
    default: { char a[32]; snprintf(a,sizeof(a),"eax, %u",v);
               bb3c_format(emit_outf(),"","add",a); return; }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_cmp_eax_mem_rcx(void)       /* cmp eax, [rcx]  (3B 01) */
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x3B); bb_emit_byte(0x01); return;
    default: bb3c_format(emit_outf(), "", "cmp", "eax, [rcx]"); return;
    }
}
