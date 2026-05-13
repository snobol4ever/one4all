#include "emitter.h"
#include "bb_emit.h"
#include "bb_box.h"
#include "templates.h"
#include "../frontend/icon/icon_gen.h"
#include "snobol4_patnd.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "bb_flat.h"
#include "../rt/rt.h"

/*---- all extern declarations in one block -------------------------------------------------------------------------*/
extern DESCR_t  bb_deferred_var_exported(void *zeta, int entry);  /* stmt_exec.c — BINARY fn_fallback for XDSAR */
extern DESCR_t  coro_bb_alternate    (void *zeta, int entry);   extern icn_alternate_state_t      *icon_alt_new(void);
extern DESCR_t  coro_bb_bang_binary  (void *zeta, int entry);   extern icn_bang_binary_state_t     *icon_bang_new(void);
extern DESCR_t  coro_bb_every        (void *zeta, int entry);   extern icn_every_state_t           *icon_every_new(void);
extern DESCR_t  coro_bb_iterate      (void *zeta, int entry);   extern icn_iterate_state_t         *icon_iterate_new(void);
extern DESCR_t  coro_bb_list_iterate (void *zeta, int entry);   extern icn_list_iterate_state_t    *icon_list_iterate_new(void);
extern DESCR_t  coro_bb_tbl_iterate  (void *zeta, int entry);   extern icn_tbl_iterate_state_t     *icon_tbl_iterate_new(void);
extern DESCR_t  coro_bb_record_iterate(void *zeta, int entry);  extern icn_record_iterate_state_t  *icon_record_iterate_new(void);
extern DESCR_t  coro_bb_cat          (void *zeta, int entry);   extern icn_cat_gen_state_t         *icon_lconcat_new(void);
extern DESCR_t  coro_bb_limit        (void *zeta, int entry);   extern icn_limit_state_t           *icon_limit_new(void);
extern DESCR_t  coro_bb_seq_expr     (void *zeta, int entry);   extern icn_seq_state_t             *icon_seq_new(void);
extern DESCR_t  coro_bb_to           (void *zeta, int entry);   extern icn_to_state_t              *icon_to_new(void);
extern DESCR_t  coro_bb_to_by        (void *zeta, int entry);   extern icn_to_by_state_t           *icon_to_by_new(void);
extern atp_t   *bb_atp_new(const char *varname);
extern bal_t   *bb_bal_new(void);
extern brkx_t  *bb_breakx_new(const char *chars);
extern cap_t   *bb_cap_new_call(bb_box_fn child_fn, void *child_state, const char *fnc_name,
                                 DESCR_t *fnc_args, int fnc_nargs, char **fnc_arg_names, int fnc_n_arg_names, int immediate);
extern cap_t   *bb_cap_new(bb_box_fn child_fn, void *child_state, const char *varname, DESCR_t *var_ptr, int immediate);
extern const char *Σ;
extern int         Σlen;
extern void       *bb_dvar_bin_new(const char *name);
extern arb_t      *bb_arb_new(void);
extern len_t      *bb_len_new(int n);
extern tab_t      *bb_tab_new(int n);
extern rtab_t     *bb_rtab_new(int n);
extern rem_t      *bb_rem_new(void);
/*------------------------------------------------------------------------------------------------------------------*/

/* EC-4: Both α and β jump to lbl_fail with no state.
 * beta_first=1 (CAT/OR/VAR): β label precedes the α jmp.
 * beta_first=0 (FAIL/ABORT):  α jmp precedes the β label. */
static void emit_bb_jmp_pair(const char *banner, bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_beta,
                              int beta_first)
{
    (void)lbl_succ;
    emit_bb_box_banner(banner, "");
    if (beta_first) { emit_label_define(lbl_beta); emit_jmp(lbl_fail, JMP_JMP); emit_jmp(lbl_fail, JMP_JMP); }
    else            { emit_jmp(lbl_fail, JMP_JMP); emit_label_define(lbl_beta); emit_jmp(lbl_fail, JMP_JMP); }
}

/* EC-3: banner + pre-allocated zeta + alpha port_call + beta label + beta port_call. */
static void emit_bb_stateful(const char *banner, const char *arg, void *zeta, const char *fn_name, uint64_t fn_fallback,
                              bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_beta)
{
    emit_bb_box_banner(banner, arg ? arg : "");
    emit_bb_port_call((uint64_t)(uintptr_t)zeta, fn_name, fn_fallback, 0, lbl_succ, lbl_fail);
    emit_label_define(lbl_beta);
    emit_bb_port_call((uint64_t)(uintptr_t)zeta, fn_name, fn_fallback, 1, lbl_succ, lbl_fail);
}

/*====================================================================================================================*/
void emit_bb_icon_alt    (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("ICN_ALT",     "", icon_alt_new(),     "coro_bb_alternate",   (uint64_t)(uintptr_t)coro_bb_alternate,   s, f, b); }
void emit_bb_icon_bang   (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("ICN_BANG",    "", icon_bang_new(),    "coro_bb_bang_binary", (uint64_t)(uintptr_t)coro_bb_bang_binary, s, f, b); }
void emit_bb_icon_every  (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("ICN_EVERY",   "", icon_every_new(),   "coro_bb_every",       (uint64_t)(uintptr_t)coro_bb_every,       s, f, b); }
void emit_bb_icon_iterate(bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("ICN_ITERATE", "", icon_iterate_new(), "coro_bb_iterate",     (uint64_t)(uintptr_t)coro_bb_iterate,     s, f, b); }
void emit_bb_icon_lconcat(bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("ICN_LCONCAT", "", icon_lconcat_new(), "coro_bb_cat",         (uint64_t)(uintptr_t)coro_bb_cat,         s, f, b); }
void emit_bb_icon_limit  (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("ICN_LIMIT",   "", icon_limit_new(),   "coro_bb_limit",       (uint64_t)(uintptr_t)coro_bb_limit,       s, f, b); }
void emit_bb_icon_seq    (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("ICN_SEQ",     "", icon_seq_new(),     "coro_bb_seq_expr",    (uint64_t)(uintptr_t)coro_bb_seq_expr,    s, f, b); }
void emit_bb_icon_to     (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("ICN_TO",      "", icon_to_new(),      "coro_bb_to",          (uint64_t)(uintptr_t)coro_bb_to,          s, f, b); }
void emit_bb_icon_to_by  (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("ICN_TO_BY",   "", icon_to_by_new(),   "coro_bb_to_by",       (uint64_t)(uintptr_t)coro_bb_to_by,       s, f, b); }
/*====================================================================================================================*/
void emit_bb_xabrt  (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_jmp_pair("ABORT",  s, f, b, 0); }
void emit_bb_xcat   (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_jmp_pair("CAT",    s, f, b, 1); }
void emit_bb_xfail  (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_jmp_pair("FAIL",   s, f, b, 0); }
void emit_bb_xor    (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_jmp_pair("ALT",    s, f, b, 1); }
void emit_bb_xvar   (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_jmp_pair("VAR",    s, f, b, 1); }
/*====================================================================================================================*/
void emit_bb_xbal   (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("BAL",    "", bb_bal_new(),          "rt_bb_bal",   (uint64_t)(uintptr_t)rt_bb_bal,   s, f, b); }
void emit_bb_xfarb  (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("ARB",    "", bb_arb_new(),          "rt_bb_arb",   (uint64_t)(uintptr_t)rt_bb_arb,   s, f, b); }
void emit_bb_xlnth  (long long n,   bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("LEN",  "", bb_len_new((int)n),  "rt_bb_len",  (uint64_t)(uintptr_t)rt_bb_len,  s, f, b); }
void emit_bb_xrtb   (long long n,   bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("RTAB", "", bb_rtab_new((int)n), "rt_bb_rtab", (uint64_t)(uintptr_t)rt_bb_rtab, s, f, b); }
void emit_bb_xstar  (bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("REM",    "", bb_rem_new(),          "rt_bb_rem",   (uint64_t)(uintptr_t)rt_bb_rem,   s, f, b); }
void emit_bb_xtb    (long long n,   bb_label_t *s, bb_label_t *f, bb_label_t *b) { emit_bb_stateful("TAB",  "", bb_tab_new((int)n),  "rt_bb_tab",  (uint64_t)(uintptr_t)rt_bb_tab,  s, f, b); }
/*====================================================================================================================*/
void emit_bb_xarbn(bb_box_fn child_fn, bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    emit_bb_stateful("ARBNO", "", rt_bb_arbno_new(child_fn, NULL), "rt_bb_arbno", (uint64_t)(uintptr_t)rt_bb_arbno, s, f, b);
}
/*====================================================================================================================*/
void emit_bb_xbrkx(const char *chars, bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    emit_bb_stateful("BREAKX", chars ? chars : "", bb_breakx_new(chars), "rt_bb_breakx", (uint64_t)(uintptr_t)rt_bb_breakx, s, f, b);
}
/*====================================================================================================================*/
void emit_bb_xcallcap(bb_box_fn child_fn, const char *fnc_name,
                      bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    emit_bb_stateful("CALLCAP", fnc_name ? fnc_name : "",
                     bb_cap_new_call(child_fn, NULL, fnc_name, NULL, 0, NULL, 0, 0),
                     "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, s, f, b);
}
/*====================================================================================================================*/
void emit_bb_xfnce(bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    emit_bb_box_banner("FENCE", "");
    emit_jmp(s, JMP_JMP);               /* alpha: succeed zero-width — once succeeded, beta cuts */
    emit_label_define(b);
    emit_jmp(f, JMP_JMP);               /* beta: always fail (FENCE cuts backtracking) */
}
/*====================================================================================================================*/
void emit_bb_xfnme(bb_box_fn child_fn, const char *varname,
                   bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    emit_bb_stateful("CAP_IMM", varname ? varname : "",
                     bb_cap_new(child_fn, NULL, varname, NULL, 1),
                     "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, s, f, b);
}
/*====================================================================================================================*/
void emit_bb_xnme(bb_box_fn child_fn, const char *varname,
                  bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    emit_bb_stateful("CAP_COND", varname ? varname : "",
                     bb_cap_new(child_fn, NULL, varname, NULL, 0),
                     "rt_bb_cap", (uint64_t)(uintptr_t)rt_bb_cap, s, f, b);
}
/*====================================================================================================================*/
void emit_bb_xeps(bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    emit_bb_box_banner("EPS", "");
    emit_jmp(s, JMP_JMP);               /* alpha: always succeed zero-width */
    emit_label_define(b);
    emit_jmp(f, JMP_JMP);               /* beta: fail */
}
/*====================================================================================================================*/
void emit_bb_xsucf(bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    (void)f;
    emit_bb_box_banner("SUCCEED", "");
    emit_jmp(s, JMP_JMP);               /* alpha: always succeed zero-width */
    emit_label_define(b);
    emit_jmp(s, JMP_JMP);               /* beta: also succeed (generates another zero-width match) */
}
/*====================================================================================================================*/
#define TEMPLATE_ADDR_SIGMA   ((uint64_t)(uintptr_t)&Σ)
#define TEMPLATE_ADDR_SIGLEN  ((uint64_t)(uintptr_t)&Σlen)
static void emit_mov_rdx_imm32(int v)
{
    uint64_t val = (uint64_t)(uint32_t)v;
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:
        bb_emit_byte(0x48); bb_emit_byte(0xBA);
        bb_emit_byte((uint8_t)(val      )); bb_emit_byte((uint8_t)(val >>  8));
        bb_emit_byte((uint8_t)(val >> 16)); bb_emit_byte((uint8_t)(val >> 24));
        bb_emit_byte(0); bb_emit_byte(0); bb_emit_byte(0); bb_emit_byte(0);
        return;
    case EMIT_TEXT:
    case EMIT_TEXT_INLINE:
    case EMIT_MACRO_DEF: {
        char args[32]; snprintf(args, sizeof(args), "rdx, %d", v);
        if (emit_bb_is_format_mode()) { fmt_body_append("mov", args); return; }
        bb3c_format(bb_emit_out ? bb_emit_out : stdout, "", "mov", args);
        return;
    }
    }
}
void emit_bb_xchr(PATND_t *p, const char *lit_label,
                  bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    const char *lit = (p && p->STRVAL_fn) ? p->STRVAL_fn : "";
    int         len = (int)strlen(lit);
    char preview[40];
    if (len > 24) snprintf(preview, sizeof(preview), "'%.24s...'", lit);
    else          snprintf(preview, sizeof(preview), "'%s'",       lit);
    emit_bb_box_banner("LIT", preview);
    emit_bounds_check_delta_plus_len(len, TEMPLATE_ADDR_SIGLEN, f);
    emit_sigma_plus_delta_to_rdi(TEMPLATE_ADDR_SIGMA, TEMPLATE_ADDR_SIGLEN);
    emit_lea_rsi_strtab_sym(lit_label, (uint64_t)(uintptr_t)lit);
    emit_mov_rdx_imm32(len);
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
/*====================================================================================================================*/
void emit_bb_xdsar(const char *varname, bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    /* TEXT: emit .data block (name ptr, child quads, in_progress long).
     * BINARY: flat_data_* are no-ops; bb_dvar_bin_new allocates real zeta below. */
    char banner[80]; snprintf(banner, sizeof(banner), "*%s", varname ? varname : "");
    emit_bb_box_banner("DEREF", banner);
    int id = g_flat_node_id++;
    char zlbl[80], slbl[80];
    snprintf(zlbl, sizeof(zlbl), ".Ldvar%d_z",    id);
    snprintf(slbl, sizeof(slbl), ".Ldvar%d_name", id);
    const char *vn = varname ? varname : "";
    flat_data_section();
    flat3c_label(slbl);  flat_data_string(vn);
    flat3c_label(zlbl);
    flat_data_quad(slbl);  flat_data_quad("0");  flat_data_quad("0");
    flat_data_quad("0");   flat_data_long(0);    flat_data_long(0);
    flat_text_section();     flat_intel_syntax();
    void *z = bb_dvar_bin_new(vn);
    emit_bb_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "bb_deferred_var_exported", (uint64_t)(uintptr_t)bb_deferred_var_exported, 0, s, f);
    emit_label_define(b);
    emit_bb_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "bb_deferred_var_exported", (uint64_t)(uintptr_t)bb_deferred_var_exported, 1, s, f);
}
/*====================================================================================================================*/
void emit_bb_xatp(const char *varname, bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    /* TEXT: emit .data block with zeta struct (string + two longs + quad ptr).
     * BINARY: flat_data_* are no-ops; atp_t *z allocated below. */
    emit_bb_box_banner("USERPAT", varname ? varname : "");
    int id = g_flat_node_id++;
    char zlbl[80], vlbl[80];
    snprintf(zlbl, sizeof(zlbl), ".Latp%d_z",     id);
    snprintf(vlbl, sizeof(vlbl), ".Latp%d_vname", id);
    const char *vn = varname ? varname : "";
    flat_data_section();
    flat3c_label(vlbl);  flat_data_string(vn);
    flat3c_label(zlbl);  flat_data_long(0);  flat_data_long(0);  flat_data_quad(vlbl);
    flat_text_section();   flat_intel_syntax();
    atp_t *z = bb_atp_new(vn);
    emit_bb_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_atp", (uint64_t)(uintptr_t)rt_bb_atp, 0, s, f);
    emit_label_define(b);
    emit_bb_port_call_rip((uint64_t)(uintptr_t)z, zlbl, "rt_bb_atp", (uint64_t)(uintptr_t)rt_bb_atp, 1, s, f);
}
/*====================================================================================================================*/
void emit_bb_xposi(int n, bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    char args[32]; snprintf(args, sizeof(args), "%d", n);
    emit_bb_box_banner("POS", args);
    emit_load_delta_cmp_imm(n, s, f);
    emit_label_define(b);
    emit_jmp(f, JMP_JMP);
}
/*====================================================================================================================*/
#define ADDR_SIGLEN  ((uint64_t)(uintptr_t)&Σlen)
void emit_bb_xrpsi(int n, bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    char args[32]; snprintf(args, sizeof(args), "%d", n);
    emit_bb_box_banner("RPOS", args);
    emit_load_siglen_sub_cmp_delta(n, ADDR_SIGLEN, s, f);
    emit_label_define(b);
    emit_jmp(f, JMP_JMP);
}
/*====================================================================================================================*/
void emit_bb_charset(bb_box_fn c_fn, const char *c_fn_name, const char *kind_name,
                     const char *chars, bb_label_t *s, bb_label_t *f, bb_label_t *b)
{
    (void)c_fn;
    typedef struct { const char *chars; int delta; } cs_t;
    cs_t *z = calloc(1, sizeof(cs_t));
    z->chars = chars;
    const char *rt_name; uint64_t rt_fn;
    if      (c_fn_name && strcmp(c_fn_name, "bb_span")   == 0) { rt_name = "rt_bb_span";   rt_fn = (uint64_t)(uintptr_t)rt_bb_span;   }
    else if (c_fn_name && strcmp(c_fn_name, "bb_brk")    == 0) { rt_name = "rt_bb_brk";    rt_fn = (uint64_t)(uintptr_t)rt_bb_brk;    }
    else if (c_fn_name && strcmp(c_fn_name, "bb_any")    == 0) { rt_name = "rt_bb_any";    rt_fn = (uint64_t)(uintptr_t)rt_bb_any;    }
    else if (c_fn_name && strcmp(c_fn_name, "bb_notany") == 0) { rt_name = "rt_bb_notany"; rt_fn = (uint64_t)(uintptr_t)rt_bb_notany; }
    else                                                        { rt_name = "rt_bb_span";   rt_fn = (uint64_t)(uintptr_t)rt_bb_span;   }
    emit_bb_stateful(kind_name ? kind_name : "CHARSET", chars ? chars : "", z, rt_name, rt_fn, s, f, b);
}
/*====================================================================================================================*/
