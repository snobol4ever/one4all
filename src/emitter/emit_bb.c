#include "emit_bb.h"
#include "emit_form.h"
#include "emit_templates.h"
#include "emit_globals.h"
#include "../runtime/interp/icon_gen.h"
#include "BB.h"
#include "../rt/rt.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
extern const char * Σ;
extern int          Σlen;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t    bb_deferred_var_exported  (void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t    icn_bb_alternate         (void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t    icn_bb_every             (void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t    icn_bb_iterate           (void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t    icn_bb_limit             (void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t    icn_bb_to                (void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t    icn_bb_to_by             (void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_not(void*,int);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_repalt(void*,int);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_while_gen(void*,int);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_until_gen(void*,int);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_repeat_gen(void*,int);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_case_gen(void*,int);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_compound_gen(void*,int);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_field_gen(void*,int);     extern icn_field_gen_state_t   *icon_field_gen_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_section_gen(void*,int);   extern icn_section_gen_state_t *icon_section_gen_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_key_gen(void*,int);       extern icn_kw_gen_state_t      *icon_kw_gen_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_listcon_gen(void*,int);   extern icn_listcon_state_t     *icon_listcon_gen_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_proc_call(void*,int);      extern icn_proc_call_state_t   *icon_proc_call_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_noop(void*,int);          extern icn_noop_state_t        *icon_noop_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_intlit(void*,int);        extern icn_intlit_state_t      *icon_intlit_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_reallit(void*,int);       extern icn_reallit_state_t     *icon_reallit_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_strlit(void*,int);        extern icn_strlit_state_t      *icon_strlit_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_csetlit(void*,int);       extern icn_csetlit_state_t     *icon_csetlit_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_global(void*,int);        extern icn_global_state_t      *icon_global_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_if_bb(void*,int);         extern icn_if_state_t          *icon_if_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_initial(void*,int);       extern icn_initial_state_t     *icon_initial_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_invocable(void*,int);     extern icn_invocable_state_t   *icon_invocable_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_link(void*,int);          extern icn_link_state_t        *icon_link_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_record_bb(void*,int);     extern icn_record_state_t      *icon_record_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_return_bb(void*,int);     extern icn_return_state_t      *icon_return_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_fail_bb(void*,int);       extern icn_fail_state_t        *icon_fail_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_unop(void*,int);          extern icn_unop_state_t        *icon_unop_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_next_bb(void*,int);       extern icn_next_state_t        *icon_next_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_break_bb(void*,int);      extern icn_break_state_t       *icon_break_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_create(void*,int);        extern icn_create_state_t      *icon_create_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_coexplist(void*,int);     extern icn_coexplist_state_t   *icon_coexplist_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_arglist(void*,int);       extern icn_arglist_state_t     *icon_arglist_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_procdecl(void*,int);      extern icn_procdecl_state_t    *icon_procdecl_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_procbody(void*,int);      extern icn_procbody_state_t    *icon_procbody_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t icn_bb_proccode(void*,int);      extern icn_proccode_state_t    *icon_proccode_new(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern atp_t    * bb_atp_new                (const char *varname);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern cap_t    * bb_cap_new_call           (bb_box_fn child_fn, void *child_state, const char *fnc_name, DESCR_t *fnc_args, int fnc_nargs, char **fnc_arg_names, int fnc_n_arg_names, int immediate);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void     * bb_dvar_bin_new           (const char *name);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_bb_jmp_pair(const char *banner, bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_beta, int beta_first) {
    (void)lbl_succ;
    emit_bb_box_banner(banner, "");
    if (beta_first) { emit_label_define(lbl_beta); emit_jmp(lbl_fail, JMP_JMP); emit_jmp(lbl_fail, JMP_JMP); }
    else            { emit_jmp(lbl_fail, JMP_JMP); emit_label_define(lbl_beta); emit_jmp(lbl_fail, JMP_JMP); }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_bb_ptr_slot(char *zlbl_out) {
    int id = g_flat_node_id++;
    snprintf(zlbl_out, 80, ".Lrtc%d_z", id);
    char zlbl_def[88]; snprintf(zlbl_def, sizeof(zlbl_def), "%s:", zlbl_out);
    FILE *out = emit_outf();
    bb3c_format(out, "",       ".section", ".data");
    bb3c_format(out, zlbl_def, ".quad",    "0");
    bb3c_format(out, "", ".section", ".text");
    bb3c_format(out, "", ".intel_syntax", "noprefix");
}
void  emit_bb_xfail(bb_label_t *s, bb_label_t *f, bb_label_t *b)       { emit_bb_jmp_pair("FAIL",   s, f, b, 0); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void  emit_bb_xeps (bb_label_t *s, bb_label_t *f, bb_label_t *b)       { emit_bb_box_banner("EPS",""); emit_jmp(s, JMP_JMP); emit_label_define(b); emit_jmp(f, JMP_JMP); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xfarb(bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    emit_bb_box_banner("ARB", "");
    if (IS_TEXT) {
        int id = g_flat_node_id++;
        char zlbl[80], zlbl_def[88];
        snprintf(zlbl,     sizeof(zlbl),     ".Larb%d_z", id);
        snprintf(zlbl_def, sizeof(zlbl_def), "%s:", zlbl);
        FILE *out = emit_outf();
        bb3c_format(out, "",       ".section", ".data");
        bb3c_format(out, zlbl_def, ".long",    "0");
        bb3c_format(out, "",       ".long",    "0");
        bb3c_format(out, "",       ".section", ".text");
        bb3c_format(out, "",       ".intel_syntax", "noprefix");
        char lcnt[80], lstart[80];
        snprintf(lcnt,   sizeof(lcnt),   "%s + 0", zlbl);
        snprintf(lstart, sizeof(lstart), "%s + 4", zlbl);
        bb3c_format(out, "", "lea",  "rax, [rip + Δ]");
        bb3c_format(out, "", "mov",  "ecx, dword ptr [rax]");
        char cnt_store[160]; snprintf(cnt_store, sizeof(cnt_store), "dword ptr [rip + %s], 0", lcnt);
        bb3c_format(out, "", "mov",  cnt_store);
        char start_store[160]; snprintf(start_store, sizeof(start_store), "rax, [rip + %s]", lstart);
        bb3c_format(out, "", "lea",  start_store);
        bb3c_format(out, "", "mov",  "dword ptr [rax], ecx");
        char jmp_succ[128]; snprintf(jmp_succ, sizeof(jmp_succ), "%s", s->name);
        bb3c_format(out, "", "jmp",  jmp_succ);
        emit_label_define(b);
        char cnt_ref[160]; snprintf(cnt_ref, sizeof(cnt_ref), "rax, [rip + %s]", lcnt);
        bb3c_format(out, "", "lea",  cnt_ref);
        bb3c_format(out, "", "mov",  "ecx, dword ptr [rax]");
        bb3c_format(out, "", "inc",  "ecx");
        bb3c_format(out, "", "mov",  "dword ptr [rax], ecx");
        char sref[160]; snprintf(sref, sizeof(sref), "rax, [rip + %s]", lstart);
        bb3c_format(out, "", "lea",  sref);
        bb3c_format(out, "", "mov",  "edx, dword ptr [rax]");
        bb3c_format(out, "", "add",  "edx, ecx");
        bb3c_format(out, "", "lea",  "rax, [rip + Σlen]");
        bb3c_format(out, "", "cmp",  "edx, dword ptr [rax]");
        char jmp_fail[128]; snprintf(jmp_fail, sizeof(jmp_fail), "%s", f->name);
        bb3c_format(out, "", "jg",   jmp_fail);
        bb3c_format(out, "", "lea",  "rax, [rip + Δ]");
        bb3c_format(out, "", "mov",  "dword ptr [rax], edx");
        bb3c_format(out, "", "jmp",  jmp_succ);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xstar(bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    emit_bb_box_banner("REM", "");
    if (IS_TEXT) {
        FILE *out = emit_outf();
        bb3c_format(out, "", ".intel_syntax", "noprefix");
        bb3c_format(out, "", "lea", "rax, [rip + Σlen]");
        bb3c_format(out, "", "mov", "ecx, dword ptr [rax]");
        bb3c_format(out, "", "lea", "rax, [rip + Δ]");
        bb3c_format(out, "", "mov", "dword ptr [rax], ecx");
        char jmp_succ[128]; snprintf(jmp_succ, sizeof(jmp_succ), "%s", s->name);
        bb3c_format(out, "", "jmp", jmp_succ);
        emit_label_define(b);
        char jmp_fail[128]; snprintf(jmp_fail, sizeof(jmp_fail), "%s", f->name);
        bb3c_format(out, "", "jmp", jmp_fail);
        return;
    }
    insn_mov_rcx_i64(TEMPLATE_ADDR_SIGLEN);
    insn_mov_eax_rcxmem();
    emit_store_delta();
    emit_jmp(s, JMP_JMP);
    emit_label_define(b);
    emit_jmp(f, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xlnth(long long n, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    char nbuf[32]; snprintf(nbuf, sizeof(nbuf), "%d", (int)n);
    emit_bb_box_banner("LEN", nbuf);
    if (IS_TEXT) {
        FILE *out = emit_outf();
        bb3c_format(out, "", ".intel_syntax", "noprefix");
        char narg[64]; snprintf(narg, sizeof(narg), "eax, dword ptr [rax]");
        bb3c_format(out, "", "lea", "rax, [rip + Δ]");
        bb3c_format(out, "", "mov", narg);
        char addarg[64]; snprintf(addarg, sizeof(addarg), "eax, %d", (int)n);
        bb3c_format(out, "", "add", addarg);
        bb3c_format(out, "", "lea", "rcx, [rip + Σlen]");
        bb3c_format(out, "", "cmp", "eax, dword ptr [rcx]");
        char jmp_fail[128]; snprintf(jmp_fail, sizeof(jmp_fail), "%s", f->name);
        bb3c_format(out, "", "jg",  jmp_fail);
        bb3c_format(out, "", "lea", "rax, [rip + Δ]");
        bb3c_format(out, "", "mov", "ecx, dword ptr [rax]");
        char addarg2[64]; snprintf(addarg2, sizeof(addarg2), "ecx, %d", (int)n);
        bb3c_format(out, "", "add", addarg2);
        bb3c_format(out, "", "mov", "dword ptr [rax], ecx");
        char jmp_succ[128]; snprintf(jmp_succ, sizeof(jmp_succ), "%s", s->name);
        bb3c_format(out, "", "jmp", jmp_succ);
        emit_label_define(b);
        bb3c_format(out, "", "jmp", jmp_fail);
        return;
    }
    emit_seq_bounds_len((int)n, TEMPLATE_ADDR_SIGLEN, f);
    emit_add_delta_imm((int)n);
    emit_jmp(s, JMP_JMP);
    emit_label_define(b);
    emit_jmp(f, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xtb(long long n, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    char nbuf[32]; snprintf(nbuf, sizeof(nbuf), "%d", (int)n);
    emit_bb_box_banner("TAB", nbuf);
    if (IS_TEXT) {
        FILE *out = emit_outf();
        bb3c_format(out, "", ".intel_syntax", "noprefix");
        bb3c_format(out, "", "lea", "rax, [rip + Δ]");
        bb3c_format(out, "", "mov", "ecx, dword ptr [rax]");
        char cmparg[64]; snprintf(cmparg, sizeof(cmparg), "ecx, %d", (int)n);
        bb3c_format(out, "", "cmp", cmparg);
        char jmp_fail[128]; snprintf(jmp_fail, sizeof(jmp_fail), "%s", f->name);
        bb3c_format(out, "", "jg",  jmp_fail);
        char movarg[64]; snprintf(movarg, sizeof(movarg), "dword ptr [rax], %d", (int)n);
        bb3c_format(out, "", "mov", movarg);
        char jmp_succ[128]; snprintf(jmp_succ, sizeof(jmp_succ), "%s", s->name);
        bb3c_format(out, "", "jmp", jmp_succ);
        emit_label_define(b);
        bb3c_format(out, "", "jmp", jmp_fail);
        return;
    }
    insn_mov_eax_r10mem();
    insn_cmp_eax_i32((uint32_t)n);
    emit_jmp(f, JMP_JG);
    insn_mov_eax_i32((uint32_t)n);
    emit_store_delta();
    emit_jmp(s, JMP_JMP);
    emit_label_define(b);
    emit_jmp(f, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xrtb(long long n, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    char nbuf[32]; snprintf(nbuf, sizeof(nbuf), "%d", (int)n);
    emit_bb_box_banner("RTAB", nbuf);
    if (IS_TEXT) {
        FILE *out = emit_outf();
        bb3c_format(out, "", ".intel_syntax", "noprefix");
        bb3c_format(out, "", "lea", "rax, [rip + Σlen]");
        bb3c_format(out, "", "mov", "ecx, dword ptr [rax]");
        char subarg[64]; snprintf(subarg, sizeof(subarg), "ecx, %d", (int)n);
        bb3c_format(out, "", "sub", subarg);
        bb3c_format(out, "", "lea", "rax, [rip + Δ]");
        bb3c_format(out, "", "cmp", "ecx, dword ptr [rax]");
        char jmp_fail[128]; snprintf(jmp_fail, sizeof(jmp_fail), "%s", f->name);
        bb3c_format(out, "", "jl",  jmp_fail);
        bb3c_format(out, "", "mov", "dword ptr [rax], ecx");
        char jmp_succ[128]; snprintf(jmp_succ, sizeof(jmp_succ), "%s", s->name);
        bb3c_format(out, "", "jmp", jmp_succ);
        emit_label_define(b);
        bb3c_format(out, "", "jmp", jmp_fail);
        return;
    }
    insn_mov_rcx_i64(TEMPLATE_ADDR_SIGLEN);
    insn_mov_eax_rcxmem();
    insn_sub_eax_i32((uint32_t)n);
    insn_mov_ecx_eax();
    insn_mov_eax_r10mem();
    insn_cmp_eax_ecx();
    emit_jmp(f, JMP_JG);
    insn_mov_rcx_i64(TEMPLATE_ADDR_SIGLEN);
    insn_mov_eax_rcxmem();
    insn_sub_eax_i32((uint32_t)n);
    emit_store_delta();
    emit_jmp(s, JMP_JMP);
    emit_label_define(b);
    emit_jmp(f, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void (*g_cap_fixup_cb)(void *cap_ptr, const char *child_α_label) = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *child_cache_get_lbl(bb_box_fn fn);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void  emit_bb_xarbn(bb_box_fn child_fn, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    void *z = rt_bb_arbno_new(child_fn, NULL);
    emit_bb_box_banner("ARBNO", "");
    if (IS_TEXT) {
        char zlbl[80]; emit_bb_ptr_slot(zlbl);
        const char *clbl = child_fn ? child_cache_get_lbl(child_fn) : NULL;
        if (clbl && g_cap_fixup_cb) { char combo[160]; snprintf(combo, sizeof(combo), "%s|%s", zlbl, clbl); g_cap_fixup_cb((void*)2, combo); }
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_arbno", (uint64_t)(uintptr_t)rt_bb_arbno, 0, s, f);
        emit_label_define(b);
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_arbno", (uint64_t)(uintptr_t)rt_bb_arbno, 1, s, f);
        return;
    }
    emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_arbno", (uint64_t)(uintptr_t)rt_bb_arbno, 0, s, f);
    emit_label_define(b);
    emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_arbno", (uint64_t)(uintptr_t)rt_bb_arbno, 1, s, f);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void  emit_bb_xbrkx(const char *chars, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    if (IS_TEXT) {
        emit_bb_box_banner("BREAKX", chars ? chars : "");
        int id = g_flat_node_id++;
        char slbl[80], zlbl[80], slbl_def[88], zlbl_def[88];
        snprintf(slbl, sizeof(slbl), ".Lbrkx%d_chars", id);
        snprintf(zlbl, sizeof(zlbl), ".Lbrkx%d_z",     id);
        snprintf(slbl_def, sizeof(slbl_def), "%s:", slbl);
        snprintf(zlbl_def, sizeof(zlbl_def), "%s:", zlbl);
        const char *ch = chars ? chars : "";
        char esc[1024]; size_t o = 0;
        if (o < sizeof(esc)) esc[o++] = '"';
        for (const char *cp = ch; *cp && o + 5 < sizeof(esc); cp++) {
            unsigned char c = (unsigned char)*cp;
            if (c == '\"' || c == '\\') { esc[o++] = '\\'; esc[o++] = (char)c; }
            else if (c >= 32 && c < 127) { esc[o++] = (char)c; }
            else { o += snprintf(esc + o, sizeof(esc) - o, "\\%03o", c); }
        }
        if (o + 1 < sizeof(esc)) esc[o++] = '"';
        esc[o] = '\0';
        FILE *out = emit_outf();
        bb3c_format(out, "",       ".section", ".data");
        bb3c_format(out, slbl_def, ".string",  esc);
        bb3c_format(out, zlbl_def, ".quad",    slbl);
        bb3c_format(out, "",       ".long",    "0");
        bb3c_format(out, "",       ".section", ".text");
        bb3c_format(out, "",       ".intel_syntax", "noprefix");
        char lloop[80], ldone[80], lfail_scan[80], linner[80], lno_match[80];
        snprintf(lloop,      sizeof(lloop),      ".Lbrkx%d_loop",    id);
        snprintf(ldone,      sizeof(ldone),      ".Lbrkx%d_done",    id);
        snprintf(lfail_scan, sizeof(lfail_scan), ".Lbrkx%d_fscan",   id);
        snprintf(linner,     sizeof(linner),     ".Lbrkx%d_inner",   id);
        snprintf(lno_match,  sizeof(lno_match),  ".Lbrkx%d_nomatch", id);
        char jmp_succ[128]; snprintf(jmp_succ, sizeof(jmp_succ), "%s", s->name);
        char jmp_fail[128]; snprintf(jmp_fail, sizeof(jmp_fail), "%s", f->name);
        bb3c_format(out, "", "lea",   "rsi, [rip + Σ]");
        bb3c_format(out, "", "mov",   "rsi, qword ptr [rsi]");
        bb3c_format(out, "", "lea",   "rax, [rip + Σlen]");
        bb3c_format(out, "", "mov",   "edx, dword ptr [rax]");
        bb3c_format(out, "", "lea",   "rax, [rip + Δ]");
        bb3c_format(out, "", "mov",   "ecx, dword ptr [rax]");
        char r8_load[160]; snprintf(r8_load,  sizeof(r8_load),  "r8, [rip + %s]", zlbl);
        bb3c_format(out, "", "lea",   r8_load);
        bb3c_format(out, "", "mov",   "r8, qword ptr [r8]");
        bb3c_format(out, "", "xor",   "r9d, r9d");
        char loop_lbl[88]; snprintf(loop_lbl, sizeof(loop_lbl), "%s:", lloop);
        bb3c_format(out, loop_lbl, "mov", "r10d, ecx");
        bb3c_format(out, "", "add",   "r10d, r9d");
        bb3c_format(out, "", "cmp",   "r10d, edx");
        bb3c_format(out, "", "jge",   ldone);
        bb3c_format(out, "", "movzx", "r11d, byte ptr [rsi + r10]");
        char inner_lbl[88]; snprintf(inner_lbl, sizeof(inner_lbl), "%s:", linner);
        bb3c_format(out, inner_lbl, "movzx", "eax, byte ptr [r8]");
        bb3c_format(out, "", "test",  "al, al");
        bb3c_format(out, "", "je",    lno_match);
        bb3c_format(out, "", "cmp",   "al, r11b");
        bb3c_format(out, "", "je",    lfail_scan);
        bb3c_format(out, "", "inc",   "r8");
        bb3c_format(out, "", "jmp",   linner);
        char nomatch_lbl[88]; snprintf(nomatch_lbl, sizeof(nomatch_lbl), "%s:", lno_match);
        bb3c_format(out, nomatch_lbl, "inc", "r9d");
        bb3c_format(out, "", "lea",   r8_load);
        bb3c_format(out, "", "mov",   "r8, qword ptr [r8]");
        bb3c_format(out, "", "jmp",   lloop);
        char fscan_lbl[88]; snprintf(fscan_lbl, sizeof(fscan_lbl), "%s:", lfail_scan);
        bb3c_format(out, fscan_lbl, "nop", "");
        char done_lbl[88]; snprintf(done_lbl, sizeof(done_lbl), "%s:", ldone);
        bb3c_format(out, done_lbl, "test", "r9d, r9d");
        bb3c_format(out, "", "je",    jmp_fail);
        bb3c_format(out, "", "mov",   "r10d, ecx");
        bb3c_format(out, "", "add",   "r10d, r9d");
        bb3c_format(out, "", "cmp",   "r10d, edx");
        bb3c_format(out, "", "jge",   jmp_fail);
        char zdlbl[160]; snprintf(zdlbl, sizeof(zdlbl), "rax, [rip + %s + 8]", zlbl);
        bb3c_format(out, "", "lea",   zdlbl);
        bb3c_format(out, "", "mov",   "dword ptr [rax], r9d");
        bb3c_format(out, "", "lea",   "rax, [rip + Δ]");
        bb3c_format(out, "", "mov",   "dword ptr [rax], r10d");
        bb3c_format(out, "", "jmp",   jmp_succ);
        emit_label_define(b);
        bb3c_format(out, "", "lea",   zdlbl);
        bb3c_format(out, "", "mov",   "r9d, dword ptr [rax]");
        bb3c_format(out, "", "lea",   "rax, [rip + Δ]");
        bb3c_format(out, "", "mov",   "ecx, dword ptr [rax]");
        bb3c_format(out, "", "sub",   "ecx, r9d");
        bb3c_format(out, "", "mov",   "dword ptr [rax], ecx");
        bb3c_format(out, "", "jmp",   jmp_fail);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xcallcap(bb_box_fn child_fn, const char *fnc_name, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    void *z = bb_cap_new_call(child_fn, NULL, fnc_name, NULL, 0, NULL, 0, 0);
    emit_bb_box_banner("CALLCAP", fnc_name ? fnc_name : "");
    if (IS_TEXT) {
        char zlbl[80]; emit_bb_ptr_slot(zlbl);
        const char *clbl = child_fn ? child_cache_get_lbl(child_fn) : NULL;
        if (clbl && g_cap_fixup_cb) { char combo[320]; snprintf(combo, sizeof(combo), "%s|%s|%s|0|1", zlbl, clbl, fnc_name ? fnc_name : ""); g_cap_fixup_cb((void*)1, combo); }
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 0, s, f);
        emit_label_define(b);
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 1, s, f);
        return;
    }
    emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 0, s, f);
    emit_label_define(b);
    emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 1, s, f);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xfnce(bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    emit_bb_box_banner("FENCE",""); emit_jmp(s, JMP_JMP); emit_label_define(b); emit_jmp(f, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xfnme(bb_box_fn child_fn, const char *varname, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    void *z = bb_cap_new(child_fn, NULL, varname, NULL, 1);
    emit_bb_box_banner("CAP_IMM", varname ? varname : "");
    if (IS_TEXT) {
        char zlbl[80]; emit_bb_ptr_slot(zlbl);
        const char *clbl = child_fn ? child_cache_get_lbl(child_fn) : NULL;
        if (clbl && g_cap_fixup_cb) { char combo[320]; snprintf(combo, sizeof(combo), "%s|%s|%s|1|0", zlbl, clbl, varname ? varname : ""); g_cap_fixup_cb((void*)1, combo); }
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 0, s, f);
        emit_label_define(b);
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 1, s, f);
        return;
    }
    emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 0, s, f);
    emit_label_define(b);
    emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 1, s, f);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xnme(bb_box_fn child_fn, const char *varname, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    void *z = bb_cap_new(child_fn, NULL, varname, NULL, 0);
    emit_bb_box_banner("CAP_COND", varname ? varname : "");
    if (IS_TEXT) {
        char zlbl[80]; emit_bb_ptr_slot(zlbl);
        const char *clbl = child_fn ? child_cache_get_lbl(child_fn) : NULL;
        if (clbl && g_cap_fixup_cb) { char combo[320]; snprintf(combo, sizeof(combo), "%s|%s|%s|0|0", zlbl, clbl, varname ? varname : ""); g_cap_fixup_cb((void*)1, combo); }
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 0, s, f);
        emit_label_define(b);
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 1, s, f);
        return;
    }
    emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 0, s, f);
    emit_label_define(b);
    emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 1, s, f);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xposi(int n, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    char args[32]; snprintf(args, sizeof(args), "%d", n);
    emit_bb_box_banner("POS", args);
    emit_seq_cmp_delta_i(n, s, f);
    emit_label_define(b); emit_jmp(f, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xrpsi(int n, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    char args[32]; snprintf(args, sizeof(args), "%d", n);
    emit_bb_box_banner("RPOS", args);
    emit_seq_cmp_siglen_delta(n, (uint64_t)(uintptr_t)&Σlen, s, f);
    emit_label_define(b); emit_jmp(f, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xchr(const char *lit, const char *lit_label, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    if (!lit) lit = "";
    int len = (int)strlen(lit);
    char preview[40];
    if (len > 24) snprintf(preview, sizeof(preview), "'%.24s...'", lit);
    else          snprintf(preview, sizeof(preview), "'%s'", lit);
    emit_bb_box_banner("LIT", preview);
    emit_seq_bounds_len(len, TEMPLATE_ADDR_SIGLEN, f);
    emit_seq_sigma_delta_rdi(TEMPLATE_ADDR_SIGMA, TEMPLATE_ADDR_SIGLEN);
    emit_seq_lea_rsi_sym(lit_label, (uint64_t)(uintptr_t)lit);
    {   uint64_t val = (uint64_t)(uint32_t)len;
        switch (bb_emit_mode) {
        case EMIT_BINARY_WIRED: case EMIT_BINARY_BROKERED: insn_mov_rdx_i64(val); break;
        default: { char a[32]; snprintf(a,sizeof(a),"rdx, %d",len);
                   if (emit_bb_is_format_mode()) fmt_body_append("mov",a);
                   else bb3c_format(bb_emit_out?bb_emit_out:stdout,"","mov",a); break; }
        }
    }
    emit_push_r10();
    emit_call_sym_plt("memcmp", (uint64_t)(uintptr_t)memcmp);
    emit_pop_r10();
    emit_test_eax_eax();
    emit_jmp(f, JMP_JNE);
    emit_add_delta_imm(len);
    emit_jmp(s, JMP_JMP);
    emit_label_define(b);
    emit_sub_delta_imm(len);
    emit_jmp(f, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xdsar(const char *varname, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    char banner[80]; snprintf(banner, sizeof(banner), "*%s", varname ? varname : "");
    emit_bb_box_banner("DEREF", banner);
    int id = g_flat_node_id++;
    char zlbl[80], slbl[80];
    snprintf(zlbl, sizeof(zlbl), ".Ldvar%d_z",    id);
    snprintf(slbl, sizeof(slbl), ".Ldvar%d_name", id);
    const char *vn = varname ? varname : "";
    flat_data_section();
    flat3c_label(slbl); flat_data_string(vn);
    flat3c_label(zlbl);
    flat_data_quad(slbl); flat_data_quad("0"); flat_data_quad("0");
    flat_data_quad("0");  flat_data_long(0);   flat_data_long(0);
    flat_text_section(); flat_intel_syntax();
    void *z = bb_dvar_bin_new(vn);
    emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "bb_deferred_var_exported", (uint64_t)(uintptr_t)bb_deferred_var_exported, 0, s, f);
    emit_label_define(b);
    emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "bb_deferred_var_exported", (uint64_t)(uintptr_t)bb_deferred_var_exported, 1, s, f);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_xatp(const char *varname, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    emit_bb_box_banner("USERPAT", varname ? varname : "");
    int id = g_flat_node_id++;
    char zlbl[80], vlbl[80];
    snprintf(zlbl, sizeof(zlbl), ".Latp%d_z",     id);
    snprintf(vlbl, sizeof(vlbl), ".Latp%d_vname", id);
    const char *vn = varname ? varname : "";
    flat_data_section();
    flat3c_label(vlbl); flat_data_string(vn);
    flat3c_label(zlbl); flat_data_long(0); flat_data_long(0); flat_data_quad(vlbl);
    flat_text_section(); flat_intel_syntax();
    atp_t *z = bb_atp_new(vn);
    emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_atp", (uint64_t)(uintptr_t)rt_bb_atp, 0, s, f);
    emit_label_define(b);
    emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_atp", (uint64_t)(uintptr_t)rt_bb_atp, 1, s, f);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_charset(bb_box_fn c_fn, const char *c_fn_name, const char *kind_name, const char *chars, bb_label_t *s, bb_label_t *f, bb_label_t *b) {
    (void)c_fn;
    typedef struct { const char *chars; int delta; } cs_t;
    cs_t *z = calloc(1, sizeof(cs_t)); z->chars = chars;
    const char *rt_name; uint64_t rt_fn;
    if      (c_fn_name && strcmp(c_fn_name,"bb_span")   == 0) { rt_name="rt_bb_span";   rt_fn=(uint64_t)(uintptr_t)rt_bb_span;   }
    else  if(c_fn_name && strcmp(c_fn_name,"bb_brk")    == 0)  { rt_name="rt_bb_brk";    rt_fn=(uint64_t)(uintptr_t)rt_bb_brk;    }
    else  if(c_fn_name && strcmp(c_fn_name,"bb_any")    == 0)  { rt_name="rt_bb_any";    rt_fn=(uint64_t)(uintptr_t)rt_bb_any;    }
    else  if(c_fn_name && strcmp(c_fn_name,"bb_notany") == 0)  { rt_name="rt_bb_notany"; rt_fn=(uint64_t)(uintptr_t)rt_bb_notany; }
    else                                                       { rt_name="rt_bb_span";   rt_fn=(uint64_t)(uintptr_t)rt_bb_span;   }
    if (IS_TEXT) {
        emit_bb_box_banner(kind_name ? kind_name : "CHARSET", chars ? chars : "");
        int id = g_flat_node_id++;
        char slbl[80], zlbl[80];
        snprintf(slbl, sizeof(slbl), ".Lcs%d_chars", id);
        snprintf(zlbl, sizeof(zlbl), ".Lcs%d_z",     id);
        const char *ch = chars ? chars : "";
        char esc[1024]; size_t o = 0;
        if (o < sizeof(esc)) esc[o++] = '"';
        for (const char *cp = ch; *cp && o + 5 < sizeof(esc); cp++) {
            unsigned char c = (unsigned char)*cp;
            if (c == '\"' || c == '\\') { esc[o++] = '\\'; esc[o++] = (char)c; }
            else if (c >= 32 && c < 127) { esc[o++] = (char)c; }
            else { o += snprintf(esc + o, sizeof(esc) - o, "\\%03o", c); }
        }
        if (o + 1 < sizeof(esc)) esc[o++] = '"';
        esc[o] = '\0';
        FILE *out = emit_outf();
        char slbl_def[88], zlbl_def[88];
        snprintf(slbl_def, sizeof(slbl_def), "%s:", slbl);
        snprintf(zlbl_def, sizeof(zlbl_def), "%s:", zlbl);
        bb3c_format(out, "",       ".section", ".data");
        bb3c_format(out, slbl_def, ".string",  esc);
        bb3c_format(out, zlbl_def, ".quad",    slbl);
        bb3c_format(out, "",       ".long",    "0");
        bb3c_format(out, "",       ".long",    "0");
        bb3c_format(out, "",       ".section", ".text");
        bb3c_format(out, "",       ".intel_syntax", "noprefix");
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, rt_name, rt_fn, 0, s, f);
        emit_label_define(b);
        emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, rt_name, rt_fn, 1, s, f);
    } else {
        emit_seq_port_call((uint64_t)(uintptr_t)z, rt_name, rt_fn, 0, s, f);
        emit_label_define(b);
        emit_seq_port_call((uint64_t)(uintptr_t)z, rt_name, rt_fn, 1, s, f);
    }
}
/* PST-RB-5i: raised from 16K to 256K. The 16K cap was sized for SNOBOL4
   beauty.sno patterns; the SCRIP-hosted parsers (parser_snocone.sc etc.)
   compose much larger compound patterns — a 931-line grammar lowered to
   a single `Compiland` pattern overflows 16K immediately. 256K leaves
   headroom for parser_*.sc patterns and is still small relative to total
   process memory. */
#define FLAT_BUF_MAX  (256 * 1024)
int g_flat_node_id   = 0;
static int g_flat_slot_count = 0;
#define FLAT_DATA_BUF_MAX     (32 * 1024)
#define FLAT_DATA_LBL_MAX     32
static char   g_flat_data_buf[FLAT_DATA_BUF_MAX];
static size_t g_flat_data_len    = 0;
static int    g_flat_data_active = 0;
#define CHILD_CACHE_MAX 64
static struct { BB_t *key; bb_box_fn fn; char text_lbl[80]; } g_child_cache[CHILD_CACHE_MAX];
static int g_child_cache_n = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static bb_box_fn child_cache_get(BB_t *p) {
    for (int i = 0; i < g_child_cache_n; i++) if (g_child_cache[i].key == p) return g_child_cache[i].fn;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *child_cache_get_lbl(bb_box_fn fn) {
    for (int i = 0; i < g_child_cache_n; i++) if (g_child_cache[i].fn == fn && g_child_cache[i].text_lbl[0]) return g_child_cache[i].text_lbl;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void child_cache_put(BB_t *p, bb_box_fn fn) {
    if (g_child_cache_n < CHILD_CACHE_MAX) { g_child_cache[g_child_cache_n].key = p; g_child_cache[g_child_cache_n].fn = fn; g_child_cache[g_child_cache_n].text_lbl[0] = '\0'; g_child_cache_n++; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void child_cache_set_lbl(bb_box_fn fn, const char *lbl) {
    for (int i = 0; i < g_child_cache_n; i++) if (g_child_cache[i].fn == fn) { snprintf(g_child_cache[i].text_lbl, 80, "%s", lbl ? lbl : ""); return; }
}
static int    g_flat_data_any    = 0;
static int    g_flat_data_just_closed = 0;
static char   g_flat_data_pending_lbl[160] = "";
static char   g_flat_data_block_lbls[FLAT_DATA_LBL_MAX][96];
static int    g_flat_data_block_nlbls = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void data_buf_reset(void) {
    g_flat_data_len = 0;
    g_flat_data_active = 0;
    g_flat_data_any = 0;
    g_flat_data_just_closed = 0;
    g_flat_data_block_nlbls = 0;
    g_flat_data_pending_lbl[0] = '\0';
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void data_buf_appendf(const char *fmt, ...) {
    if (g_flat_data_len >= FLAT_DATA_BUF_MAX) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(g_flat_data_buf + g_flat_data_len, FLAT_DATA_BUF_MAX - g_flat_data_len, fmt, ap);
    va_end(ap);
    if (n > 0) {
        size_t left = FLAT_DATA_BUF_MAX - g_flat_data_len;
        g_flat_data_len += ((size_t)n < left) ? (size_t)n : left;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void data_buf_three_col(const char *lbl, const char *act, const char *got) {
    char fused_lbl[160];
    const char *eff_lbl = lbl ? lbl : "";
    if ((eff_lbl[0] == '\0') && g_flat_data_pending_lbl[0]) {
        snprintf(fused_lbl, sizeof(fused_lbl), "%s", g_flat_data_pending_lbl);
        g_flat_data_pending_lbl[0] = '\0';
        eff_lbl = fused_lbl;
    } else if (eff_lbl[0] != '\0' && g_flat_data_pending_lbl[0]) {
        char line[256];
        int n = snprintf(line, sizeof(line), "%-24s", g_flat_data_pending_lbl);
        if (n > 0) {
            while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) line[--n] = '\0';
            data_buf_appendf("%s\n", line);
        }
        g_flat_data_pending_lbl[0] = '\0';
    }
    char line[512];
    int n = snprintf(line, sizeof(line), "%-24s%-16s %s", eff_lbl, act ? act : "", got ? got : "");
    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
    while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) line[--n] = '\0';
    data_buf_appendf("%s\n", line);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void data_buf_pend_label(const char *name) {
    if (g_flat_data_pending_lbl[0]) {
        char line[256];
        int n = snprintf(line, sizeof(line), "%-24s", g_flat_data_pending_lbl);
        if (n > 0) {
            while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) line[--n] = '\0';
            data_buf_appendf("%s\n", line);
        }
    }
    snprintf(g_flat_data_pending_lbl, sizeof(g_flat_data_pending_lbl), "%s:", name ? name : "");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void data_buf_flush_pending_label(void) {
    if (!g_flat_data_pending_lbl[0]) return;
    char line[256];
    int n = snprintf(line, sizeof(line), "%-24s", g_flat_data_pending_lbl);
    if (n > 0) {
        while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) line[--n] = '\0';
        data_buf_appendf("%s\n", line);
    }
    g_flat_data_pending_lbl[0] = '\0';
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_flat_set_cap_fixup(void (*cb)(void *cap_ptr, const char *child_α_label)) { g_cap_fixup_cb = cb; }
#define SYM_SIGMA   "\xCE\xA3"
#define SYM_SIGLEN  "\xCE\xA3""len"
#define SYM_DELTA   "\xCE\x94"
#define ADDR_SIGMA   ((uint64_t)(uintptr_t)&Σ)
#define ADDR_SIGLEN  ((uint64_t)(uintptr_t)&Σlen)
#define ADDR_DELTA   ((uint64_t)(uintptr_t)&Δ)
static const char *(*g_flat_intern_str)(const char *s) = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_flat_set_intern_str(const char *(*fn)(const char *)) { g_flat_intern_str = fn; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *emit_flat_intern_str(const char *s) {
    return (g_flat_intern_str && g_is_text) ? g_flat_intern_str(s) : NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void flat3c(const char *lbl, const char *act, const char *got) {
    if (!g_is_text) return;
    FILE *f = bb_emit_out;
    if (!f) return;
    bb3c_format(f, lbl ? lbl : "", act ? act : "", got ? got : "");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void flat3c_action(const char *act, const char *args) { flat3c("", act, args ? args : ""); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void data_buf_remember_label(const char *name);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_flat_label(const char *name) {
    if (!g_is_text) return;
    if (g_flat_data_active) { data_buf_pend_label(name); data_buf_remember_label(name); return; }
    char buf[160]; snprintf(buf, sizeof(buf), "%s:", name);
    flat3c(buf, "", "");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void data_buf_remember_label(const char *name) {
    if (g_flat_data_block_nlbls >= FLAT_DATA_LBL_MAX) return;
    snprintf(g_flat_data_block_lbls[g_flat_data_block_nlbls], sizeof(g_flat_data_block_lbls[0]), "%s", name ? name : "");
    g_flat_data_block_nlbls++;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void data_buf_emit_block_comment(void) { g_flat_data_block_nlbls = 0; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_flat_data_section(void)  { if (!g_is_text) return; g_flat_data_active = 1; g_flat_data_any = 1; g_flat_data_block_nlbls = 0; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_flat_text_section(void) {
    if (!g_is_text) return;
    if (g_flat_data_active) { data_buf_emit_block_comment(); g_flat_data_active = 0; g_flat_data_just_closed = 1; return; }
    flat3c("", ".section", ".text");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_flat_intel_syntax(void) {
    if (!g_is_text) return;
    if (g_flat_data_active) return;
    if (g_flat_data_just_closed) { g_flat_data_just_closed = 0; return; }
    flat3c("", ".intel_syntax", "noprefix");
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_flat_data_string(const char *s) {
    if (!g_is_text) return;
    char esc[1024]; size_t o = 0;
    if (o < sizeof(esc)) esc[o++] = '"';
    for (const char *cp = s ? s : ""; *cp && o + 5 < sizeof(esc); cp++) {
        unsigned char c = (unsigned char)*cp;
        if (c == '\"' || c == '\\') { esc[o++] = '\\'; esc[o++] = (char)c; }
        else if (c >= 32 && c < 127) { esc[o++] = (char)c; }
        else { o += snprintf(esc + o, sizeof(esc) - o, "\\%03o", c); }
    }
    if (o + 1 < sizeof(esc)) esc[o++] = '"';
    esc[o] = '\0';
    if (g_flat_data_active) data_buf_three_col("", ".string", esc);
    else                    flat3c("", ".string", esc);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_flat_data_quad(const char *arg) {
    if (!g_is_text) return;
    if (g_flat_data_active) data_buf_three_col("", ".quad", arg ? arg : "0");
    else                    flat3c("", ".quad", arg ? arg : "0");
}
void emit_flat_data_long(long long v) {
    if (!g_is_text) return;
    char buf[32]; snprintf(buf, sizeof(buf), "%lld", v);
    if (g_flat_data_active) data_buf_three_col("", ".long", buf);
    else                    flat3c("", ".long", buf);
}
void emit_flat_entry_dispatch(bb_label_t *lbl_alpha_body, bb_label_t *lbl_beta) {
    if (!g_is_text) return;
    char buf[512];
    int o = 0;
    o += snprintf(buf + o, sizeof(buf) - o, "esi, 0;");
    while (o < 27 && o < (int)sizeof(buf) - 1) buf[o++] = ' ';
    buf[o] = '\0';
    snprintf(buf + o, sizeof(buf) - o, "je %s; jmp %s", lbl_alpha_body ? lbl_alpha_body->name : "?", lbl_beta ? lbl_beta->name : "?");
    flat3c_action("cmp", buf);
}
#define BB_BANNER_RULE_LEN 119
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_flat_banner_rule(char ch) {
    if (!g_is_text) return;
    char buf[BB_BANNER_RULE_LEN + 4];
    buf[0] = '#';
    for (int i = 0; i < BB_BANNER_RULE_LEN; i++) buf[1 + i] = ch;
    buf[1 + BB_BANNER_RULE_LEN] = '\0';
    emit_text_rawf("%s\n", buf);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_flat_ir_banner(void) { if (!g_is_text) return; emit_flat_banner_rule('='); }
static void emit_flat_ir(BB_t *nd, bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_flat_ir_fence(BB_t *nd, bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β) {
    if (!nd || nd->n == 0) { emit_bb_xfnce(lbl_succ, lbl_fail, lbl_β); return; }
    int id = g_flat_node_id++;
    bb_label_t child_γ, child_ω;
    emit_label_initf(&child_γ, "xfnce%d_γ", id);
    emit_label_initf(&child_ω, "xfnce%d_ω", id);
    emit_flat_ir(nd->c[0], &child_γ, &child_ω, lbl_β);
    emit_label_define_bb(&child_γ);
    emit_jmp_label(lbl_succ, JMP_JMP);
    emit_label_define_bb(&child_ω);
    emit_jmp_label(lbl_fail, JMP_JMP);
    emit_label_define_bb(lbl_β);
    emit_jmp_label(lbl_fail, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_flat_ir_cat(BB_t *nd, bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β) {
    int id = g_flat_node_id++;
    bb_label_t mid_γ, right_ω, left_β, right_β, xcat_ω;
    emit_label_initf(&mid_γ,   "xcat%d_γ",         id);
    emit_label_initf(&right_ω, "xcat%d_right_ω",   id);
    emit_label_initf(&left_β,  "xcat%d_left_β",    id);
    emit_label_initf(&right_β, "xcat%d_right_β",   id);
    emit_label_initf(&xcat_ω,  "xcat%d_ω",         id);
    if (!nd || nd->n == 0) {
        emit_jmp_label(lbl_succ, JMP_JMP);
        emit_label_define_bb(lbl_β); emit_jmp_label(lbl_fail, JMP_JMP);
        emit_label_define_bb(&xcat_ω); emit_jmp_label(lbl_fail, JMP_JMP);
        emit_label_define_bb(&mid_γ); emit_label_define_bb(&right_ω);
        emit_label_define_bb(&right_β); emit_label_define_bb(&left_β);
        return;
    }
    if (nd->n == 1) {
        emit_flat_ir(nd->c[0], lbl_succ, lbl_fail, &left_β);
        emit_label_define_bb(lbl_β); emit_jmp_label(&left_β, JMP_JMP);
        emit_label_define_bb(&xcat_ω); emit_jmp_label(lbl_fail, JMP_JMP);
        emit_label_define_bb(&mid_γ); emit_label_define_bb(&right_ω); emit_label_define_bb(&right_β);
        return;
    }
    emit_flat_ir(nd->c[0], &mid_γ, &xcat_ω, &left_β);
    emit_label_define_bb(&mid_γ);
    bb_label_t *last_β = &right_β;
    if (nd->n == 2) {
        emit_flat_ir(nd->c[1], lbl_succ, &right_ω, &right_β);
    } else {
        int nc = nd->n;
        bb_label_t *mids  = alloca(sizeof(bb_label_t) * (nc - 1));
        bb_label_t *betas = alloca(sizeof(bb_label_t) * (nc - 1));
        for (int i = 0; i < nc - 1; i++) {
            emit_label_initf(&mids[i],  "xcat%d_mid%d_γ", id, i+1);
            emit_label_initf(&betas[i], "xcat%d_mid%d_β", id, i+1);
        }
        for (int i = 1; i < nc; i++) {
            bb_label_t *s = (i < nc-1) ? &mids[i-1] : lbl_succ;
            emit_flat_ir(nd->c[i], s, &right_ω, &betas[i-1]);
            if (i < nc-1) emit_label_define_bb(&mids[i-1]);
        }
        last_β = &betas[nc-2];
    }
    emit_label_define_bb(&right_ω); emit_jmp_label(&left_β, JMP_JMP);
    emit_label_define_bb(lbl_β); emit_jmp_label(last_β, JMP_JMP);
    emit_label_define_bb(&xcat_ω);  emit_jmp_label(lbl_fail, JMP_JMP);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void emit_flat_ir_alt(BB_t *nd, bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β) {
    int id = g_flat_node_id++;
    int nc = nd ? nd->n : 0;
    if (nc == 0) { emit_label_define_bb(lbl_β); emit_jmp_label(lbl_fail, JMP_JMP); return; }
    if (nc == 1) { emit_flat_ir(nd->c[0], lbl_succ, lbl_fail, lbl_β); return; }
    bb_label_t *ci_βs = alloca((size_t)nc * sizeof(bb_label_t));
    bb_label_t *ci_ωs = alloca((size_t)nc * sizeof(bb_label_t));
    for (int i = 0; i < nc; i++) {
        emit_label_initf(&ci_βs[i], "alt%d_c%d_β", id, i);
        emit_label_initf(&ci_ωs[i], "alt%d_c%d_ω", id, i);
    }
    for (int i = 0; i < nc; i++) {
        bb_label_t *f = (i < nc-1) ? &ci_ωs[i] : &ci_ωs[nc-1];
        emit_flat_ir(nd->c[i], lbl_succ, f, &ci_βs[i]);
        if (i < nc-1) emit_label_define_bb(&ci_ωs[i]);
        else          emit_label_define_bb(&ci_ωs[nc-1]);
    }
    emit_jmp_label(lbl_fail, JMP_JMP);
    emit_label_define_bb(lbl_β); emit_jmp_label(&ci_βs[0], JMP_JMP);
}
extern int memcmp(const void *, const void *, size_t);
static void emit_flat_ir(BB_t *nd, bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β) {
    if (!nd) { emit_bb_xeps(lbl_succ, lbl_fail, lbl_β); return; }
    switch (nd->t) {
    case BB_PAT_LIT: {
        const char *lit = nd->sval ? nd->sval : "";
        const char *lit_label = (g_flat_intern_str && g_is_text) ? g_flat_intern_str(lit) : NULL;
        emit_bb_xchr(lit, lit_label, lbl_succ, lbl_fail, lbl_β);
        break;
    }
    case BB_PAT_ARB:    emit_bb_xfarb(lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_REM:    emit_bb_xstar(lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_SPAN:   emit_bb_charset(NULL, "bb_span",   "SPAN",   nd->sval?nd->sval:"", lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_ANY:    emit_bb_charset(NULL, "bb_any",    "ANY",    nd->sval?nd->sval:"", lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_BREAK:  emit_bb_charset(NULL, "bb_brk",    "BREAK",  nd->sval?nd->sval:"", lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_NOTANY: emit_bb_charset(NULL, "bb_notany", "NOTANY", nd->sval?nd->sval:"", lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_LEN:    emit_bb_xlnth((long long)nd->ival, lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_POS:    if (nd->n) emit_bb_xrpsi((int)nd->ival, lbl_succ, lbl_fail, lbl_β);
                        else       emit_bb_xposi ((int)nd->ival, lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_TAB:    if (nd->n) emit_bb_xrtb ((long long)nd->ival, lbl_succ, lbl_fail, lbl_β);
                        else       emit_bb_xtb   ((long long)nd->ival, lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_FENCE:  emit_flat_ir_fence(nd, lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_ABORT:  emit_bb_xfail(lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_CAT:    emit_flat_ir_cat(nd, lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_ALT:    emit_flat_ir_alt(nd, lbl_succ, lbl_fail, lbl_β); break;
    case BB_PAT_ARBNO: {
        BB_t *ch = (nd->n > 0) ? nd->c[0] : NULL;
        bb_box_fn cfn = ch ? child_cache_get(ch) : NULL;
        emit_bb_xarbn(cfn, lbl_succ, lbl_fail, lbl_β);
        break;
    }
    case BB_PAT_ASSIGN_IMM: {
        BB_t *ch = (nd->n > 0) ? nd->c[0] : NULL;
        bb_box_fn cfn = ch ? child_cache_get(ch) : NULL;
        const char *vn = nd->sval ? nd->sval : "";
        if (nd->value.v == DT_N && nd->value.slen == 1 && nd->value.ptr) {
            void *z = bb_cap_new(cfn, NULL, NULL, (DESCR_t *)nd->value.ptr, 1);
            emit_bb_box_banner("CAP_IMM", "");
            if (IS_TEXT) {
                char zlbl[80]; emit_bb_ptr_slot(zlbl);
                const char *clbl = cfn ? child_cache_get_lbl(cfn) : NULL;
                if (clbl && g_cap_fixup_cb) { char combo[320]; snprintf(combo, sizeof(combo), "%s|%s||1|0", zlbl, clbl); g_cap_fixup_cb((void*)1, combo); }
                emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 0, lbl_succ, lbl_fail);
                emit_label_define(lbl_β);
                emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 1, lbl_succ, lbl_fail);
            } else {
                emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 0, lbl_succ, lbl_fail);
                emit_label_define_bb(lbl_β);
                emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 1, lbl_succ, lbl_fail);
            }
        } else {
            emit_bb_xfnme(cfn, vn, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    case BB_PAT_ASSIGN_COND: {
        BB_t *ch = (nd->n > 0) ? nd->c[0] : NULL;
        bb_box_fn cfn = ch ? child_cache_get(ch) : NULL;
        const char *vn = nd->sval ? nd->sval : "";
        if (nd->value.v == DT_N && nd->value.slen == 1 && nd->value.ptr) {
            void *z = bb_cap_new(cfn, NULL, NULL, (DESCR_t *)nd->value.ptr, 0);
            emit_bb_box_banner("CAP_COND", "");
            if (IS_TEXT) {
                char zlbl[80]; emit_bb_ptr_slot(zlbl);
                const char *clbl = cfn ? child_cache_get_lbl(cfn) : NULL;
                if (clbl && g_cap_fixup_cb) { char combo[320]; snprintf(combo, sizeof(combo), "%s|%s||0|0", zlbl, clbl); g_cap_fixup_cb((void*)1, combo); }
                emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 0, lbl_succ, lbl_fail);
                emit_label_define(lbl_β);
                emit_seq_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 1, lbl_succ, lbl_fail);
            } else {
                emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 0, lbl_succ, lbl_fail);
                emit_label_define_bb(lbl_β);
                emit_seq_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, 1, lbl_succ, lbl_fail);
            }
        } else {
            emit_bb_xnme(cfn, vn, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    default:
        emit_label_define_bb(lbl_β);
        emit_jmp_label(lbl_fail, JMP_JMP);
        emit_jmp_label(lbl_fail, JMP_JMP);
        break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int emit_flat_body(BB_t *nd, const char *prefix, int text_externalise, int brokered) {
    bb_label_t lbl_α, lbl_α_body, lbl_succ, lbl_fail, lbl_β;
    emit_label_initf(&lbl_α,      "%s_α",      prefix);
    emit_label_initf(&lbl_α_body, "%s_α_body", prefix);
    emit_label_initf(&lbl_succ,       "%s_γ",      prefix);
    emit_label_initf(&lbl_fail,       "%s_ω",      prefix);
    emit_label_initf(&lbl_β,       "%s_β",       prefix);
    if (text_externalise) data_buf_reset();
    if (text_externalise) {
        emit_flat_ir_banner();
        emit_text_global(lbl_α.name);
        emit_text_global(lbl_β.name);
        emit_text_global(lbl_succ.name);
        emit_text_global(lbl_fail.name);
        emit_label_define_bb(&lbl_α);
    }
    {   emit_sym_lea_r10("Δ", ADDR_DELTA); }
    flat_box_entry_dispatch(&lbl_α_body, &lbl_β);
    emit_label_define_bb(&lbl_α_body);
    if (!text_externalise) emit_label_define_bb(&lbl_α);
    emit_flat_ir(nd, &lbl_succ, &lbl_fail, &lbl_β);
    emit_label_define_bb(&lbl_succ);
    emit_sigma_plus_delta(ADDR_SIGMA);
    emit_mov_rdx_rax();
    emit_mov_eax_imm32(1);
    if (brokered) emit_pop_rbp();
    emit_ret();
    emit_label_define_bb(&lbl_fail);
    emit_mov_eax_imm32(99);
    emit_xor_edx_edx();
    if (brokered) emit_pop_rbp();
    emit_ret();
    if (text_externalise && g_is_text && g_flat_data_any) {
        data_buf_flush_pending_label();
        bb3c_flush_pending();
        flat3c("", ".section", ".data");
        emit_text_rawf("%.*s", (int)g_flat_data_len, g_flat_data_buf);
        flat3c("", ".section", ".text");
        data_buf_reset();
    }
    return 0;
}
static int g_in_prebuild = 0;
static int g_text_child_counter = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void pre_build_children_text(BB_t *nd, FILE *out, const char *base_prefix) {
    if (!nd) return;
    if (nd->t == BB_PAT_ARBNO || nd->t == BB_PAT_ASSIGN_COND || nd->t == BB_PAT_ASSIGN_IMM || nd->t == BB_PAT_CALLOUT) {
        BB_t *ch = (nd->n > 0) ? nd->c[0] : NULL;
        if (ch && !child_cache_get(ch)) {
            pre_build_children_text(ch, out, base_prefix);
            char child_prefix[120];
            snprintf(child_prefix, sizeof(child_prefix), "%s_c%d", base_prefix, g_text_child_counter++);
            emitter_init_text(out, TEXT_MODE_INVOCATION);
            emit_flat_body(ch, child_prefix, 1, 0);
            emitter_end();
            bb3c_flush_pending();
            char α_lbl[128];
            snprintf(α_lbl, sizeof(α_lbl), "%s_α", child_prefix);
            bb_box_fn sentinel = (bb_box_fn)(uintptr_t)ch;
            child_cache_put(ch, sentinel);
            child_cache_set_lbl(sentinel, α_lbl);
        }
        return;
    }
    for (int i = 0; i < nd->n; i++) pre_build_children_text(nd->c[i], out, base_prefix);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void pre_build_children(BB_t *nd) {
    if (!nd) return;
    if (nd->t == BB_PAT_ARBNO || nd->t == BB_PAT_ASSIGN_COND || nd->t == BB_PAT_ASSIGN_IMM || nd->t == BB_PAT_CALLOUT) {
        BB_t *ch = (nd->n > 0) ? nd->c[0] : NULL;
        if (ch && !child_cache_get(ch)) {
            pre_build_children(ch);
            bb_box_fn fn = (nd->t == BB_PAT_ARBNO || nd->t == BB_PAT_ASSIGN_COND || nd->t == BB_PAT_ASSIGN_IMM) ? bb_build_brokered(ch) : bb_build_flat(ch);
            if (!fn) fn = bb_build_brokered(ch);
            child_cache_put(ch, fn);
        }
        return;
    }
    for (int i = 0; i < nd->n; i++) pre_build_children(nd->c[i]);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_box_fn bb_build_flat(BB_t *nd) {
    bb_buf_t buf = bb_alloc(FLAT_BUF_MAX);
    if (!buf) return NULL;
    if (!g_in_prebuild) { g_child_cache_n = 0; g_in_prebuild = 1; pre_build_children(nd); g_in_prebuild = 0; }
    g_flat_slot_count = 0; g_flat_node_id = 0;
    emitter_init_binary(buf, FLAT_BUF_MAX);
    emit_flat_body(nd, "pat_flat", 0, 0);
    int nbytes = emitter_end();
    extern int bb_emit_overflow;
    if (bb_emit_overflow || nbytes <= 0 || nbytes > FLAT_BUF_MAX) { bb_free(buf, FLAT_BUF_MAX); return NULL; }
    bb_seal(buf, (size_t)nbytes);
    return (bb_box_fn)buf;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
bb_box_fn bb_build_brokered(BB_t *nd) {
    bb_buf_t buf = bb_alloc(FLAT_BUF_MAX);
    if (!buf) return NULL;
    if (!g_in_prebuild) { g_child_cache_n = 0; g_in_prebuild = 1; pre_build_children(nd); g_in_prebuild = 0; }
    g_flat_slot_count = 0; g_flat_node_id = 0;
    emit_mode_set(EMIT_BINARY_BROKERED, NULL);
    emitter_init_binary(buf, FLAT_BUF_MAX);
    emit_seq_brokered_enter();
    emit_flat_body(nd, "pat_brok", 0, 1);
    int nbytes = emitter_end();
    emit_mode_set(EMIT_BINARY_WIRED, NULL);
    extern int bb_emit_overflow;
    if (bb_emit_overflow || nbytes <= 0 || nbytes > FLAT_BUF_MAX) { bb_free(buf, FLAT_BUF_MAX); return NULL; }
    bb_seal(buf, (size_t)nbytes);
    return (bb_box_fn)buf;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int emit_flat_build(BB_t *nd, FILE *out, const char *prefix) {
    g_child_cache_n = 0;
    g_text_child_counter = 0;
    pre_build_children_text(nd, out, prefix);
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    int rc = emit_flat_body(nd, prefix, 1, 0);
    emitter_end();
    bb3c_flush_pending();
    return rc;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_bb_register_child_label(BB_t *nd, const char *α_label) {
    bb_box_fn fn = child_cache_get(nd);
    if (fn) child_cache_set_lbl(fn, α_label);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void emit_flat_reset(void) { g_flat_slot_count = 0; g_flat_node_id = 0; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void bm_line(FILE *f, const char *lbl, const char *act, const char *got) {
    char line[512];
    int n = snprintf(line, sizeof(line), "%-24s%-16s %s", lbl ? lbl : "", act ? act : "", got ? got : "");
    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
    while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) n--;
    line[n] = '\0';
    fprintf(f, "%s\n", line);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void bm_macro(FILE *f, const char *name, const char *args) {
    char decl[160];
    if (args && args[0]) snprintf(decl, sizeof(decl), "%s %s", name, args);
    else                 snprintf(decl, sizeof(decl), "%s", name);
    bm_line(f, "", ".macro", decl);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void bm_endm(FILE *f)                        { bm_line(f, "", ".endm", ""); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void bm_op  (FILE *f, const char *mn, const char *args) { bm_line(f, "", mn, args ? args : ""); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void bm_jmp (FILE *f, const char *cond, const char *tgt) { char arg[128]; snprintf(arg, sizeof(arg), "\\%s", tgt); bm_line(f, "", cond, arg); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int emit_bb_macro_library_to_path(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# === BEGIN bb macro library (EM-7c-bb-macros) ===\n");
    fprintf(f, "                        .intel_syntax    noprefix\n");
    fprintf(f, "# One named .macro/.endm per leaf-box port.  Three-column shape.\n");
    fprintf(f, "# GAS expands to byte-identical inline x86 (same as emit_* emissions).\n");
    bm_macro(f, "DELTA_LOAD", "");
    bm_op   (f, "mov", "eax, [r10]");
    bm_endm (f);
    bm_macro(f, "SIGLEN_LOAD", "");
    bm_op   (f, "lea", "rcx, [rip + Σlen]");
    bm_op   (f, "mov", "eax, [rcx]");
    bm_endm (f);
    bm_macro(f, "EPS_α", "lbl_succ");
    bm_jmp  (f, "jmp", "lbl_succ");
    bm_endm (f);
    bm_macro(f, "EPS_β", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    bm_macro(f, "FAIL_α", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    bm_macro(f, "FAIL_β", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    bm_macro(f, "RPOS_α", "n, lbl_succ, lbl_fail");
    bm_op   (f, "SIGLEN_LOAD", "");
    bm_op   (f, "sub", "eax, \\n");
    bm_op   (f, "mov", "ecx, eax");
    bm_op   (f, "DELTA_LOAD", "");
    bm_op   (f, "cmp", "eax, ecx");
    bm_jmp  (f, "jne", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_succ");
    bm_endm (f);
    bm_macro(f, "RPOS_β", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    bm_macro(f, "POS_α", "n, lbl_succ, lbl_fail");
    bm_op   (f, "DELTA_LOAD", "");
    bm_op   (f, "cmp", "eax, \\n");
    bm_jmp  (f, "jne", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_succ");
    bm_endm (f);
    bm_macro(f, "POS_β", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    fprintf(f, "# === END bb macro library ===\n");
    return fclose(f) == 0 ? 0 : -1;
}
