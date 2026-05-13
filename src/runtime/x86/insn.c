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
