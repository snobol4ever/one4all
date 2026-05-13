

#include "emit_bb_flat.h"
#include "emit_bb_gen.h"
#include "emit.h"
#include "snobol4.h"
#include "bb_box.h"
#include "emit_templates.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

/* Constructor externs (implemented in bb_boxes.c / stmt_exec.c) */
extern len_t   *bb_len_new(int n);
extern tab_t   *bb_tab_new(int n);
extern rtab_t  *bb_rtab_new(int n);
extern fence_t *bb_fence_new(void);
extern arb_t   *bb_arb_new(void);
extern void    *bb_arbno_new(bb_box_fn fn, void *state);
extern brkx_t  *bb_breakx_new(const char *chars);
extern rem_t   *bb_rem_new(void);
extern atp_t   *bb_atp_new(const char *varname);
extern cap_t   *bb_cap_new(bb_box_fn child_fn, void *child_state,
                            const char *varname, DESCR_t *var_ptr, int immediate);
extern void    *bb_dvar_bin_new(const char *name);

#define FLAT_BUF_MAX  (16 * 1024)


int g_flat_node_id   = 0;
static int g_flat_slot_count = 0;


#define FLAT_DATA_BUF_MAX     (32 * 1024)
#define FLAT_DATA_LBL_MAX     32

static char   g_flat_data_buf[FLAT_DATA_BUF_MAX];
static size_t g_flat_data_len    = 0;
static int    g_flat_data_active = 0;
static int    g_flat_data_any    = 0;
static int    g_flat_data_just_closed = 0;

static char   g_flat_data_pending_lbl[160] = "";

static char   g_flat_data_block_lbls[FLAT_DATA_LBL_MAX][96];
static int    g_flat_data_block_nlbls = 0;

static void data_buf_reset(void)
{
    g_flat_data_len = 0;
    g_flat_data_active = 0;
    g_flat_data_any = 0;
    g_flat_data_just_closed = 0;
    g_flat_data_block_nlbls = 0;
    g_flat_data_pending_lbl[0] = '\0';
}

static void data_buf_appendf(const char *fmt, ...)
{
    if (g_flat_data_len >= FLAT_DATA_BUF_MAX) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(g_flat_data_buf + g_flat_data_len,
                      FLAT_DATA_BUF_MAX - g_flat_data_len, fmt, ap);
    va_end(ap);
    if (n > 0) {
        size_t left = FLAT_DATA_BUF_MAX - g_flat_data_len;
        g_flat_data_len += ((size_t)n < left) ? (size_t)n : left;
    }
}

/* Append a fully-formatted three-column data line to the buffer.  Mirrors */
static void data_buf_three_col(const char *lbl, const char *act, const char *got)
{
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
    int n = snprintf(line, sizeof(line), "%-24s%-16s %s",
                     eff_lbl, act ? act : "", got ? got : "");
    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
    while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) line[--n] = '\0';
    data_buf_appendf("%s\n", line);
}

/* Defer a label to fuse with the next data line.  Used by flat3c_label's */
static void data_buf_pend_label(const char *name)
{
    if (g_flat_data_pending_lbl[0]) {
        char line[256];
        int n = snprintf(line, sizeof(line), "%-24s", g_flat_data_pending_lbl);
        if (n > 0) {
            while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) line[--n] = '\0';
            data_buf_appendf("%s\n", line);
        }
    }
    snprintf(g_flat_data_pending_lbl, sizeof(g_flat_data_pending_lbl),
             "%s:", name ? name : "");
}

/* Flush any unfused pending label to the buffer as a standalone line.  Called */
static void data_buf_flush_pending_label(void)
{
    if (!g_flat_data_pending_lbl[0]) return;
    char line[256];
    int n = snprintf(line, sizeof(line), "%-24s", g_flat_data_pending_lbl);
    if (n > 0) {
        while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) line[--n] = '\0';
        data_buf_appendf("%s\n", line);
    }
    g_flat_data_pending_lbl[0] = '\0';
}

/* EM-7c-capture: callback installed by sm_codegen_x64_emit to collect cap fixups. */
static void (*g_cap_fixup_cb)(void *cap_ptr, const char *child_α_label) = NULL;

void bb_flat_set_cap_fixup_cb(void (*cb)(void *cap_ptr, const char *child_α_label))
{
    g_cap_fixup_cb = cb;
}


#define SYM_SIGMA   "\xCE\xA3"
#define SYM_SIGLEN  "\xCE\xA3""len"
#define SYM_DELTA   "\xCE\x94"
#define ADDR_SIGMA   ((uint64_t)(uintptr_t)&Σ)
#define ADDR_SIGLEN  ((uint64_t)(uintptr_t)&Σlen)
#define ADDR_DELTA   ((uint64_t)(uintptr_t)&Δ)

/* ── intern_str hook — set by sm_codegen_x64_emit before calling bb_build_flat_text */
static const char *(*g_flat_intern_str)(const char *s) = NULL;

void bb_flat_set_intern_str(const char *(*fn)(const char *))
{
    g_flat_intern_str = fn;
}

/* ── EM-7c-s-file-beautify helpers (2026-05-09) ────────────────────────────── */

static void flat3c(const char *lbl, const char *act, const char *got)
{
    if (!g_is_text) return;
    FILE *f = bb_emit_out;
    if (!f) return;
    bb3c_format(f, lbl ? lbl : "", act ? act : "", got ? got : "");
}

static void flat3c_action(const char *act, const char *args)
{
    flat3c("", act, args ? args : "");
}

/* Forward decls for data-buffer helpers used by flat3c_label below; full */
static void data_buf_remember_label(const char *name);

void flat3c_label(const char *name)
{
    if (!g_is_text) return;
    if (g_flat_data_active) {
        data_buf_pend_label(name);
        data_buf_remember_label(name);
        return;
    }
    char buf[160]; snprintf(buf, sizeof(buf), "%s:", name);
    flat3c(buf, "", "");
}

/* EM-FORMAT-BB-DATA-CONSOLIDATE: helper to record a label-name into the */
static void data_buf_remember_label(const char *name)
{
    if (g_flat_data_block_nlbls >= FLAT_DATA_LBL_MAX) return;
    snprintf(g_flat_data_block_lbls[g_flat_data_block_nlbls],
             sizeof(g_flat_data_block_lbls[0]), "%s", name ? name : "");
    g_flat_data_block_nlbls++;
}

static void data_buf_emit_block_comment(void)
{
    g_flat_data_block_nlbls = 0;
}

void flat_data_section(void)
{
    if (!g_is_text) return;
    g_flat_data_active = 1;
    g_flat_data_any    = 1;
    g_flat_data_block_nlbls = 0;
}

void flat_text_section(void)
{
    if (!g_is_text) return;
    if (g_flat_data_active) {
        data_buf_emit_block_comment();
        g_flat_data_active = 0;
        g_flat_data_just_closed = 1;
        return;
    }
    flat3c("", ".section", ".text");
}

void flat_intel_syntax(void)
{
    if (!g_is_text) return;
    if (g_flat_data_active) return;
    if (g_flat_data_just_closed) {
        g_flat_data_just_closed = 0;
        return;
    }
    flat3c("", ".intel_syntax", "noprefix");
}

void flat_data_string(const char *s)
{
    if (!g_is_text) return;
    char esc[1024];
    size_t o = 0;
    if (o < sizeof(esc)) esc[o++] = '"';
    for (const char *cp = s ? s : ""; *cp && o + 5 < sizeof(esc); cp++) {
        unsigned char c = (unsigned char)*cp;
        if (c == '"' || c == '\\') { esc[o++] = '\\'; esc[o++] = (char)c; }
        else if (c >= 32 && c < 127) { esc[o++] = (char)c; }
        else { o += snprintf(esc + o, sizeof(esc) - o, "\\%03o", c); }
    }
    if (o + 1 < sizeof(esc)) esc[o++] = '"';
    esc[o] = '\0';
    if (g_flat_data_active) data_buf_three_col("", ".string", esc);
    else                    flat3c("", ".string", esc);
}

void flat_data_quad(const char *arg)
{
    if (!g_is_text) return;
    if (g_flat_data_active) data_buf_three_col("", ".quad", arg ? arg : "0");
    else                    flat3c("", ".quad", arg ? arg : "0");
}

void flat_data_quad_int(long long v)
{
    if (!g_is_text) return;
    char buf[32]; snprintf(buf, sizeof(buf), "%lld", v);
    if (g_flat_data_active) data_buf_three_col("", ".quad", buf);
    else                    flat3c("", ".quad", buf);
}

void flat_data_long(long long v)
{
    if (!g_is_text) return;
    char buf[32]; snprintf(buf, sizeof(buf), "%lld", v);
    if (g_flat_data_active) data_buf_three_col("", ".long", buf);
    else                    flat3c("", ".long", buf);
}

void flat_data_zero(int n)
{
    if (!g_is_text) return;
    char buf[16]; snprintf(buf, sizeof(buf), "%d", n);
    if (g_flat_data_active) data_buf_three_col("", ".zero", buf);
    else                    flat3c("", ".zero", buf);
}

void flat_globl(const char *name)
{
    if (!g_is_text) return;
    flat3c("", ".globl", name);
}

/* Emit the three-line box-call sequence: `lea rdi,[rip+ζ]` / `mov esi,mode` / */
void flat_box_call(const char *rdi_load,
                          const char *fn, int mode)
{
    if (!g_is_text) return;
    flat3c_action("push", "r10");
    flat3c_action("lea", rdi_load);
    char esi_arg[32]; snprintf(esi_arg, sizeof(esi_arg), "esi, %d", mode);
    flat3c_action("mov", esi_arg);
    char call_arg[64]; snprintf(call_arg, sizeof(call_arg), "%s@PLT", fn);
    flat3c_action("call", call_arg);
    flat3c_action("pop", "r10");
}

/* Variant: arbno's box call uses a slot pointer dereference rather than */
void flat_box_call_slot(const char *slot_lbl,
                               const char *fn, int mode)
{
    if (!g_is_text) return;
    flat3c_action("push", "r10");
    char rdi_arg[160]; snprintf(rdi_arg, sizeof(rdi_arg),
                                "rdi, qword ptr [rip + %s]", slot_lbl);
    flat3c_action("mov", rdi_arg);
    char esi_arg[32]; snprintf(esi_arg, sizeof(esi_arg), "esi, %d", mode);
    flat3c_action("mov", esi_arg);
    char call_arg[64]; snprintf(call_arg, sizeof(call_arg), "%s@PLT", fn);
    flat3c_action("call", call_arg);
    flat3c_action("pop", "r10");
}

/* EM-FORMAT-BB-LAW (TRIPLE-FUSION): emit */
void flat_box_dispatch_jne_jmp(bb_label_t *lbl_succ,
                                      bb_label_t *lbl_fail)
{
    if (!g_is_text) return;
    char buf[512];
    int o = 0;
    o += snprintf(buf + o, sizeof(buf) - o, "rax, rax;");
    while (o < 27 && o < (int)sizeof(buf) - 1) buf[o++] = ' ';
    buf[o] = '\0';
    snprintf(buf + o, sizeof(buf) - o, "jne %s; jmp %s",
             lbl_succ ? lbl_succ->name : "?",
             lbl_fail ? lbl_fail->name : "?");
    flat3c_action("test", buf);
}

/* EM-FORMAT-BB-LAW (TRIPLE-FUSION): emit the entry dispatch */
void flat_box_entry_dispatch(bb_label_t *lbl_alpha_body,
                                    bb_label_t *lbl_beta)
{
    if (!g_is_text) return;
    char buf[512];
    int o = 0;
    o += snprintf(buf + o, sizeof(buf) - o, "esi, 0;");
    while (o < 27 && o < (int)sizeof(buf) - 1) buf[o++] = ' ';
    buf[o] = '\0';
    snprintf(buf + o, sizeof(buf) - o, "je %s; jmp %s",
             lbl_alpha_body ? lbl_alpha_body->name : "?",
             lbl_beta       ? lbl_beta->name       : "?");
    flat3c_action("cmp", buf);
}

/* ── EM-FORMAT-BB-BOX-BANNERS helpers (2026-05-09) ───────────────────────── */

/* xkind_name lives in snobol4_pattern.c as static; we need our own local */
static const char *flat_xkind_name(XKIND_t k) {
    switch (k) {
        case XCHR:     return "CHR";
        case XSPNC:    return "SPAN";
        case XBRKC:    return "BREAK";
        case XANYC:    return "ANY";
        case XNNYC:    return "NOTANY";
        case XLNTH:    return "LEN";
        case XPOSI:    return "POS";
        case XRPSI:    return "RPOS";
        case XTB:      return "TAB";
        case XRTB:     return "RTAB";
        case XFARB:    return "ARB";
        case XARBN:    return "ARBNO";
        case XSTAR:    return "REM";
        case XFNCE:    return "FENCE";
        case XFAIL:    return "FAIL";
        case XABRT:    return "ABORT";
        case XSUCF:    return "SUCCEED";
        case XBAL:     return "BAL";
        case XEPS:     return "EPS";
        case XCAT:     return "CAT";
        case XOR:      return "ALT";
        case XDSAR:    return "DEREF";
        case XFNME:    return "CAP_IMM";
        case XNME:     return "CAP_COND";
        case XCALLCAP: return "CALLCAP";
        case XVAR:     return "VAR";
        case XATP:     return "USERPAT";
        case XBRKX:    return "BREAKX";
        default:       return "?";
    }
}

/* Append at most `cap-1` chars from `s` (escaping " and non-printables as `.`) */
static int patnd_buf_append(char *buf, size_t cap, size_t *o, const char *s)
{
    if (!s) return 0;
    while (*s && *o + 1 < cap) {
        unsigned char c = (unsigned char)*s++;
        if (c < 0x20 || c == 0x7f) { buf[(*o)++] = '.'; }
        else { buf[(*o)++] = (char)c; }
    }
    buf[*o] = '\0';
    return (*o + 1 >= cap) ? 1 : 0;
}

static int patnd_buf_appendf(char *buf, size_t cap, size_t *o, const char *fmt, ...)
{
    if (*o + 1 >= cap) return 1;
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf + *o, cap - *o, fmt, ap);
    va_end(ap);
    if (n < 0) return 1;
    if ((size_t)n >= cap - *o) { *o = cap - 1; return 1; }
    *o += (size_t)n;
    return 0;
}

/* Render PATND_t tree in SNOBOL4-source style, single-line, into buf. */
static void patnd_to_sno_r(const PATND_t *p, char *buf, size_t cap,
                           size_t *o, int depth)
{
    if (*o + 4 >= cap) return;
    if (!p) { patnd_buf_append(buf, cap, o, "()"); return; }
    if (depth >= 16) { patnd_buf_append(buf, cap, o, "..."); return; }
    switch (p->kind) {
    case XCHR: {
        const char *s = p->STRVAL_fn ? p->STRVAL_fn : "";
        patnd_buf_appendf(buf, cap, o, "'%s'", s);
        break;
    }
    case XSPNC: case XBRKC: case XANYC: case XNNYC: case XBRKX: {
        patnd_buf_appendf(buf, cap, o, "%s('%s')",
                          flat_xkind_name(p->kind),
                          p->STRVAL_fn ? p->STRVAL_fn : "");
        break;
    }
    case XLNTH: case XTB: case XRTB: case XPOSI: case XRPSI:
        patnd_buf_appendf(buf, cap, o, "%s(%lld)",
                          flat_xkind_name(p->kind), (long long)p->num);
        break;
    case XFARB:    patnd_buf_append(buf, cap, o, "ARB");      break;
    case XSTAR:    patnd_buf_append(buf, cap, o, "REM");      break;
    case XBAL:     patnd_buf_append(buf, cap, o, "BAL");      break;
    case XSUCF:    patnd_buf_append(buf, cap, o, "SUCCEED");  break;
    case XABRT:    patnd_buf_append(buf, cap, o, "ABORT");    break;
    case XEPS:     patnd_buf_append(buf, cap, o, "EPSILON");  break;
    case XFAIL:    patnd_buf_append(buf, cap, o, "FAIL");     break;
    case XVAR:     patnd_buf_append(buf, cap, o, "<var>");    break;
    case XCAT:
        for (int i = 0; i < p->nchildren; i++) {
            if (i > 0 && *o + 1 < cap) buf[(*o)++] = ' ';
            patnd_to_sno_r(p->children[i], buf, cap, o, depth + 1);
        }
        buf[*o] = '\0';
        break;
    case XOR:
        for (int i = 0; i < p->nchildren; i++) {
            if (i > 0) patnd_buf_append(buf, cap, o, " | ");
            patnd_to_sno_r(p->children[i], buf, cap, o, depth + 1);
        }
        break;
    case XARBN:
        patnd_buf_append(buf, cap, o, "ARBNO(");
        if (p->nchildren > 0) patnd_to_sno_r(p->children[0], buf, cap, o, depth + 1);
        patnd_buf_append(buf, cap, o, ")");
        break;
    case XFNCE:
        if (p->nchildren > 0) {
            patnd_buf_append(buf, cap, o, "FENCE(");
            patnd_to_sno_r(p->children[0], buf, cap, o, depth + 1);
            patnd_buf_append(buf, cap, o, ")");
        } else {
            patnd_buf_append(buf, cap, o, "FENCE");
        }
        break;
    case XFNME:
        if (p->nchildren > 0) patnd_to_sno_r(p->children[0], buf, cap, o, depth + 1);
        patnd_buf_append(buf, cap, o, " $ <var>");
        break;
    case XNME:
        if (p->nchildren > 0) patnd_to_sno_r(p->children[0], buf, cap, o, depth + 1);
        patnd_buf_append(buf, cap, o, " . <var>");
        break;
    case XCALLCAP:
        if (p->nchildren > 0) patnd_to_sno_r(p->children[0], buf, cap, o, depth + 1);
        patnd_buf_append(buf, cap, o, " . *<fn>()");
        break;
    case XDSAR:
        patnd_buf_appendf(buf, cap, o, "*%s",
                          p->STRVAL_fn ? p->STRVAL_fn : "<var>");
        break;
    case XATP: {
        patnd_buf_appendf(buf, cap, o, "%s(",
                          p->STRVAL_fn ? p->STRVAL_fn : "<fn>");
        for (int i = 0; i < p->nargs; i++) {
            if (i > 0) patnd_buf_append(buf, cap, o, ", ");
            patnd_buf_append(buf, cap, o, "<arg>");
        }
        patnd_buf_append(buf, cap, o, ")");
        break;
    }
    default:
        patnd_buf_appendf(buf, cap, o, "<%s>", flat_xkind_name(p->kind));
        break;
    }
}

static void patnd_to_sno_string(const PATND_t *p, char *buf, size_t cap)
{
    if (!buf || cap == 0) return;
    buf[0] = '\0';
    size_t o = 0;
    patnd_to_sno_r(p, buf, cap, &o, 0);
}


#define BB_BANNER_RULE_LEN 119

/* EM-FORMAT-BB-PORT-COMPLETION-LONE-LABEL-FIX (sess 2026-05-09): */
void emit_flat_banner_rule(char ch)
{
    if (!g_is_text) return;
    char buf[BB_BANNER_RULE_LEN + 4];
    buf[0] = '#';
    for (int i = 0; i < BB_BANNER_RULE_LEN; i++) buf[1 + i] = ch;
    buf[1 + BB_BANNER_RULE_LEN] = '\0';
    emit_fprintf_raw("%s\n", buf);
}

/* Pattern-blob banner: emit at the top of each pat_inv_<id> blob. */
static void emit_flat_pat_banner(const char *prefix, PATND_t *p)
{
    if (!g_is_text) return;
    (void)prefix; (void)p;
    emit_flat_banner_rule('=');
}

/* Per-box banner: emit before the α-arm of a box. */
void emit_flat_box_banner(const char *kind,
                          const char *args, const char *label_prefix)
{
    if (!g_is_text) return;
    emit_flat_banner_rule('-');
    if (args && *args) {
        emit_fprintf_raw("#                       BOX %s(%s)  [%s]\n", kind, args,
                label_prefix ? label_prefix : "");
    } else {
        emit_fprintf_raw("#                       BOX %s  [%s]\n", kind,
                label_prefix ? label_prefix : "");
    }
}

/* ── forward declarations ────────────────────────────────────────────────── */
static void emit_flat_node(PATND_t *p,
                           bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                           bb_label_t *lbl_β);

/* ── XCAT ───────────────────────────────────────────────────────────────── */
static void emit_flat_xcat(PATND_t *p,
                           bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                           bb_label_t *lbl_β)
{
    int id = g_flat_node_id++;
    bb_label_t mid_γ, right_ω, left_β, right_β, xcat_ω;
    bb_label_initf(&mid_γ,   "xcat%d_\xCE\xB3",         id);
    bb_label_initf(&right_ω, "xcat%d_right_\xCF\x89",   id);
    bb_label_initf(&left_β,  "xcat%d_left_\xCE\xB2",    id);
    bb_label_initf(&right_β, "xcat%d_right_\xCE\xB2",   id);
    bb_label_initf(&xcat_ω,  "xcat%d_\xCF\x89",         id);
    if (p->nchildren == 0) {
        emit_jmp_label(lbl_succ, JMP_JMP);
        emit_label_define_bb(lbl_β); emit_jmp_label(lbl_fail, JMP_JMP);
        emit_label_define_bb(&xcat_ω); emit_jmp_label(lbl_fail, JMP_JMP);
        emit_label_define_bb(&mid_γ); emit_label_define_bb(&right_ω);
        emit_label_define_bb(&right_β); emit_label_define_bb(&left_β);
        return;
    }
    if (p->nchildren == 1) {
        emit_flat_node(p->children[0], lbl_succ, lbl_fail, &left_β);
        emit_label_define_bb(lbl_β); emit_jmp_label(&left_β, JMP_JMP);
        emit_label_define_bb(&xcat_ω); emit_jmp_label(lbl_fail, JMP_JMP);
        emit_label_define_bb(&mid_γ); emit_label_define_bb(&right_ω); emit_label_define_bb(&right_β);
        return;
    }
    emit_flat_node(p->children[0], &mid_γ, &xcat_ω, &left_β);
    emit_label_define_bb(&mid_γ);
    if (p->nchildren == 2) {
        emit_flat_node(p->children[1], lbl_succ, &right_ω, &right_β);
    } else {
        int nc = p->nchildren;
        bb_label_t *mids  = alloca(sizeof(bb_label_t) * (nc - 1));
        bb_label_t *betas = alloca(sizeof(bb_label_t) * (nc - 1));
        for (int i = 0; i < nc - 1; i++) {
            bb_label_initf(&mids[i],  "xcat%d_mid%d_\xCE\xB3", id, i+1);
            bb_label_initf(&betas[i], "xcat%d_mid%d_\xCE\xB2", id, i+1);
        }
        for (int i = 1; i < nc; i++) {
            bb_label_t *s = (i < nc-1) ? &mids[i-1] : lbl_succ;
            emit_flat_node(p->children[i], s, &right_ω, &betas[i-1]);
            if (i < nc-1) emit_label_define_bb(&mids[i-1]);
        }
    }
    emit_label_define_bb(&right_ω); emit_jmp_label(&left_β, JMP_JMP);
    emit_label_define_bb(lbl_β); emit_jmp_label(&right_β, JMP_JMP);
    emit_label_define_bb(&xcat_ω);  emit_jmp_label(lbl_fail, JMP_JMP);
}

/* ── XOR (alternation) ──────────────────────────────────────────────────── */
static void emit_flat_alt(PATND_t *p,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                          bb_label_t *lbl_β)
{
    int id = g_flat_node_id++;
    int nc = p->nchildren;
    if (nc == 0) { emit_label_define_bb(lbl_β); emit_jmp_label(lbl_fail, JMP_JMP); return; }
    if (nc == 1) { emit_flat_node(p->children[0], lbl_succ, lbl_fail, lbl_β); return; }
    bb_label_t *ci_βs = alloca((size_t)nc * sizeof(bb_label_t));
    bb_label_t *ci_ωs = alloca((size_t)nc * sizeof(bb_label_t));
    for (int i = 0; i < nc; i++) {
        bb_label_initf(&ci_βs[i], "alt%d_c%d_\xCE\xB2", id, i);
        bb_label_initf(&ci_ωs[i], "alt%d_c%d_\xCF\x89", id, i);
    }
    for (int i = 0; i < nc; i++) {
        bb_label_t *f = (i < nc-1) ? &ci_ωs[i] : &ci_ωs[nc-1];
        emit_flat_node(p->children[i], lbl_succ, f, &ci_βs[i]);
        if (i < nc-1) emit_label_define_bb(&ci_ωs[i]);
        else          emit_label_define_bb(&ci_ωs[nc-1]);
    }
    emit_jmp_label(lbl_fail, JMP_JMP);
    emit_label_define_bb(lbl_β); emit_jmp_label(&ci_βs[0], JMP_JMP);
}

/* ── leaf: literal string ───────────────────────────────────────────────── */
static void emit_flat_lit(const char *lit, int len,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                          bb_label_t *lbl_β) __attribute__((unused));
static void emit_flat_lit(const char *lit, int len,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                          bb_label_t *lbl_β)
{
    emit_load_delta();
    emit_add_eax_imm32((uint32_t)len);
    emit_cmp_eax_siglen(ADDR_SIGLEN);
    emit_jmp_label(lbl_fail, JMP_JG);
    emit_sigma_plus_delta(ADDR_SIGMA);
    emit_mov_rdi_rax();
    emit_mov_rdx_imm64((uint64_t)(uint32_t)len);
    if (g_is_text && g_flat_intern_str) {
        const char *lbl = g_flat_intern_str(lit);
        emit_sym_lea_rcx(lbl, (uint64_t)(uintptr_t)lit);
        emit_fprintf_raw("    mov     rsi, rcx\n");
    } else {
        emit_mov_rsi_imm64((uint64_t)(uintptr_t)lit);
    }
    emit_call_sym_plt("memcmp", (uint64_t)(uintptr_t)memcmp);
    emit_test_eax_eax();
    emit_jmp_label(lbl_fail, JMP_JNE);
    emit_add_delta_imm(len);
    emit_jmp_label(lbl_succ, JMP_JMP);
    emit_label_define_bb(lbl_β);
    emit_sub_delta_imm(len);
    emit_jmp_label(lbl_fail, JMP_JMP);
}

/* ── leaf: epsilon ──────────────────────────────────────────────────────── */

/* ── leaf: POS(n) ───────────────────────────────────────────────────────── */

/* ── leaf: charset (ANY/NOTANY/SPAN/BRK) ───────────────────────────────── */
__attribute__((unused))
static void emit_flat_charset_call(bb_box_fn c_fn,
                                   const char *c_fn_name,
                                   const char *chars,
                                   bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                                   bb_label_t *lbl_β)
{
    if (g_is_text) {
        const char *kind = "CHARSET";
        if      (c_fn_name && !strcmp(c_fn_name, "bb_span"))   kind = "SPAN";
        else if (c_fn_name && !strcmp(c_fn_name, "bb_any"))    kind = "ANY";
        else if (c_fn_name && !strcmp(c_fn_name, "bb_brk"))    kind = "BREAK";
        else if (c_fn_name && !strcmp(c_fn_name, "bb_notany")) kind = "NOTANY";
        char preview[40];
        if (chars && *chars) {
            int n = (int)strlen(chars);
            if (n > 24) snprintf(preview, sizeof(preview), "'%.24s...'", chars);
            else        snprintf(preview, sizeof(preview), "'%s'", chars);
        } else {
            preview[0] = '\0';
        }
        emit_flat_box_banner(kind, preview, lbl_succ->name);
        int id = g_flat_node_id++;
        char zlbl[64], slbl[64];
        snprintf(zlbl, sizeof(zlbl), ".Lcs%d_z",     id);
        snprintf(slbl, sizeof(slbl), ".Lcs%d_chars", id);
        flat_data_section();
        flat3c_label(slbl);
        flat_data_string(chars);
        flat3c_label(zlbl);
        flat_data_quad(slbl);
        flat_data_long(0);
        flat_data_long(0);
        flat_text_section();
        flat_intel_syntax();
        char rdi_arg[96]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", zlbl);
        flat3c_action("lea", rdi_arg);
        flat3c_action("mov", "esi, 0");
        emit_call_sym_plt(c_fn_name, (uint64_t)(uintptr_t)c_fn);
        flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);
        emit_label_define_bb(lbl_β);
        flat3c_action("lea", rdi_arg);
        flat3c_action("mov", "esi, 1");
        emit_call_sym_plt(c_fn_name, (uint64_t)(uintptr_t)c_fn);
        flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);
    } else {
        typedef struct { const char *chars; int delta; } cs_t;
        cs_t *z = calloc(1, sizeof(cs_t));
        z->chars = chars;
        emit_mov_rdi_imm64((uint64_t)(uintptr_t)z);
        emit_mov_esi_imm32(0);
        emit_call_sym_plt(c_fn_name, (uint64_t)(uintptr_t)c_fn);
        emit_test_rax_rax();
        emit_jmp_label(lbl_succ, JMP_JNE);
        emit_jmp_label(lbl_fail, JMP_JMP);
        emit_label_define_bb(lbl_β);
        emit_mov_rdi_imm64((uint64_t)(uintptr_t)z);
        emit_mov_esi_imm32(1);
        emit_call_sym_plt(c_fn_name, (uint64_t)(uintptr_t)c_fn);
        emit_test_rax_rax();
        emit_jmp_label(lbl_succ, JMP_JNE);
        emit_jmp_label(lbl_fail, JMP_JMP);
    }
}


typedef struct {
    bb_box_fn   c_fn;
    const char *c_fn_name;
    const char *kind_name;
    const char *chars;
} charset_text_arg_t;

/* Text-path body for emit_bb_charset.  Has access to all of bb_flat.c's */
static void charset_text_body(bb_label_t *lbl_succ,
                              bb_label_t *lbl_fail,
                              bb_label_t *lbl_β,
                              void *arg_)
{
    const charset_text_arg_t *a = (const charset_text_arg_t *)arg_;
    const char *chars     = a->chars     ? a->chars     : "";
    const char *c_fn_name = a->c_fn_name ? a->c_fn_name : "";
    const char *kind      = a->kind_name ? a->kind_name : "CHARSET";
    char preview[40];
    if (chars && *chars) {
        int n = (int)strlen(chars);
        if (n > 24) snprintf(preview, sizeof(preview), "'%.24s...'", chars);
        else        snprintf(preview, sizeof(preview), "'%s'", chars);
    } else {
        preview[0] = '\0';
    }
    emit_flat_box_banner(kind, preview, lbl_succ->name);
    int id = g_flat_node_id++;
    char zlbl[64], slbl[64];
    snprintf(zlbl, sizeof(zlbl), ".Lcs%d_z",     id);
    snprintf(slbl, sizeof(slbl), ".Lcs%d_chars", id);
    flat_data_section();
    flat3c_label(slbl);
    flat_data_string(chars);
    flat3c_label(zlbl);
    flat_data_quad(slbl);
    flat_data_long(0);
    flat_data_long(0);
    flat_text_section();
    flat_intel_syntax();
    char rdi_arg[96];
    snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", zlbl);
    flat3c_action("lea", rdi_arg);
    flat3c_action("mov", "esi, 0");
    emit_call_sym_plt(c_fn_name, (uint64_t)(uintptr_t)a->c_fn);
    flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);
    emit_label_define_bb(lbl_β);
    flat3c_action("lea", rdi_arg);
    flat3c_action("mov", "esi, 1");
    emit_call_sym_plt(c_fn_name, (uint64_t)(uintptr_t)a->c_fn);
    flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);
}

/* Per-kind wrappers called from emit_flat_node.  Each builds a */

/* ── XBRKX text-body callback (EM-MODE4-IS-MODE3-DUMP-i) ─────────────────── */

extern brkx_t  *bb_breakx_new(const char *chars);


typedef struct {
    const char *chars;
} brkx_text_arg_t;

/* Text-path body for emit_bb_xbrkx.  Access to bb_flat.c static helpers. */
static void brkx_text_body(bb_label_t *lbl_succ,
                            bb_label_t *lbl_fail,
                            bb_label_t *lbl_β,
                            void *arg_)
{
    const brkx_text_arg_t *a = (const brkx_text_arg_t *)arg_;
    const char *chars = a->chars ? a->chars : "";
    char preview[40];
    if (chars && *chars) {
        int n = (int)strlen(chars);
        if (n > 24) snprintf(preview, sizeof(preview), "'%.24s...'", chars);
        else        snprintf(preview, sizeof(preview), "'%s'", chars);
    } else {
        preview[0] = '\0';
    }
    emit_flat_box_banner("BREAKX", preview, lbl_succ->name);
    int id = g_flat_node_id++;
    char zlbl[64], slbl[64];
    snprintf(zlbl, sizeof(zlbl), ".Lbrkx%d_z",     id);
    snprintf(slbl, sizeof(slbl), ".Lbrkx%d_chars", id);
    flat_data_section();
    flat3c_label(slbl);
    flat_data_string(chars);
    flat3c_label(zlbl);
    flat_data_quad(slbl);
    flat_data_long(0);
    flat_text_section();
    flat_intel_syntax();
    char rdi_arg[96];
    snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", zlbl);
    flat3c_action("lea", rdi_arg);
    flat3c_action("mov", "esi, 0");
    emit_call_sym_plt("bb_breakx", 0);
    flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);
    emit_label_define_bb(lbl_β);
    flat3c_action("lea", rdi_arg);
    flat3c_action("mov", "esi, 1");
    emit_call_sym_plt("bb_breakx", 0);
    flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);
}


typedef struct {
    bb_box_fn   c_fn;
    const char *c_fn_name;
    const char *kind_name;
    const char *lbl_prefix;
    long long   num;
    int         data_pad;
} intcur_text_arg_t;

static void intcur_text_body(bb_label_t *lbl_succ,
                             bb_label_t *lbl_fail,
                             bb_label_t *lbl_β,
                             void *arg_)
{
    const intcur_text_arg_t *a = (const intcur_text_arg_t *)arg_;
    char banner_args[32];
    snprintf(banner_args, sizeof(banner_args), "%lld", a->num);
    emit_flat_box_banner(a->kind_name, banner_args, lbl_succ->name);
    int id = g_flat_node_id++;
    char lbl[64];
    snprintf(lbl, sizeof(lbl), ".L%s%d_z", a->lbl_prefix, id);
    flat_data_section();
    flat3c_label(lbl);
    flat_data_long(a->num);
    if (a->data_pad) flat_data_long(0);
    flat_text_section();
    flat_intel_syntax();
    char rdi_arg[96];
    snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", lbl);
    flat_box_call(rdi_arg, a->c_fn_name, 0);
    flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);
    emit_label_define_bb(lbl_β);
    flat_box_call(rdi_arg, a->c_fn_name, 1);
    flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);
}


/* ── emit_flat_node dispatch ─────────────────────────────────────────────── */
extern int memcmp(const void *, const void *, size_t);

/* Generic two-call emitter: α calls fn(ζ,0), β calls fn(ζ,1), result nonzero=success */
void emit_flat_box_call(bb_box_fn fn, const char *fn_name,
                               void *z,
                               bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                               bb_label_t *lbl_β)
{
    emit_push_r10();
    emit_mov_rdi_imm64((uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(0);
    emit_call_sym_plt(fn_name, (uint64_t)(uintptr_t)fn);
    emit_pop_r10();
    if (g_is_text) {
        flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);
    } else {
        emit_test_rax_rax();
        emit_jmp_label(lbl_succ, JMP_JNE);
        emit_jmp_label(lbl_fail, JMP_JMP);
    }
    emit_label_define_bb(lbl_β);
    emit_push_r10();
    emit_mov_rdi_imm64((uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(1);
    emit_call_sym_plt(fn_name, (uint64_t)(uintptr_t)fn);
    emit_pop_r10();
    if (g_is_text) {
        flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);
    } else {
        emit_test_rax_rax();
        emit_jmp_label(lbl_succ, JMP_JNE);
        emit_jmp_label(lbl_fail, JMP_JMP);
    }
}

static void emit_flat_node(PATND_t *p,
                           bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                           bb_label_t *lbl_β)
{
    if (!p) { emit_bb_xeps(lbl_succ, lbl_fail, lbl_β); return; }
    switch (p->kind) {
    case XCHR: {
        const char *lit = p->STRVAL_fn ? p->STRVAL_fn : "";
        const char *lit_label = (g_flat_intern_str && g_is_text)
                                ? g_flat_intern_str(lit) : NULL;
        emit_bb_xchr(p, lit_label, lbl_succ, lbl_fail, lbl_β);
        break;
    }
    case XEPS:  emit_bb_xeps (lbl_succ, lbl_fail, lbl_β); break;
    case XFAIL: emit_bb_xfail(lbl_succ, lbl_fail, lbl_β); break;
    case XPOSI: emit_bb_xposi((int)p->num, lbl_succ, lbl_fail, lbl_β); break;
    case XRPSI: emit_bb_xrpsi((int)p->num, lbl_succ, lbl_fail, lbl_β); break;
    case XCAT:  emit_flat_xcat(p, lbl_succ, lbl_fail, lbl_β); break;
    case XOR:   emit_flat_alt (p, lbl_succ, lbl_fail, lbl_β); break;
    case XSPNC: emit_bb_charset(NULL,   "bb_span",   "SPAN",   p->STRVAL_fn?p->STRVAL_fn:"", lbl_succ, lbl_fail, lbl_β); break;
    case XANYC: emit_bb_charset(NULL,    "bb_any",    "ANY",    p->STRVAL_fn?p->STRVAL_fn:"", lbl_succ, lbl_fail, lbl_β); break;
    case XBRKC: emit_bb_charset(NULL,    "bb_brk",    "BREAK",  p->STRVAL_fn?p->STRVAL_fn:"", lbl_succ, lbl_fail, lbl_β); break;
    case XNNYC: emit_bb_charset(NULL, "bb_notany", "NOTANY", p->STRVAL_fn?p->STRVAL_fn:"", lbl_succ, lbl_fail, lbl_β); break;
    case XLNTH: emit_bb_xlnth((long long)p->num, lbl_succ, lbl_fail, lbl_β); break;
    case XTB:   emit_bb_xtb  ((long long)p->num, lbl_succ, lbl_fail, lbl_β); break;
    case XRTB:  emit_bb_xrtb ((long long)p->num, lbl_succ, lbl_fail, lbl_β); break;
    case XFNCE: emit_bb_xfnce(lbl_succ, lbl_fail, lbl_β); break;
    case XFARB: emit_bb_xfarb(lbl_succ, lbl_fail, lbl_β); break;
    case XSTAR: emit_bb_xstar(lbl_succ, lbl_fail, lbl_β); break;
    case XBRKX: emit_bb_xbrkx(p->STRVAL_fn ? p->STRVAL_fn : "", lbl_succ, lbl_fail, lbl_β); break;
    case XATP:  emit_bb_xatp (p->STRVAL_fn, lbl_succ, lbl_fail, lbl_β); break;
    case XDSAR: emit_bb_xdsar(p->STRVAL_fn, lbl_succ, lbl_fail, lbl_β); break;
    default:
        emit_label_define_bb(lbl_β);
        emit_jmp_label(lbl_fail, JMP_JMP);
        emit_jmp_label(lbl_fail, JMP_JMP);
        break;
    }
}
/* A pattern is invariant when its BB graph structure is fully known at */
static int flat_is_eligible(PATND_t *p)
{
    if (!p) return 1;
    if (p->kind == XVAR) return 0;
    if (p->kind == XCAT && p->nchildren > 2) return 0;
    if (p->kind == XNME || p->kind == XFNME || p->kind == XARBN || p->kind == XCALLCAP) return 0;
    for (int i = 0; i < p->nchildren; i++)
        if (!flat_is_eligible(p->children[i])) return 0;
    return 1;
}

/* ── shared emission body ─────────────────────────────────────────────────── */
static int emit_flat_body(PATND_t *p,
                            const char *prefix, int text_externalise, int brokered)
{
    bb_label_t lbl_α, lbl_α_body, lbl_succ, lbl_fail, lbl_β;
    bb_label_initf(&lbl_α,      "%s_α",      prefix);
    bb_label_initf(&lbl_α_body, "%s_α_body", prefix);
    bb_label_initf(&lbl_succ,       "%s_γ",      prefix);
    bb_label_initf(&lbl_fail,       "%s_ω",      prefix);
    bb_label_initf(&lbl_β,       "%s_β",       prefix);
    if (text_externalise) data_buf_reset();
    if (text_externalise) {
        emit_flat_pat_banner(prefix, p);
        emit_global_sym(lbl_α.name);
        emit_global_sym(lbl_β.name);
        emit_global_sym(lbl_succ.name);
        emit_global_sym(lbl_fail.name);
        emit_label_define_bb(&lbl_α);
    }
    {   emit_sym_lea_r10("delta", ADDR_DELTA);
    }
    flat_box_entry_dispatch(&lbl_α_body, &lbl_β);
    emit_label_define_bb(&lbl_α_body);
    if (!text_externalise) emit_label_define_bb(&lbl_α);
    emit_flat_node(p, &lbl_succ, &lbl_fail, &lbl_β);
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
        emit_fprintf_raw("%.*s", (int)g_flat_data_len, g_flat_data_buf);
        flat3c("", ".section", ".text");
        data_buf_reset();
    }
    return 0;
}

/* ── public entry points ──────────────────────────────────────────────────── */

bb_box_fn bb_build_flat(PATND_t *p)
{
    if (!flat_is_eligible(p)) return NULL;
    bb_buf_t buf = bb_alloc(FLAT_BUF_MAX);
    if (!buf) return NULL;
    g_flat_slot_count = 0; g_flat_node_id = 0;
    emitter_init_binary(buf, FLAT_BUF_MAX);
    emit_flat_body(p, "pat_flat", 0, 0);
    int nbytes = emitter_end();
    if (nbytes <= 0 || nbytes > FLAT_BUF_MAX) { bb_free(buf, FLAT_BUF_MAX); return NULL; }
    bb_seal(buf, (size_t)nbytes);
    return (bb_box_fn)buf;
}

/* ── bb_build_brokered — EM-BB-PURGE-1 / EDP-7 ─────────────────────────── */
bb_box_fn bb_build_brokered(PATND_t *p)
{
    if (!flat_is_eligible(p)) return NULL;
    bb_buf_t buf = bb_alloc(FLAT_BUF_MAX);
    if (!buf) return NULL;
    g_flat_slot_count = 0; g_flat_node_id = 0;
    emit_mode_set(EMIT_BINARY_BROKERED, NULL);
    emitter_init_binary(buf, FLAT_BUF_MAX);
    emit_brokered_prologue();
    emit_flat_body(p, "pat_brok", 0, 1);
    int nbytes = emitter_end();
    emit_mode_set(EMIT_BINARY_WIRED, NULL);
    if (nbytes <= 0 || nbytes > FLAT_BUF_MAX) { bb_free(buf, FLAT_BUF_MAX); return NULL; }
    bb_seal(buf, (size_t)nbytes);
    return (bb_box_fn)buf;
}

int bb_build_flat_text(PATND_t *p, FILE *out, const char *prefix)
{
    if (!flat_is_eligible(p)) return -1;
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    int rc = emit_flat_body(p, prefix, 1, 0);
    emitter_end();
    bb3c_flush_pending();
    return rc;
}

void bb_build_flat_text_reset(void)
{
    g_flat_slot_count = 0;
    g_flat_node_id    = 0;
}

/* ── EM-7c-bb-macros: BB macro library writer ──────────────────────────── */

static void bm_line(FILE *f, const char *lbl, const char *act, const char *got)
{
    char line[512];
    int n = snprintf(line, sizeof(line), "%-24s%-16s %s",
                     lbl ? lbl : "", act ? act : "", got ? got : "");
    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
    while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) n--;
    line[n] = '\0';
    fprintf(f, "%s\n", line);
}
static void bm_macro(FILE *f, const char *name, const char *args)
{
    char decl[160];
    if (args && args[0])
        snprintf(decl, sizeof(decl), "%s %s", name, args);
    else
        snprintf(decl, sizeof(decl), "%s", name);
    bm_line(f, "", ".macro", decl);
}
static void bm_endm(FILE *f)  { bm_line(f, "", ".endm", ""); }
static void bm_op(FILE *f, const char *mn, const char *args)
{
    bm_line(f, "", mn, args ? args : "");
}
static void bm_jmp(FILE *f, const char *cond, const char *tgt)
{
    char arg[128]; snprintf(arg, sizeof(arg), "\\%s", tgt);
    bm_line(f, "", cond, arg);
}

int bb_macros_write_to_path(const char *path)
{
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
    bm_op   (f, "lea", "rcx, [rip + \xCE\xA3len]");
    bm_op   (f, "mov", "eax, [rcx]");
    bm_endm (f);
    bm_macro(f, "EPS_\xCE\xB1", "lbl_succ");
    bm_jmp  (f, "jmp", "lbl_succ");
    bm_endm (f);
    bm_macro(f, "EPS_\xCE\xB2", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    bm_macro(f, "FAIL_\xCE\xB1", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    bm_macro(f, "FAIL_\xCE\xB2", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    bm_macro(f, "RPOS_\xCE\xB1", "n, lbl_succ, lbl_fail");
    bm_op   (f, "SIGLEN_LOAD", "");
    bm_op   (f, "sub", "eax, \\n");
    bm_op   (f, "mov", "ecx, eax");
    bm_op   (f, "DELTA_LOAD", "");
    bm_op   (f, "cmp", "eax, ecx");
    bm_jmp  (f, "jne", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_succ");
    bm_endm (f);
    bm_macro(f, "RPOS_\xCE\xB2", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    bm_macro(f, "POS_\xCE\xB1", "n, lbl_succ, lbl_fail");
    bm_op   (f, "DELTA_LOAD", "");
    bm_op   (f, "cmp", "eax, \\n");
    bm_jmp  (f, "jne", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_succ");
    bm_endm (f);
    bm_macro(f, "POS_\xCE\xB2", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    fprintf(f, "# === END bb macro library ===\n");
    return fclose(f) == 0 ? 0 : -1;
}
