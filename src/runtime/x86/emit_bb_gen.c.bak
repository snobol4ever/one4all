
#include "emit_bb_gen.h"
#include "emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

bb_emit_mode_t  bb_emit_mode = EMIT_TEXT;
FILE           *bb_emit_out  = NULL;   /* set by caller; defaults to stdout */

/* EM-BB-FORMAT: set by --bb-format; BB templates emit 3-col macro invocations
 * in TEXT mode instead of raw GAS.  0 = legacy raw-GAS (default). */
int g_bb_emit_format = 0;

int emit_bb_is_format_mode(void) {
    return g_bb_emit_format &&
           (bb_emit_mode == EMIT_TEXT || bb_emit_mode == EMIT_TEXT_INLINE);
}

/* EM-BB-FORMAT-ARCH: port-context accumulator.
 * In FORMAT mode, body helpers append instruction fragments here.
 * emit_label_define saves the port label without emitting.
 * emit_jmp flushes one 4-column ;-fused line: LABEL: ; body ; jXX target */
static char g_fmt_label[BB_LABEL_NAME_MAX + 4];  /* "LABEL:" or "" */
static char g_fmt_body[512];                       /* accumulated col2+col3 */

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
    /* Emit: COL1(24) COL2(16) COL3(body ; jXX target) */
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

bb_buf_t        bb_emit_buf  = NULL;
int             bb_emit_pos  = 0;
int             bb_emit_size = 0;

bb_patch_t      bb_patch_list[BB_PATCH_MAX];
int             bb_patch_count = 0;

/* Set by emit_macro_begin in TEXT mode; cleared by emit_macro_end.
 * When set, t_* body helpers are no-ops: TEXT mode emits only the macro
 * invocation line (via emit_macro_begin) — the body instructions are the
 * macro definition (MACRO_DEF) or the BINARY emission path, not TEXT. */
static int g_in_text_macro_body = 0;

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_mode_set(bb_emit_mode_t m, FILE *out)
{
    bb_emit_mode = m;
    bb_emit_out  = out;
}

/*
 * Each helper below contains the full three-way decision.  Templates call
 * them as plain C functions (no `e` argument, no vtable indirection).
 * The mode is bb_emit_mode, set by emit_mode_set() at the top of the pass.
 *
 * "Do nothing" is one of the three things a helper may do (e.g. comments
 * are no-ops in binary mode; instruction encoding is a no-op in macro_def
 * mode if the macro body is defined elsewhere).
 *
 * Helpers are added incrementally as templates are ported.  Each call site
 * in the templates points at the free-standing helper instead of e->method.
 */

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_comment(const char *text)
{
    FILE *f;
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_flush_pending_cjmp_only();
        f = bb_emit_out ? bb_emit_out : stdout;
        fprintf(f, "    # %s\n", text ? text : "");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_box_banner(const char *kind, const char *args)
{
    FILE *f;
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_flush_pending_cjmp_only();
        f = bb_emit_out ? bb_emit_out : stdout;
        fputc('#', f);
        for (int i = 1; i < 120; i++) fputc('-', f);
        fputc('\n', f);
        fprintf(f, "    # BOX %s(%s)\n",
                kind ? kind : "?", args ? args : "");
        return;
    }
}

/*
 * Each instruction helper renders the same x86 effect three ways.
 * BINARY:    write the exact byte sequence into bb_emit_buf via bb_emit_byte.
 * TEXT:      write a three-column GAS line via bb3c_format(out, "", mn, args).
 * MACRO_DEF: write the instruction as a line of a `.macro` body — currently
 *            the same form as TEXT (GAS syntax inside .macro is the same
 *            as outside); future shape with `\param` substitution will
 *            diverge here.
 */

static FILE *emit_outf(void) { return bb_emit_out ? bb_emit_out : stdout; }

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_inc_mem_r13_disp8(uint8_t disp)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */ {
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

/* EM-BB-PURGE-1 / EDP-6 — C-ABI wrapper helpers for brokered blobs.
 * emit_brokered_prologue: push rbp; mov rbp, rsp
 * emit_brokered_epilogue_ret(result): mov eax, result; pop rbp; ret
 * The broker calls fn(zeta, port) via C call; these establish/tear down the
 * frame around the flat BB body (which is identical in WIRED and BROKERED). */

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_brokered_prologue(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_BROKERED:
    case EMIT_BINARY_WIRED:
        bb_emit_byte(0x55);                              /* push rbp */
        bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0xE5); /* mov rbp, rsp */
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
        bb_emit_byte(0x5D);   /* pop rbp */
        bb_emit_byte(0xC3);   /* ret */
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
    /* No-op in all three modes today: mode-3 uses variable-size blobs
     * (per-pc address table in g_blob_addrs[]); mode-4 has no fixed-blob
     * concept; macro_def bodies don't carry blob sizes.  Kept as a hook
     * for a future architecture where fixed-size dispatch slots return. */
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
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
        g_in_text_macro_body = 1;  /* suppress body t_* calls until emit_macro_end */
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
        g_in_text_macro_body = 1;  /* allow body t_* output until emit_macro_end */
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_macro_end(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        return;
    case EMIT_TEXT_INLINE: return;
    case EMIT_TEXT:
        g_in_text_macro_body = 0;  /* body suppression ends here */
        return;
    case EMIT_MACRO_DEF:
        bb3c_flush_pending_cjmp_only();
        fputs(".endm\n", emit_outf());
        g_in_text_macro_body = 0;  /* body ends */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        switch (kind) {
        case JMP_JMP: bb_insn_jmp_rel32(target);  return;
        case JMP_JE:  bb_insn_je_rel32(target);   return;   /* rel32: no overflow on large blobs */
        case JMP_JNE: bb_insn_jne_rel32(target);  return;   /* rel32: no overflow on large blobs */
        case JMP_JL:  bb_insn_jl_rel32(target);   return;   /* rel32: no overflow on large blobs */
        case JMP_JGE: bb_insn_jge_rel32(target);  return;   /* rel32: no overflow on large blobs */
        case JMP_JG:  bb_insn_jg_rel32(target);   return;
        }
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_lea_rdi_strtab_sym(const char *sym_label, uint64_t in_proc_ptr)
{
    /* Load address of a strtab string into rdi.
     *
     *   BINARY:    movabs rdi, <in_proc_ptr>    — 10 bytes: 48 BF <8>
     *              In-process JIT: the strtab string is already in memory;
     *              bake its address directly.  Same encoding as emit_mov_rdi_imm64.
     *
     *   TEXT:      lea rdi, [rip + sym_label]   — 7 bytes at link time;
     *              GAS assembles this as a RIP-relative LEA (opcode 48 8D 3D).
     *
     *   MACRO_DEF: `lea rdi, [rip + \lbl]`      — same as TEXT but \lbl
     *              is the macro parameter name, not a concrete label. */
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        emit_mov_rdi_imm64(in_proc_ptr);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;  /* TEXT: suppress macro body */
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
    /* Load address of a strtab string into rdx.  Parallel to
     * emit_lea_rdi_strtab_sym; used for the third argument (namelist) in
     * SM_PAT_CAPTURE_FN and SM_PAT_CAPTURE_FN_ARGS templates.
     *
     *   BINARY:    movabs rdx, <in_proc_ptr>  — 10 bytes: 48 BA <8>
     *   TEXT:      lea rdx, [rip + sym_label]
     *   MACRO_DEF: lea rdx, [rip + \namelist_lbl] */
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */ {
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
        if (g_in_text_macro_body) return;  /* TEXT: suppress macro body */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */ {
        uint32_t u = (uint32_t)val;
        bb_emit_byte(0xBA);
        bb_emit_byte((uint8_t)(u      )); bb_emit_byte((uint8_t)(u >>  8));
        bb_emit_byte((uint8_t)(u >> 16)); bb_emit_byte((uint8_t)(u >> 24));
        return;
    }
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;  /* TEXT: suppress macro body */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */ {
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
        if (g_in_text_macro_body) return;  /* TEXT: suppress macro body */
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
    /* jz .Lretskip_<pc>  — conditional jump to the skip label.
     *   BINARY:    0F 84 <rel32>  (forward ref; patch needed — not wired yet)
     *   TEXT:      jz  .Lretskip_<pc>
     *   MACRO_DEF: jz  .Lretskip_\pc\()   (GAS param ref + empty paste) */
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        /* Mode-3 uses standard_blob; template not called in BINARY mode.
         * Emit NOP placeholder so binary mode is at least structurally safe. */
        bb_emit_byte(0x90);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;  /* TEXT: suppress macro body */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;  /* TEXT: suppress macro body */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */ {
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
        if (g_in_text_macro_body) return;  /* TEXT: suppress macro body */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        /* BINARY path uses standard_blob; this helper is TEXT/MACRO_DEF only. */
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
    /* NOOP shape: emit one three-column line with macro_name in col 2.
     * The .LpcN: label preceding this line is consumed by bb3c_format's
     * label-pickup path; the macro body is empty so it assembles to nothing.
     *   BINARY:    no-op — label is placed by bb_label_define; no bytes emitted.
     *   TEXT:      three-column line, col2 = macro_name, no operands.
     *   MACRO_DEF: same as TEXT. */
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
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
void emit_banner_stno(int stno, int lineno, const char *src_text)
{
#define STNO_RULE \
    "#=======================================================================================================================\n"
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        FILE *f = emit_outf();
        bb3c_flush_pending();
        fputs(STNO_RULE, f);
        if (src_text && *src_text)
            fprintf(f, "# stmt %d  (line %d):  %s\n", stno, lineno, src_text);
        else if (lineno > 0)
            fprintf(f, "# stmt %d  (line %d)\n", stno, lineno);
        else
            fprintf(f, "# stmt %d\n", stno);
        fputs(STNO_RULE, f);
        return;
    }
    }
#undef STNO_RULE
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_label_init(bb_label_t *lbl, const char *name)
{
    strncpy(lbl->name, name, BB_LABEL_NAME_MAX - 1);
    lbl->name[BB_LABEL_NAME_MAX - 1] = '\0';
    lbl->offset = BB_LABEL_UNRESOLVED;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_label_initf(bb_label_t *lbl, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(lbl->name, BB_LABEL_NAME_MAX, fmt, ap);
    va_end(ap);
    lbl->offset = BB_LABEL_UNRESOLVED;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_label_define(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        FILE *f = bb_emit_out ? bb_emit_out : stdout;
        char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", lbl->name);
        bb3c_format(f, lbuf, "", "");
        return;
    }

    lbl->offset = bb_emit_pos;

    for (int i = 0; i < bb_patch_count; i++) {
        bb_patch_t *p = &bb_patch_list[i];
        if (p->label != lbl) continue;

        int target = lbl->offset;

        if (p->kind == PATCH_REL8) {
            int disp = target - (p->site + 1);
            if (disp < -128 || disp > 127) {
                fprintf(stderr,
                        "bb_label_define: rel8 overflow for '%s': disp=%d\n",
                        lbl->name, disp);
                abort();
            }
            bb_emit_buf[p->site] = (uint8_t)(int8_t)disp;
        } else {
            int disp = target - (p->site + 4);
            uint32_t u;
            memcpy(&u, &disp, 4);
            bb_emit_buf[p->site + 0] = (uint8_t)(u      );
            bb_emit_buf[p->site + 1] = (uint8_t)(u >>  8);
            bb_emit_buf[p->site + 2] = (uint8_t)(u >> 16);
            bb_emit_buf[p->site + 3] = (uint8_t)(u >> 24);
        }

        bb_patch_list[i] = bb_patch_list[--bb_patch_count];
        i--;   /* re-check this slot */
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_begin(bb_buf_t buf, int size)
{
    bb_emit_buf   = buf;
    bb_emit_pos   = 0;
    bb_emit_size  = size;
    bb_patch_count = 0;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
int bb_emit_end(void)
{
    if (bb_patch_count > 0) {
        fprintf(stderr, "bb_emit_end: %d unresolved forward reference(s):\n",
                bb_patch_count);
        for (int i = 0; i < bb_patch_count; i++)
            fprintf(stderr, "  site=%d label='%s'\n",
                    bb_patch_list[i].site,
                    bb_patch_list[i].label->name);
        abort();
    }
    return bb_emit_pos;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_patch_rel8(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        fprintf(stderr,
                "bb_emit_patch_rel8: TEXT-mode reach (target='%s') — "
                "use bb_insn_*_rel8 mnemonic helpers\n",
                lbl->name);
        abort();
    }
    if (bb_label_defined(lbl)) {
        int disp = lbl->offset - (bb_emit_pos + 1);
        if (disp < -128 || disp > 127) {
            fprintf(stderr,
                    "bb_emit_patch_rel8: rel8 overflow for '%s': disp=%d\n",
                    lbl->name, disp);
            abort();
        }
        bb_emit_byte((uint8_t)(int8_t)disp);
        return;
    }
    if (bb_patch_count >= BB_PATCH_MAX) {
        fprintf(stderr, "bb_emit_patch_rel8: patch list full\n");
        abort();
    }
    bb_patch_list[bb_patch_count].site  = bb_emit_pos;
    bb_patch_list[bb_patch_count].label = lbl;
    bb_patch_list[bb_patch_count].kind  = PATCH_REL8;
    bb_patch_count++;
    bb_emit_byte(0x00);   /* placeholder */
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_patch_rel32(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        fprintf(stderr,
                "bb_emit_patch_rel32: TEXT-mode reach (target='%s') — "
                "use bb_insn_*_rel32 mnemonic helpers\n",
                lbl->name);
        abort();
    }
    if (bb_label_defined(lbl)) {
        int disp = lbl->offset - (bb_emit_pos + 4);
        bb_emit_i32(disp);
        return;
    }
    if (bb_patch_count >= BB_PATCH_MAX) {
        fprintf(stderr, "bb_emit_patch_rel32: patch list full\n");
        abort();
    }
    bb_patch_list[bb_patch_count].site  = bb_emit_pos;
    bb_patch_list[bb_patch_count].label = lbl;
    bb_patch_list[bb_patch_count].kind  = PATCH_REL32;
    bb_patch_count++;
    bb_emit_u32(0x00000000);   /* placeholder */
}

/*
 * EM-7b: byte primitives are dual-mode.
 *
 * BINARY: write to bb_emit_buf, advance bb_emit_pos, bounds-check.
 * TEXT:   emit ".byte 0xNN" line to bb_emit_out.  No buffer, no
 *         bounds check.  bb_emit_pos still advances so that any code
 *         interested in "how far have I emitted" sees a consistent
 *         counter — but TEXT-mode label patching does not need it
 *         (jumps go through symbolic helpers like bb_insn_jmp_rel32
 *         which emit "jmp <name>" and let the assembler resolve).
 *
 * Path 1 from the EM-7b scoping audit (sess #74): preserves bb_flat.c
 * call sites unchanged.  The resulting .s contains walls of .byte
 * directives — acceptable for now per the readability standard
 * (mode-4 readability lives in the SM-side emitter; BB-box bytes
 * may be raw .byte sequences linked via external α/β/γ/ω labels).
 */

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_byte(uint8_t b)
{
    if (bb_emit_mode != EMIT_BINARY_WIRED) {
        fprintf(stderr,
                "bb_emit_byte: non-BINARY-mode reach (mode=%d, b=0x%02x) — "
                "convert caller to a named bb_insn_* helper\n",
                (int)bb_emit_mode, (unsigned)b);
        abort();
    }
    if (bb_emit_pos >= bb_emit_size) {
        fprintf(stderr, "bb_emit_byte: buffer overflow at pos=%d size=%d\n",
                bb_emit_pos, bb_emit_size);
        abort();
    }
    bb_emit_buf[bb_emit_pos++] = b;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_u16(uint16_t v)
{
    bb_emit_byte((uint8_t)(v     ));
    bb_emit_byte((uint8_t)(v >> 8));
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_u32(uint32_t v)
{
    bb_emit_byte((uint8_t)(v      ));
    bb_emit_byte((uint8_t)(v >>  8));
    bb_emit_byte((uint8_t)(v >> 16));
    bb_emit_byte((uint8_t)(v >> 24));
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_emit_u64(uint64_t v)
{
    bb_emit_u32((uint32_t)(v      ));
    bb_emit_u32((uint32_t)(v >> 32));
}

void bb_emit_i8(int8_t v)   { bb_emit_byte((uint8_t)v); }
void bb_emit_i32(int32_t v) { uint32_t u; memcpy(&u, &v, 4); bb_emit_u32(u); }

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_text(const char *fmt, ...)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_text_label(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        FILE *f = bb_emit_out ? bb_emit_out : stdout;
        char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", lbl->name);
        bb3c_format(f, lbuf, "", "");
    } else {
        bb_label_define(lbl);
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_text_comment(const char *fmt, ...)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    fprintf(f, "; ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
}

/* ── BB three-column line emission (EM-7c-bb-three-column) ──────────────────
 *
 *   LABEL:                  ACTION           GOTO
 *   col 1 (24 wide)         col 2 (16 wide)  col 3 (free)
 *
 * EM-7c-s-file-beautify (2026-05-09): removed the literal `;` separators
 * that the prior PARTIAL rung introduced.  The shape now matches SM-side
 * (`emit_three_column_line` in sm_codegen_x64_emit.c, sm_line, and
 * emit_sm_template's `render_call_line`) — one printf format
 * `%-24s%-16s %s\n` shared across the entire `.s` file.
 *
 * NULL or empty args render as whitespace of the right column width.
 * Trailing whitespace is preserved (cheap; keeps every line the same
 * shape; readers can strip if desired).  Each line ends with one '\n'.
 *
 * Examples:
 *   bb3c_format(f, "_α:",     "",                "");           // pure label
 *   bb3c_format(f, "",        "lea r10, [rip+Δ]","");           // pure action
 *   bb3c_format(f, "",        "",                "jmp _β");     // pure goto
 *   bb3c_format(f, "_body:",  "cmp esi, 0",      "je _α_body"); // merged
 */
/* EM-FORMAT-BB lone-label fusion (2026-05-09):
 *
 * Pending-label buffer.  When a caller emits a label-only line (col-1
 * has content; col-2 and col-3 empty), we hold it instead of writing.
 * The next call with non-empty col-2 or col-3 fuses the pending label
 * into its col-1 (if the caller passed an empty col-1) or flushes the
 * pending label as a standalone line first (if the caller passed a
 * non-empty col-1, e.g. for a multi-label chain at the same address).
 *
 * Multiple consecutive label-only calls form a chain at the same
 * address: each new label-only call flushes the prior pending label
 * as a standalone line and replaces it.  The last label in the chain
 * is the one that fuses with the first content line that follows.
 *
 * `bb3c_flush_pending()` MUST be called at end-of-file before closing
 * `out` to flush any trailing pending label.  Currently called from
 * sm_codegen_x64_emit's emit-finalize path (added EM-FORMAT-BB).
 *
 * The buffer is keyed off a single static FILE*; if a different output
 * stream comes in mid-emission, the prior pending label is flushed to
 * its original stream first.  Multi-stream emission isn't expected in
 * mode-4 today; this is defensive.
 */

static char  g_bb3c_pending_label[256] = "";
static FILE *g_bb3c_pending_out          = NULL;

/* EM-FORMAT-BB-FUSED-GOTOS (sess 2026-05-09):
 * Deferred-emit buffer for conditional jumps so that an immediately-
 * following unconditional jmp can fuse with the prior conditional
 * into a single line: `<cond_mn>  <succ_target> ; jmp <fail_target>`.
 *
 * The pattern arises 33 times in bb_flat.c as adjacent
 *   EV_JMP(succ, JMP_J*); EV_JMP(fail, JMP_JMP);
 * where the first is conditional (JE/JNE/JL/JGE/JG) and the second is
 * the unconditional fallthrough.  Reads as one "if cond goto succ
 * else goto fail" decision.
 *
 * Flush rules: the pending cond-jmp must be flushed standalone before
 * any non-jmp content (label, action, directive, banner, file end).
 * `bb3c_format` calls `bb3c_flush_pending_cond_jmp` at its top.  The
 * flush writes the cond-jmp via the low-level `bb3c_write_line` to
 * avoid recursing back into bb3c_format. */
static char  g_bb3c_pending_cjmp_mn[16]      = "";
static char  g_bb3c_pending_cjmp_target[256] = "";
static FILE *g_bb3c_pending_cjmp_out         = NULL;

/* EM-FORMAT-BB-LAW (sess 2026-05-09):
 * Per archive/BB-GEN-X86-TEXT.md "The Three-Column NASM Layout — The Law":
 *   "Column 3 (goto): col 60+. Semicolon comment OR live `jmp`."
 *
 * The layout is conceptually 3 columns (LABEL / ACTION / GOTO) but the
 * ACTION column carries macro-name + params together.  In our emitter,
 * mnemonic and args live in separate sub-columns (16-wide MNEMONIC,
 * variable-wide ARGS) so the layout is effectively four sub-columns:
 *
 *   col-1 LABEL (24)  col-2 MNEMONIC (16)  col-3 ARGS (27)  col-4 GOTO
 *   file col 0..23    file col 24..39      file col 41..67  file col 68+
 *
 * BB-jxx cases:
 *   (1) Fused cond+uncond: col-2 = cond mnemonic, col-3 = "<succ_target>;",
 *       col-4 = "jmp <fail_target>".  The `;` lands in col-3 after the
 *       cond's arg.  col-4 starts at vcol 68.
 *   (2) Bare label+jmp / standalone uncond: col-2 empty, col-3 empty,
 *       col-4 = "jmp <target>".  jmp lands at vcol 68.
 *   (3) Standalone cond-jmp (defensive, doesn't fire on tracked corpora):
 *       col-2 = cond mnemonic, col-3 = target, col-4 empty.
 *
 * Triple fusion (action+cond+uncond on one line) is NOT done here — it
 * is a future sub-rung (EM-FORMAT-BB-LAW-TRIPLE-FUSION). */
#define BB_COL3_WIDTH 27

/* Count visual columns (display width) of a UTF-8 string.
 * For our purposes: bytes 0x00..0x7F are 1 column each; UTF-8 lead bytes
 * 0xC0..0xFF start a multi-byte sequence whose continuation bytes
 * (0x80..0xBF) DO NOT add to width.  So width = number of bytes that
 * are NOT continuation bytes.  This treats every UTF-8 codepoint as 1
 * column wide — fine for the Greek letters (α/β/γ/ω/Σ/Δ) we use, all
 * of which are visually 1 column in monospace. */
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int bb3c_visual_width(const char *s)
{
    int w = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if ((*p & 0xC0) != 0x80) w++;
    }
    return w;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int bb3c_pad_to_width(char *buf, size_t bufsz, const char *s, int target)
{
    int sw = bb3c_visual_width(s);
    int slen = (int)strlen(s ? s : "");
    if (slen >= (int)bufsz) slen = (int)bufsz - 1;
    int o = 0;
    if (s && slen > 0) {
        memcpy(buf + o, s, (size_t)slen);
        o += slen;
    }
    int pad = target - sw;
    while (pad-- > 0 && o < (int)bufsz - 1) {
        buf[o++] = ' ';
    }
    if (o >= (int)bufsz) o = (int)bufsz - 1;
    buf[o] = '\0';
    return o;
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void bb3c_write_line(FILE *out, const char *L, const char *A, const char *G)
{
    char buf[768];
    int o = 0;
    o += bb3c_pad_to_width(buf + o, sizeof(buf) - o, L ? L : "", 24);
    o += bb3c_pad_to_width(buf + o, sizeof(buf) - o, A ? A : "", 16);
    if (o < (int)sizeof(buf) - 1) buf[o++] = ' ';
    if (G && *G) {
        int gl = (int)strlen(G);
        if (gl > (int)sizeof(buf) - 1 - o) gl = (int)sizeof(buf) - 1 - o;
        memcpy(buf + o, G, (size_t)gl);
        o += gl;
    }
    while (o > 0 && (buf[o-1] == ' ' || buf[o-1] == '\t')) o--;
    buf[o] = '\0';
    fputs(buf, out);
    fputc('\n', out);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void bb3c_flush_pending_cond_jmp(void)
{
    if (g_bb3c_pending_cjmp_mn[0] && g_bb3c_pending_cjmp_out) {
        bb3c_write_line(g_bb3c_pending_cjmp_out, "",
                        g_bb3c_pending_cjmp_mn,
                        g_bb3c_pending_cjmp_target);
        g_bb3c_pending_cjmp_mn[0]     = '\0';
        g_bb3c_pending_cjmp_target[0] = '\0';
        g_bb3c_pending_cjmp_out       = NULL;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void bb3c_flush_pending_to(FILE *target)
{
    /* Flush cond-jmp first: it logically precedes any pending label
     * (the cond-jmp came from an earlier emit; the label is buffered
     * waiting for the next content line). */
    bb3c_flush_pending_cond_jmp();
    if (g_bb3c_pending_label[0] && g_bb3c_pending_out) {
        bb3c_write_line(g_bb3c_pending_out, g_bb3c_pending_label, "", "");
        g_bb3c_pending_label[0] = '\0';
        g_bb3c_pending_out = NULL;
    }
    (void)target; /* reserved for future multi-stream variants */
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb3c_flush_pending_cjmp_only(void)
{
    bb3c_flush_pending_cond_jmp();
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb3c_flush_pending(void)
{
    bb3c_flush_pending_to(NULL);
}

/* EM-FORMAT-BB-FUSED-GOTOS: emit a jmp/cond-jmp line with optional fusion.
 *
 *   bb3c_emit_jmp(out, "jne", "lbl_succ");  -> defer to pending cjmp slot
 *   bb3c_emit_jmp(out, "jmp", "lbl_fail");  -> if cjmp pending, fuse:
 *                                              "<jne>  lbl_succ ; jmp lbl_fail"
 *                                             else emit standalone
 *
 * Single entry point for both `text_emit_jmp` (in emitter_text.c) and
 * `bb_insn_jmp/je/jne/jl/jge/jg_*` (in this file's TEXT-mode arms). */
/*----------------------------------------------------------------------------------------------------------------------------------------*/
static int bb3c_is_cond_jmp(const char *mn)
{
    if (!mn) return 0;
    return (strcmp(mn, "je")  == 0) ||
           (strcmp(mn, "jne") == 0) ||
           (strcmp(mn, "jl")  == 0) ||
           (strcmp(mn, "jge") == 0) ||
           (strcmp(mn, "jg")  == 0) ||
           (strcmp(mn, "jle") == 0) ||
           (strcmp(mn, "jbe") == 0);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb3c_emit_jmp(FILE *out, const char *mn, const char *target)
{
    const char *m = mn ? mn : "";
    const char *t = target ? target : "";

    if (bb3c_is_cond_jmp(m)) {
        if (g_bb3c_pending_cjmp_mn[0] && g_bb3c_pending_cjmp_out != out) {
            bb3c_flush_pending_cond_jmp();
        }
        if (g_bb3c_pending_cjmp_mn[0]) {
            bb3c_flush_pending_cond_jmp();
        }
        snprintf(g_bb3c_pending_cjmp_mn,     sizeof(g_bb3c_pending_cjmp_mn),     "%s", m);
        snprintf(g_bb3c_pending_cjmp_target, sizeof(g_bb3c_pending_cjmp_target), "%s", t);
        g_bb3c_pending_cjmp_out = out;
        return;
    }

    if (g_bb3c_pending_cjmp_mn[0] && g_bb3c_pending_cjmp_out == out) {
        /* EM-FORMAT-BB-LAW (4-column layout):
         *   col-1 LABEL (24)  col-2 ACTION (16)  col-3 ARGS (27)  col-4 GOTO
         * Fused cond+uncond:
         *   col-2 = cond mnemonic (`je`/`jne`/...)
         *   col-3 = "<succ_target>;" (left-anchored, padded to 27 visual cols)
         *   col-4 = "jmp <fail_target>"
         * The `;` lands in col-3 right after the cond's arg; col-4 starts at
         * file vcol 68 so the uncond `jmp` aligns vertically with the GOTO
         * column of bare label+jmp lines below.  ONE space between mnemonic
         * and arg in col-2/col-3 (managed by bb3c_format's separator).
         * ONE space between `jmp` and arg in col-4. */
        char rest[512];
        char col3[288];
        int n = snprintf(col3, sizeof(col3), "%s;",
                         g_bb3c_pending_cjmp_target);
        if (n < 0) n = 0;
        int o = bb3c_pad_to_width(rest, sizeof(rest), col3, BB_COL3_WIDTH);
        snprintf(rest + o, sizeof(rest) - o, "jmp %s", t);
        char saved_mn[16];
        snprintf(saved_mn, sizeof(saved_mn), "%s", g_bb3c_pending_cjmp_mn);
        g_bb3c_pending_cjmp_mn[0]     = '\0';
        g_bb3c_pending_cjmp_target[0] = '\0';
        g_bb3c_pending_cjmp_out       = NULL;
        bb3c_format(out, "", saved_mn, rest);
        return;
    }
    char rest[512];
    int o = bb3c_pad_to_width(rest, sizeof(rest), "", BB_COL3_WIDTH);
    snprintf(rest + o, sizeof(rest) - o, "%s %s", m, t);
    bb3c_format(out, "", "", rest);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb3c_format(FILE *out, const char *label, const char *action, const char *goto_)
{
    if (g_bb3c_pending_cjmp_mn[0]) {
        if (g_bb3c_pending_cjmp_out == out || g_bb3c_pending_cjmp_out == NULL) {
            bb3c_flush_pending_cond_jmp();
        } else {
            bb3c_flush_pending_cond_jmp();
        }
    }

    const char *L = label  ? label  : "";
    const char *A = action ? action : "";
    const char *G = goto_  ? goto_  : "";

    int label_only = (*L) && !(*A) && !(*G);
    int has_content = (*A) || (*G);

    if (g_bb3c_pending_label[0] && g_bb3c_pending_out && g_bb3c_pending_out != out) {
        bb3c_write_line(g_bb3c_pending_out, g_bb3c_pending_label, "", "");
        g_bb3c_pending_label[0] = '\0';
        g_bb3c_pending_out = NULL;
    }

    if (label_only) {
        if (g_bb3c_pending_label[0]) {
            bb3c_write_line(out, g_bb3c_pending_label, "", "");
        }
        snprintf(g_bb3c_pending_label, sizeof(g_bb3c_pending_label), "%s", L);
        g_bb3c_pending_out = out;
        return;
    }

    if (has_content) {
        char fused_lbl[256];
        const char *eff_L = L;
        if (g_bb3c_pending_label[0]) {
            if (!*eff_L) {
                /* Caller didn't supply col-1; fuse pending label into this line. */
                snprintf(fused_lbl, sizeof(fused_lbl), "%s", g_bb3c_pending_label);
                eff_L = fused_lbl;
            } else {
                bb3c_write_line(out, g_bb3c_pending_label, "", "");
            }
            g_bb3c_pending_label[0] = '\0';
            g_bb3c_pending_out = NULL;
        }
        bb3c_write_line(out, eff_L, A, G);
        return;
    }

}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb3c_text(const char *label, const char *action, const char *goto_)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    bb3c_format(f, label, action, goto_);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
static void bb3c_op(const char *mn, const char *fmt, ...)
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
static void bb3c_jmp(const char *mn, const char *target)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    bb3c_emit_jmp(f, mn ? mn : "", target ? target : "");
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_eax_imm32(uint32_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("mov", "eax, %u", imm);
        return;
    }
    bb_emit_byte(0xB8);
    bb_emit_u32(imm);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_mov_rax_imm64(uint64_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("mov", "rax, 0x%llx", (unsigned long long)imm);
        return;
    }
    bb_emit_byte(0x48);
    bb_emit_byte(0xB8);
    bb_emit_u64(imm);
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
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jmp", target->name); return;
    }
    bb_emit_byte(0xEB);
    bb_emit_patch_rel8(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jmp_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jmp", target->name); return;
    }
    bb_emit_byte(0xE9);
    bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jl_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jl", target->name); return;
    }
    bb_emit_byte(0x7C);
    bb_emit_patch_rel8(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jge_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jge", target->name); return;
    }
    bb_emit_byte(0x7D);
    bb_emit_patch_rel8(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_je_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("je", target->name); return;
    }
    bb_emit_byte(0x74);
    bb_emit_patch_rel8(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jne_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jne", target->name); return;
    }
    bb_emit_byte(0x75);
    bb_emit_patch_rel8(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jne_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jne", target->name); return;
    }
    bb_emit_byte(0x0F); bb_emit_byte(0x85);
    bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_je_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("je", target->name); return;
    }
    bb_emit_byte(0x0F); bb_emit_byte(0x84);
    bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jl_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jl", target->name); return;
    }
    bb_emit_byte(0x0F); bb_emit_byte(0x8C);
    bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jge_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jge", target->name); return;
    }
    bb_emit_byte(0x0F); bb_emit_byte(0x8D);
    bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_jg_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jg", target->name); return;
    }
    bb_emit_byte(0x0F); bb_emit_byte(0x8F);
    bb_emit_patch_rel32(target);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_cmp_esi_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("cmp", "esi, %u", (unsigned)imm); return;
    }
    bb_emit_byte(0x83); bb_emit_byte(0xFE); bb_emit_byte(imm);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_cmp_esi_imm32(uint32_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("cmp", "esi, %u", imm); return;
    }
    bb_emit_byte(0x81); bb_emit_byte(0xFE); bb_emit_u32(imm);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_movzx_eax_rdi_off8(uint8_t off)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("movzx", "eax, byte [rdi + %u]", (unsigned)off); return;
    }
    bb_emit_byte(0x0F); bb_emit_byte(0xB6);
    bb_emit_byte(0x47); bb_emit_byte(off);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_cmp_al_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("cmp", "al, %u", (unsigned)imm); return;
    }
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
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("sub", "rsp, %u", (unsigned)imm); return;
    }
    bb_emit_byte(0x48); bb_emit_byte(0x83); bb_emit_byte(0xEC);
    bb_emit_byte(imm);
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void bb_insn_add_rsp_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("add", "rsp, %u", (unsigned)imm); return;
    }
    bb_emit_byte(0x48); bb_emit_byte(0x83); bb_emit_byte(0xC4);
    bb_emit_byte(imm);
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
    /* EM-BB-FORMAT-4: FORMAT mode produces one fused 4-column line per port.
     * Body = "call fn@PLT ; jne lbl_succ"; final jmp lbl_fail flushes.
     * No push/pop r10 or zeta setup in the asm text — the fn handles state. */
    if (emit_bb_is_format_mode()) {
        char call_frag[BB_LABEL_NAME_MAX + 32];
        snprintf(call_frag, sizeof(call_frag), "call %s@PLT", fn_name);
        fmt_body_append(call_frag, "");
        char jne[BB_LABEL_NAME_MAX + 8];
        snprintf(jne, sizeof(jne), "jne %s", lbl_succ->name);
        fmt_body_append(jne, "");
        emit_jmp(lbl_fail, JMP_JMP);  /* flush the accumulated port line */
        return;
    }
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x41); bb_emit_byte(0x52);   /* push r10 */
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
        bb_emit_byte(0x41); bb_emit_byte(0x5A);   /* pop r10 */
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
        /* BINARY: zeta_label unused; fall through to standard port call */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
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
            /* jne fail → body; final jmp succ → flush via emit_jmp */
            char jne[BB_LABEL_NAME_MAX + 8];
            snprintf(jne, sizeof(jne), "jne %s", lbl_fail->name);
            fmt_body_append(jne, "");
            emit_jmp(lbl_succ, JMP_JMP);  /* flush α-port line */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
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
            emit_jmp(lbl_succ, JMP_JMP);  /* flush α-port line */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
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
            /* FORMAT-7: accumulate bounds check fragments; caller provides flush jmp */
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

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_ret(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        bb_emit_byte(0xC3);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", "ret", "");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_push_r10(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x41); bb_emit_byte(0x52);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) { fmt_body_append("push", "r10"); return; }
        bb3c_format(emit_outf(), "", "push", "r10");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_pop_r10(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x41); bb_emit_byte(0x5A);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) { fmt_body_append("pop", "r10"); return; }
        bb3c_format(emit_outf(), "", "pop", "r10");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_test_rax_rax(void)
{
    /* test rax, rax — set ZF from rax; used before conditional jumps.
     *   BINARY:    48 85 C0  (REX.W TEST rax, rax)
     *   TEXT:      `test rax, rax`
     *   MACRO_DEF: same as TEXT */
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        bb_emit_byte(0x48); bb_emit_byte(0x85); bb_emit_byte(0xC0);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        bb3c_format(emit_outf(), "", "test", "rax, rax");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_test_eax_eax(void)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        bb_emit_byte(0x85); bb_emit_byte(0xC0);
        return;
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF:
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (emit_bb_is_format_mode()) { fmt_body_append("test", "eax, eax"); return; }
        bb3c_format(emit_outf(), "", "test", "eax, eax");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_mov_rdi_imm64(uint64_t val)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */ {
        bb_emit_byte(0x48); bb_emit_byte(0xBF);
        bb_emit_byte((uint8_t)(val      ));
        bb_emit_byte((uint8_t)(val >>  8));
        bb_emit_byte((uint8_t)(val >> 16));
        bb_emit_byte((uint8_t)(val >> 24));
        bb_emit_byte((uint8_t)(val >> 32));
        bb_emit_byte((uint8_t)(val >> 40));
        bb_emit_byte((uint8_t)(val >> 48));
        bb_emit_byte((uint8_t)(val >> 56));
        return;
    }
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[64];
        snprintf(args, sizeof(args), "rdi, 0x%llx", (unsigned long long)val);
        bb3c_format(emit_outf(), "", "mov", args);
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_call_sym_plt(const char *sym, uint64_t fn_fallback)
{
    /* call sym@PLT
     *   BINARY: in-process JIT can't reach a real PLT; instead bake the
     *           resolved fn address and call indirect through rax.
     *           Sequence is 12 bytes: 48 B8 <8-byte fn> FF D0
     *           (= mov rax, fn; call rax).  This matches the existing
     *           BB_INSN_CALL_SYM_PLT binary encoding in emitter_binary.c.
     *   TEXT:   emit `call <sym>@PLT` — GAS resolves via PLT at link time.
     *   MACRO_DEF: same form as TEXT (sym is a fixed name in the macro). */
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */ {
        bb_emit_byte(0x48); bb_emit_byte(0xB8);
        bb_emit_byte((uint8_t)(fn_fallback      ));
        bb_emit_byte((uint8_t)(fn_fallback >>  8));
        bb_emit_byte((uint8_t)(fn_fallback >> 16));
        bb_emit_byte((uint8_t)(fn_fallback >> 24));
        bb_emit_byte((uint8_t)(fn_fallback >> 32));
        bb_emit_byte((uint8_t)(fn_fallback >> 40));
        bb_emit_byte((uint8_t)(fn_fallback >> 48));
        bb_emit_byte((uint8_t)(fn_fallback >> 56));
        bb_emit_byte(0xFF); bb_emit_byte(0xD0);
        return;
    }
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_TEXT && g_in_text_macro_body) return;
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[80];
        snprintf(args, sizeof(args), "%s@PLT", sym ? sym : "??sym??");
        if (emit_bb_is_format_mode()) { fmt_body_append("call", args); return; }
        bb3c_format(emit_outf(), "", "call", args);
        return;
    }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_mov_esi_imm32(int val)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */ {
        uint32_t u = (uint32_t)val;
        bb_emit_byte(0xBE);
        bb_emit_byte((uint8_t)(u      ));
        bb_emit_byte((uint8_t)(u >>  8));
        bb_emit_byte((uint8_t)(u >> 16));
        bb_emit_byte((uint8_t)(u >> 24));
        return;
    }
    case EMIT_TEXT_INLINE:
    case EMIT_TEXT: {
        if (g_in_text_macro_body) return;  /* TEXT: suppress macro body */
        if (bb_emit_mode == EMIT_MACRO_DEF && !g_in_text_macro_body) return;
        char args[32];
        snprintf(args, sizeof(args), "esi, %d", val);
        bb3c_format(emit_outf(), "", "mov", args);
        return;
    }
    case EMIT_MACRO_DEF:
        bb3c_format(emit_outf(), "", "mov", "esi, \\n");
        return;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------*/
void emit_add_delta_imm(int v)
{
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        bb_emit_byte(0x41); bb_emit_byte(0x8B); bb_emit_byte(0x02);
        bb_emit_byte(0x05);
        bb_emit_byte((uint8_t)((uint32_t)v      ));
        bb_emit_byte((uint8_t)((uint32_t)v >>  8));
        bb_emit_byte((uint8_t)((uint32_t)v >> 16));
        bb_emit_byte((uint8_t)((uint32_t)v >> 24));
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
    case EMIT_BINARY_BROKERED:  /* stub: same as WIRED until EM-BB-PURGE-1 */
        bb_emit_byte(0x41); bb_emit_byte(0x8B); bb_emit_byte(0x02);
        bb_emit_byte(0x2D);
        bb_emit_byte((uint8_t)((uint32_t)v      ));
        bb_emit_byte((uint8_t)((uint32_t)v >>  8));
        bb_emit_byte((uint8_t)((uint32_t)v >> 16));
        bb_emit_byte((uint8_t)((uint32_t)v >> 24));
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
