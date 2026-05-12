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
#include "../rt/rt.h"   /* rt_bb_* declarations */

/* ---- all extern declarations in one block ---- */
extern DESCR_t coro_bb_alternate(void *zeta, int entry);
extern icn_alternate_state_t *icon_alt_new(void);
extern DESCR_t coro_bb_bang_binary(void *zeta, int entry);
extern icn_bang_binary_state_t *icon_bang_new(void);
extern DESCR_t coro_bb_every(void *zeta, int entry);
extern icn_every_state_t *icon_every_new(void);
extern DESCR_t coro_bb_iterate    (void *zeta, int entry);
extern DESCR_t coro_bb_list_iterate(void *zeta, int entry);
extern DESCR_t coro_bb_tbl_iterate (void *zeta, int entry);
extern DESCR_t coro_bb_record_iterate(void *zeta, int entry);
extern icn_iterate_state_t     *icon_iterate_new(void);
extern icn_list_iterate_state_t *icon_list_iterate_new(void);
extern icn_tbl_iterate_state_t  *icon_tbl_iterate_new(void);
extern icn_record_iterate_state_t *icon_record_iterate_new(void);
extern DESCR_t coro_bb_cat(void *zeta, int entry);
extern icn_cat_gen_state_t *icon_lconcat_new(void);
extern DESCR_t coro_bb_limit(void *zeta, int entry);
extern icn_limit_state_t *icon_limit_new(void);
extern DESCR_t coro_bb_seq_expr(void *zeta, int entry);
extern icn_seq_state_t *icon_seq_new(void);
extern DESCR_t coro_bb_to   (void *zeta, int entry);
extern DESCR_t coro_bb_to_by(void *zeta, int entry);
extern icn_to_state_t    *icon_to_new(void);
extern icn_to_by_state_t *icon_to_by_new(void);
extern atp_t  *bb_atp_new(const char *varname);
extern bal_t  *bb_bal_new(void);
extern brkx_t  *bb_breakx_new(const char *chars);
extern cap_t  *bb_cap_new_call(bb_box_fn child_fn, void *child_state,
                                const char *fnc_name,
                                DESCR_t *fnc_args, int fnc_nargs,
                                char **fnc_arg_names, int fnc_n_arg_names,
                                int immediate);
extern cap_t  *bb_cap_new(bb_box_fn child_fn, void *child_state,
                           const char *varname, DESCR_t *var_ptr, int immediate);
extern const char *Σ;
extern int         Σlen;
extern void   *bb_dvar_bin_new(const char *name);
extern arb_t  *bb_arb_new(void);
extern len_t  *bb_len_new (int n);
extern tab_t  *bb_tab_new (int n);
extern rtab_t *bb_rtab_new(int n);
extern rem_t  *bb_rem_new(void);
/* ------------------------------------------------ */

/* EDP-5: TEXT-mode helper for simple stateful boxes (single .long 0 ζ slot).
 * Emits: .data label + .long 0 + .text + push/lea/mov/call/pop/test/jne/jmp
 * for both α (port=0) and β (port=1) entries.
 * lbl_prefix examples: "fence", "rem" (used for .Lfence<id>_z). */
static void flat_text_simple_box(emitter_t *e,
                                  const char *lbl_prefix,
                                  const char *fn_name,
                                  bb_label_t *lbl_succ,
                                  bb_label_t *lbl_fail,
                                  bb_label_t *lbl_β)
{
    int id = g_flat_node_id++;
    char zlbl[80]; snprintf(zlbl, sizeof(zlbl), ".L%s%d_z", lbl_prefix, id);
    flat_data_section(e);
    flat3c_label(e, zlbl);
    flat_data_long(e, 0);
    flat_text_section(e);
    flat_intel_syntax(e);
    char rdi_arg[120]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", zlbl);
    flat_box_call(e, rdi_arg, fn_name, 0);
    flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    flat_box_call(e, rdi_arg, fn_name, 1);
    flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_icon_alt(emitter_t *e,
                      bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_ALT", "");
    icn_alternate_state_t *z = icon_alt_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_alternate",
                   (uint64_t)(uintptr_t)coro_bb_alternate,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_alternate",
                   (uint64_t)(uintptr_t)coro_bb_alternate,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_icon_bang(emitter_t *e,
                       bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_BANG", "");
    icn_bang_binary_state_t *z = icon_bang_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_bang_binary",
                   (uint64_t)(uintptr_t)coro_bb_bang_binary,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_bang_binary",
                   (uint64_t)(uintptr_t)coro_bb_bang_binary,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_icon_every(emitter_t *e,
                        bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_EVERY", "");
    icn_every_state_t *z = icon_every_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_every",
                   (uint64_t)(uintptr_t)coro_bb_every,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_every",
                   (uint64_t)(uintptr_t)coro_bb_every,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_icon_iterate(emitter_t *e,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_ITERATE", "");
    icn_iterate_state_t *z = icon_iterate_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_iterate",
                   (uint64_t)(uintptr_t)coro_bb_iterate,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_iterate",
                   (uint64_t)(uintptr_t)coro_bb_iterate,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_icon_lconcat(emitter_t *e,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_LCONCAT", "");
    icn_cat_gen_state_t *z = icon_lconcat_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_cat",
                   (uint64_t)(uintptr_t)coro_bb_cat,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_cat",
                   (uint64_t)(uintptr_t)coro_bb_cat,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_icon_limit(emitter_t *e,
                        bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_LIMIT", "");
    icn_limit_state_t *z = icon_limit_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_limit",
                   (uint64_t)(uintptr_t)coro_bb_limit,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_limit",
                   (uint64_t)(uintptr_t)coro_bb_limit,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_icon_seq(emitter_t *e,
                      bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_SEQ", "");
    icn_seq_state_t *z = icon_seq_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_seq_expr",
                   (uint64_t)(uintptr_t)coro_bb_seq_expr,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_seq_expr",
                   (uint64_t)(uintptr_t)coro_bb_seq_expr,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_icon_to(emitter_t *e,
                     bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_TO", "");
    icn_to_state_t *z = icon_to_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_to", (uint64_t)(uintptr_t)coro_bb_to,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_to", (uint64_t)(uintptr_t)coro_bb_to,
                   1, lbl_succ, lbl_fail);
}
void emit_bb_icon_to_by(emitter_t *e,
                        bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_TO_BY", "");
    icn_to_by_state_t *z = icon_to_by_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_to_by", (uint64_t)(uintptr_t)coro_bb_to_by,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_to_by", (uint64_t)(uintptr_t)coro_bb_to_by,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_xabrt(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e; (void)lbl_succ;
    t_bb_box_banner("ABORT", "");
    /* α: always fail */
    t_emit_jmp(lbl_fail, JMP_JMP);
    t_label_define(lbl_β);
    /* β: always fail */
    t_emit_jmp(lbl_fail, JMP_JMP);
}
/*====================================================================================================================*/
void emit_bb_xarbn(emitter_t *e, bb_box_fn child_fn,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ARBNO", "");
    void *z = rt_bb_arbno_new(child_fn, NULL);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_arbno",
                   (uint64_t)(uintptr_t)rt_bb_arbno,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_arbno",
                   (uint64_t)(uintptr_t)rt_bb_arbno,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_xatp(emitter_t *e, const char *varname,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    t_bb_box_banner("USERPAT", varname ? varname : "");
    if (bb_emit_mode == EMIT_TEXT || bb_emit_mode == EMIT_TEXT_INLINE) {
        int id = g_flat_node_id++;
        char zlbl[80], vlbl[80];
        snprintf(zlbl, sizeof(zlbl), ".Latp%d_z",     id);
        snprintf(vlbl, sizeof(vlbl), ".Latp%d_vname", id);
        const char *vn = varname ? varname : "";
        flat_data_section(e);
        flat3c_label(e, vlbl); flat_data_string(e, vn);
        flat3c_label(e, zlbl);
        flat_data_long(e, 0); flat_data_long(e, 0);
        flat_data_quad(e, vlbl);
        flat_text_section(e);
        flat_intel_syntax(e);
        char rdi_arg[120]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", zlbl);
        flat_box_call(e, rdi_arg, "rt_bb_atp", 0);
        flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        t_label_define(lbl_β);
        flat_box_call(e, rdi_arg, "rt_bb_atp", 1);
        flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        return;
    }
    (void)e;
    atp_t *z = bb_atp_new(varname ? varname : "");
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_atp",
                   (uint64_t)(uintptr_t)rt_bb_atp,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_atp",
                   (uint64_t)(uintptr_t)rt_bb_atp,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_xbal(emitter_t *e,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("BAL", "");
    bal_t *z = bb_bal_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_bal",
                   (uint64_t)(uintptr_t)rt_bb_bal,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_bal",
                   (uint64_t)(uintptr_t)rt_bb_bal,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_xbrkx(emitter_t *e,
                   const char *chars,
                   bb_label_t *lbl_succ,
                   bb_label_t *lbl_fail,
                   bb_label_t *lbl_β)
{
    (void)e;
    brkx_t *z = bb_breakx_new(chars);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_breakx",
                   (uint64_t)(uintptr_t)rt_bb_breakx,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_breakx",
                   (uint64_t)(uintptr_t)rt_bb_breakx,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_xcallcap(emitter_t *e, bb_box_fn child_fn,
                      const char *fnc_name,
                      bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("CALLCAP", fnc_name ? fnc_name : "");
    cap_t *z = bb_cap_new_call(child_fn, NULL, fnc_name,
                                NULL, 0, NULL, 0, 0);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap",
                   (uint64_t)(uintptr_t)rt_bb_cap,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap",
                   (uint64_t)(uintptr_t)rt_bb_cap,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_xcat(emitter_t *e,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("CAT", "");
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
/*====================================================================================================================*/
#define TEMPLATE_ADDR_SIGMA   ((uint64_t)(uintptr_t)&Σ)
#define TEMPLATE_ADDR_SIGLEN  ((uint64_t)(uintptr_t)&Σlen)
static void t_mov_rdx_imm32(int v)
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
    case EMIT_MACRO_DEF: {
        char args[32]; snprintf(args, sizeof(args), "rdx, %d", v);
        bb3c_format(bb_emit_out ? bb_emit_out : stdout, "", "mov", args);
        return;
    }
    }
}
void emit_bb_xchr(emitter_t *e, PATND_t *p,
                  const char *lit_label,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                  bb_label_t *lbl_β)
{
    (void)e;
    const char *lit = (p && p->STRVAL_fn) ? p->STRVAL_fn : "";
    int len = (int)strlen(lit);
    char preview[40];
    if (len > 24) snprintf(preview, sizeof(preview), "'%.24s...'", lit);
    else          snprintf(preview, sizeof(preview), "'%s'", lit);
    t_bb_box_banner("LIT", preview);
    t_bounds_check_delta_plus_len(len, TEMPLATE_ADDR_SIGLEN, lbl_fail);
    t_sigma_plus_delta_to_rdi(TEMPLATE_ADDR_SIGMA, TEMPLATE_ADDR_SIGLEN);
    t_lea_rsi_strtab_sym(lit_label, (uint64_t)(uintptr_t)lit);
    t_mov_rdx_imm32(len);
    t_call_sym_plt("memcmp", (uint64_t)(uintptr_t)memcmp);
    t_test_eax_eax();
    t_emit_jmp(lbl_fail, JMP_JNE);
    t_add_delta_imm(len);
    t_emit_jmp(lbl_succ, JMP_JMP);
    t_label_define(lbl_β);
    t_sub_delta_imm(len);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
/*====================================================================================================================*/
void emit_bb_xdsar(emitter_t *e, const char *varname,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    char banner[80]; snprintf(banner, sizeof(banner), "*%s", varname ? varname : "");
    t_bb_box_banner("DEREF", banner);
    if (bb_emit_mode == EMIT_TEXT || bb_emit_mode == EMIT_TEXT_INLINE) {
        int id = g_flat_node_id++;
        char zlbl[80], slbl[80];
        snprintf(zlbl, sizeof(zlbl), ".Ldvar%d_z",    id);
        snprintf(slbl, sizeof(slbl), ".Ldvar%d_name", id);
        const char *vn = varname ? varname : "";
        flat_data_section(e);
        flat3c_label(e, slbl); flat_data_string(e, vn);
        flat3c_label(e, zlbl);
        flat_data_quad(e, slbl);   /* name */
        flat_data_quad(e, "0");    /* child_fn */
        flat_data_quad(e, "0");    /* child_state */
        flat_data_quad(e, "0");    /* child_size */
        flat_data_long(e, 0);      /* in_progress */
        flat_data_long(e, 0);      /* padding */
        flat_text_section(e);
        flat_intel_syntax(e);
        char rdi_arg[120]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", zlbl);
        flat_box_call(e, rdi_arg, "bb_deferred_var_exported", 0);
        flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        t_label_define(lbl_β);
        flat_box_call(e, rdi_arg, "bb_deferred_var_exported", 1);
        flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        return;
    }
    (void)e;
    void *z = bb_dvar_bin_new(varname ? varname : "");
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_deferred_var_exported",
                   0,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_deferred_var_exported",
                   0,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_xeps(emitter_t *e,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("EPS", "");
    t_emit_jmp(lbl_succ, JMP_JMP);
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
/*====================================================================================================================*/
void emit_bb_xfail(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e; (void)lbl_succ;
    t_bb_box_banner("FAIL", "");
    t_emit_jmp(lbl_fail, JMP_JMP);
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
/*====================================================================================================================*/
void emit_bb_xfarb(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ARB", "");
    arb_t *z = bb_arb_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_arb",
                   (uint64_t)(uintptr_t)rt_bb_arb,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_arb",
                   (uint64_t)(uintptr_t)rt_bb_arb,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_xfnce(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("FENCE", "");
    /* α: succeed zero-width (no state needed — once succeeded, β cuts) */
    t_emit_jmp(lbl_succ, JMP_JMP);
    t_label_define(lbl_β);
    /* β: always fail (FENCE cuts backtracking) */
    t_emit_jmp(lbl_fail, JMP_JMP);
}
/*====================================================================================================================*/
void emit_bb_xfnme(emitter_t *e, bb_box_fn child_fn, const char *varname,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("CAP_IMM", varname ? varname : "");
    cap_t *z = bb_cap_new(child_fn, NULL, varname, NULL, 1);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap",
                   (uint64_t)(uintptr_t)rt_bb_cap,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap",
                   (uint64_t)(uintptr_t)rt_bb_cap,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_intcur(emitter_t *e,
                    bb_box_fn c_fn,
                    const char *c_fn_name,
                    const char *kind_name,
                    long long num,
                    bb_label_t *lbl_succ,
                    bb_label_t *lbl_fail,
                    bb_label_t *lbl_β)
{
    (void)kind_name; (void)e; (void)c_fn;
    void *z;
    const char *rt_name;
    uint64_t rt_fn;
    if (c_fn_name && c_fn_name[3] == 'l') {
        z = bb_len_new((int)num); rt_name = "rt_bb_len"; rt_fn = (uint64_t)(uintptr_t)rt_bb_len;
    } else if (c_fn_name && c_fn_name[3] == 't') {
        z = bb_tab_new((int)num); rt_name = "rt_bb_tab"; rt_fn = (uint64_t)(uintptr_t)rt_bb_tab;
    } else if (c_fn_name && c_fn_name[3] == 'r') {
        z = bb_rtab_new((int)num); rt_name = "rt_bb_rtab"; rt_fn = (uint64_t)(uintptr_t)rt_bb_rtab;
    } else {
        int *r = calloc(2, sizeof(int)); r[0] = (int)num; z = r;
        rt_name = "rt_bb_len"; rt_fn = (uint64_t)(uintptr_t)rt_bb_len;
    }
    t_bb_port_call((uint64_t)(uintptr_t)z, rt_name, rt_fn, 0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, rt_name, rt_fn, 1, lbl_succ, lbl_fail);
}
void emit_bb_xlnth(emitter_t *e, long long num,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    emit_bb_intcur(e, NULL, "bb_len", "LEN", num, lbl_succ, lbl_fail, lbl_β);
}
/*====================================================================================================================*/
void emit_bb_xnme(emitter_t *e, bb_box_fn child_fn, const char *varname,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("CAP_COND", varname ? varname : "");
    cap_t *z = bb_cap_new(child_fn, NULL, varname, NULL, 0);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap",
                   (uint64_t)(uintptr_t)rt_bb_cap,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_cap",
                   (uint64_t)(uintptr_t)rt_bb_cap,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_xor(emitter_t *e,
                 bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ALT", "");
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
/*====================================================================================================================*/
void emit_bb_xposi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    char args[32]; snprintf(args, sizeof(args), "%d", n);
    t_bb_box_banner("POS", args);
    t_load_delta_cmp_imm(n, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
/*====================================================================================================================*/
#define ADDR_SIGLEN ((uint64_t)(uintptr_t)&Σlen)
void emit_bb_xrpsi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    char args[32]; snprintf(args, sizeof(args), "%d", n);
    t_bb_box_banner("RPOS", args);
    t_load_siglen_sub_cmp_delta(n, ADDR_SIGLEN, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
/*====================================================================================================================*/
void emit_bb_xrtb(emitter_t *e, long long num,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    emit_bb_intcur(e, NULL, "bb_rtab", "RTAB", num, lbl_succ, lbl_fail, lbl_β);
}
/*====================================================================================================================*/
void emit_bb_charset(emitter_t *e,
                     bb_box_fn c_fn,
                     const char *c_fn_name,
                     const char *kind_name,
                     const char *chars,
                     bb_label_t *lbl_succ,
                     bb_label_t *lbl_fail,
                     bb_label_t *lbl_β)
{
    (void)kind_name; (void)e; (void)c_fn;
    typedef struct { const char *chars; int delta; } cs_t;
    cs_t *z = calloc(1, sizeof(cs_t));
    z->chars = chars;
    /* Map old bb_* name to rt_bb_* name+ptr */
    const char *rt_name; uint64_t rt_fn;
    if      (c_fn_name && strcmp(c_fn_name, "bb_span")   == 0) { rt_name = "rt_bb_span";   rt_fn = (uint64_t)(uintptr_t)rt_bb_span;   }
    else if (c_fn_name && strcmp(c_fn_name, "bb_brk")    == 0) { rt_name = "rt_bb_brk";    rt_fn = (uint64_t)(uintptr_t)rt_bb_brk;    }
    else if (c_fn_name && strcmp(c_fn_name, "bb_any")    == 0) { rt_name = "rt_bb_any";    rt_fn = (uint64_t)(uintptr_t)rt_bb_any;    }
    else if (c_fn_name && strcmp(c_fn_name, "bb_notany") == 0) { rt_name = "rt_bb_notany"; rt_fn = (uint64_t)(uintptr_t)rt_bb_notany; }
    else                                                        { rt_name = "rt_bb_span";   rt_fn = (uint64_t)(uintptr_t)rt_bb_span;   }
    t_bb_port_call((uint64_t)(uintptr_t)z, rt_name, rt_fn, 0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, rt_name, rt_fn, 1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_xstar(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("REM", "");
    /* α: match rest of subject — Δ=Σlen, always succeed (zero or more chars) */
    /* In BINARY: call rt_bb_rem_alpha (sets Δ=Σlen, returns non-fail).
     * Simplest: call a trivial rt helper or just jmp lbl_succ (Δ advance handled by caller).
     * SNOBOL4 REM is: match everything from Δ to end, no backtrack.
     * In brokered mode t_bb_port_call handles Δ, so we need a call.
     * Use rt_bb_rem stub (stateless): α succeeds with full remainder, β fails. */
    rem_t *z = bb_rem_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_rem",
                   (uint64_t)(uintptr_t)rt_bb_rem,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "rt_bb_rem",
                   (uint64_t)(uintptr_t)rt_bb_rem,
                   1, lbl_succ, lbl_fail);
}
/*====================================================================================================================*/
void emit_bb_xsucf(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e; (void)lbl_fail;
    t_bb_box_banner("SUCCEED", "");
    /* α: always succeed zero-width */
    t_emit_jmp(lbl_succ, JMP_JMP);
    t_label_define(lbl_β);
    /* β: also succeed (SUCCEED always generates another zero-width match) */
    t_emit_jmp(lbl_succ, JMP_JMP);
}
/*====================================================================================================================*/
void emit_bb_xtb(emitter_t *e, long long num,
                 bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    emit_bb_intcur(e, NULL, "bb_tab", "TAB", num, lbl_succ, lbl_fail, lbl_β);
}
/*====================================================================================================================*/
void emit_bb_xvar(emitter_t *e,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e; (void)lbl_succ;
    t_bb_box_banner("VAR", "");
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
/*====================================================================================================================*/
