/*
 * bb_flat.c — Flat-Glob Invariant Pattern Emitter (M-DYN-FLAT)
 *
 * Emits an entire invariant PATND_t tree as one contiguous x86-64 blob.
 * All sub-boxes are inlined flat; control flows via direct jmp, never call/ret.
 *
 * EM-7b'': Zero byte knowledge in this file.  Every emission is a named
 * helper call (ev_load_delta, ev_mov_rax_imm64, etc.) that routes through
 * emitter_v * -> emit_insn -> TEXT (readable mnemonic) or BINARY (bytes).
 * The walker reads as a description of pattern-matcher semantics, not x86.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  RT-129 / M-DYN-FLAT (EM-7b'')
 */

#include "bb_flat.h"
#include "bb_emit.h"
#include "emitter_v.h"
#include "snobol4.h"
#include "bb_box.h"
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
extern void    *bb_arbno_new(bb_box_fn fn, void *state);  /* arbno_t* opaque */
extern brkx_t  *bb_breakx_new(const char *chars);
extern rem_t   *bb_rem_new(void);
extern atp_t   *bb_atp_new(const char *varname);
extern cap_t   *bb_cap_new(bb_box_fn child_fn, void *child_state,
                            const char *varname, DESCR_t *var_ptr, int immediate);
extern void    *bb_dvar_bin_new(const char *name);

#define FLAT_BUF_MAX  (16 * 1024)

static int g_flat_node_id   = 0;
static int g_flat_slot_count = 0;

/* ── EM-FORMAT-BB-DATA-CONSOLIDATE state (2026-05-10) ─────────────────────────
 *
 * Per-blob deferred-data buffer.  In TEXT mode, every `.section .data` block
 * inside a `pat_inv_<id>` blob would normally interleave with `.text` 4-5
 * times (charset slots, capture state, len/tab/rtab/fence/arb/star/breakx
 * slots).  Instead we buffer all data emissions into `g_flat_data_buf` and
 * dump them as ONE consolidated `.section .data` block at the END of the
 * blob's body via `flat_data_consolidate_flush`.
 *
 * Original sites get a `# data: <labels>` comment so the reader can still
 * tie each box to its locals.
 *
 * Flow:
 *   flat_data_section()  -> g_flat_data_active = 1; reset per-block label list
 *   flat_data_*() helpers route through `data_buf_appendf` instead of `flat3c`
 *   flat3c_label()       -> if active, also routes; collects label name in
 *                           per-block list for the inline comment
 *   flat_text_section()  -> if active, emit `# data: name1, name2` to main
 *                           stream and toggle active off (we never actually
 *                           switched the main stream out of `.text`)
 *   flat_intel_syntax()  -> if was deferred just now, no-op (stayed in text)
 *
 * At end of `flat_emit_body_v`: `flat_data_consolidate_flush(e)` emits ONE
 * `.section .data` directive, dumps the buffer, restores `.section .text`
 * and `.intel_syntax noprefix`, then resets state.
 *
 * BINARY mode: state is unused; helpers gate on `e->is_text` first.         */

#define FLAT_DATA_BUF_MAX     (32 * 1024)   /* per-blob; plenty for 5 corpora */
#define FLAT_DATA_LBL_MAX     32            /* per-block; capture has ~3      */

static char   g_flat_data_buf[FLAT_DATA_BUF_MAX];
static size_t g_flat_data_len    = 0;
static int    g_flat_data_active = 0;       /* 1 = currently inside a .data block */
static int    g_flat_data_any    = 0;       /* 1 = at least one block this blob   */
static int    g_flat_data_just_closed = 0;  /* 1 = next flat_intel_syntax is the
                                             * trailing one from the box's
                                             * `.section .text; .intel_syntax`
                                             * pair — suppress (we never left
                                             * intel-syntax on the main stream). */
/* In-buffer pending-label fusion (mirrors bb3c_format's logic but local to
 * the data buffer): a label-only line waits for the next content line and
 * fuses into col-1.  Flushed standalone if the buffer ends or another label
 * arrives first. */
static char   g_flat_data_pending_lbl[160] = "";
/* Per-block (one .data...flat_text_section() block) label list, used to build
 * the `# data: ...` comment emitted to the main text stream when the block
 * closes. */
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

/* Append a fully-formatted three-column data line to the buffer.  Mirrors
 * `bb3c_format`'s `%-24s%-16s %s` shape but writes to memory; right-trims
 * trailing whitespace to keep the audit invariant clean.  Consumes any
 * in-buffer pending label as the fused col-1 (label-fusion mirrors
 * bb_emit.c's bb3c_format behaviour). */
static void data_buf_three_col(const char *lbl, const char *act, const char *got)
{
    /* Fusion: if caller passed an empty label and a label is pending, use it. */
    char fused_lbl[160];
    const char *eff_lbl = lbl ? lbl : "";
    if ((eff_lbl[0] == '\0') && g_flat_data_pending_lbl[0]) {
        snprintf(fused_lbl, sizeof(fused_lbl), "%s", g_flat_data_pending_lbl);
        g_flat_data_pending_lbl[0] = '\0';
        eff_lbl = fused_lbl;
    } else if (eff_lbl[0] != '\0' && g_flat_data_pending_lbl[0]) {
        /* Caller has its own label and a label is pending: emit pending
         * standalone, then fall through to use caller's label. */
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
    /* Right-trim trailing spaces/tabs (matches bb3c_format behaviour). */
    while (n > 0 && (line[n-1] == ' ' || line[n-1] == '\t')) line[--n] = '\0';
    data_buf_appendf("%s\n", line);
}

/* Defer a label to fuse with the next data line.  Used by flat3c_label's
 * data-active path (replaces the prior immediate three-column emission). */
static void data_buf_pend_label(const char *name)
{
    /* If a label is already pending, flush it standalone before this one. */
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

/* Flush any unfused pending label to the buffer as a standalone line.  Called
 * from data_buf_emit_consolidated_block before dumping. */
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

/* EM-7c-capture: callback installed by sm_codegen_x64_emit to collect cap fixups.
 * Called from flat_emit_node when XNME/XFNME emits a child sub-blob. */
static void (*g_cap_fixup_cb)(void *cap_ptr, const char *child_α_label) = NULL;

void bb_flat_set_cap_fixup_cb(void (*cb)(void *cap_ptr, const char *child_α_label))
{
    g_cap_fixup_cb = cb;
}

/* ── address constants — EM-7c-symbolic ──────────────────────────────────── */
/* Symbol names for globals exported by libscrip_rt.so.                      */
/* TEXT mode: ev_lea_rcx_sym emits  lea rcx, [rip + sym]                     */
/* BINARY mode: ev_lea_rcx_sym emits mov rcx, imm64  (process-address)       */
#define SYM_SIGMA   "\xCE\xA3"          /* UTF-8: Σ */
#define SYM_SIGLEN  "\xCE\xA3""len"     /* UTF-8: Σlen */
#define SYM_DELTA   "\xCE\x94"          /* UTF-8: Δ */
#define ADDR_SIGMA   ((uint64_t)(uintptr_t)&Σ)
#define ADDR_SIGLEN  ((uint64_t)(uintptr_t)&Σlen)
#define ADDR_DELTA   ((uint64_t)(uintptr_t)&Δ)

/* ── intern_str hook — set by sm_codegen_x64_emit before calling bb_build_flat_text */
/* NULL = no strtab available (BINARY mode or standalone use).               */
static const char *(*g_flat_intern_str)(emitter_v *e, const char *s) = NULL;

void bb_flat_set_intern_str(const char *(*fn)(emitter_v *, const char *))
{
    g_flat_intern_str = fn;
}

/* ── EM-7c-s-file-beautify helpers (2026-05-09) ──────────────────────────────
 *
 * Minimal three-column emitters for the EV_TEXT-shaped data/text blocks that
 * each box-kind path emits.  Goal: every line emitted from this file uses
 * the same `%-24s%-16s %s\n` shape as SM-side; no tab-indented stragglers.
 *
 * Shape:  LABEL(24) / OPCODE(16) / ARGS+COMMENT(free).
 *
 * flat3c_*       — three-column line emitters routed through the emitter_v's
 *                  fprintf_raw to keep TEXT-mode shape uniform.
 * flat_data_*    — convenience wrappers for `.section .data` directives.
 * flat_text_*    — convenience wrappers for `.section .text` / `.intel_syntax`.
 * flat_box_call  — emits the four-line (lea / mov esi / call / test) sequence.
 *
 * All helpers are no-ops in BINARY mode (e->is_text == 0).  Callers gate on
 * `e->is_text` already; the helpers stay safe for callers that don't.
 * ──────────────────────────────────────────────────────────────────────── */

static void flat3c(emitter_v *e, const char *lbl, const char *act, const char *got)
{
    if (!e->is_text) return;
    /* EM-FORMAT-BB lone-label fusion (2026-05-09): route through
     * bb3c_format so the pending-label buffer covers data-section labels
     * (e.g. .Lcap1_data:, .Llen2_z:) and child-entry labels (cap1_child_α:)
     * that previously emitted standalone label-only lines. */
    FILE *f = emitter_text_file(e);
    if (!f) return;
    bb3c_format(f, lbl ? lbl : "", act ? act : "", got ? got : "");
}

static void flat3c_action(emitter_v *e, const char *act, const char *args)
{
    flat3c(e, "", act, args ? args : "");
}

/* Forward decls for data-buffer helpers used by flat3c_label below; full
 * definitions sit just after the label helper. */
static void data_buf_remember_label(const char *name);

static void flat3c_label(emitter_v *e, const char *name)
{
    if (!e->is_text) return;
    /* EM-FORMAT-BB-DATA-CONSOLIDATE: while a deferred-data block is active,
     * label definitions belong with the data, not the main text stream.
     * Defer the label so it fuses with the next data directive (`.string`,
     * `.quad`, `.long`, `.zero`).  Also remember the bare name so we can
     * emit `# data: <labels>` at the original site when the block closes. */
    if (g_flat_data_active) {
        data_buf_pend_label(name);
        data_buf_remember_label(name);
        return;
    }
    char buf[160]; snprintf(buf, sizeof(buf), "%s:", name);
    flat3c(e, buf, "", "");
}

/* EM-FORMAT-BB-DATA-CONSOLIDATE: helper to record a label-name into the
 * current data block's label list (for the inline `# data: ...` comment). */
static void data_buf_remember_label(const char *name)
{
    if (g_flat_data_block_nlbls >= FLAT_DATA_LBL_MAX) return;
    snprintf(g_flat_data_block_lbls[g_flat_data_block_nlbls],
             sizeof(g_flat_data_block_lbls[0]), "%s", name ? name : "");
    g_flat_data_block_nlbls++;
}

static void data_buf_emit_block_comment(emitter_v *e)
{
    (void)e;
    g_flat_data_block_nlbls = 0;
}

static void flat_data_section(emitter_v *e)
{
    if (!e->is_text) return;
    /* Begin a new deferred-data block.  The main text stream stays in `.text`;
     * everything until the next flat_text_section() is buffered. */
    g_flat_data_active = 1;
    g_flat_data_any    = 1;
    g_flat_data_block_nlbls = 0;
}

static void flat_text_section(emitter_v *e)
{
    if (!e->is_text) return;
    if (g_flat_data_active) {
        /* End of a buffered data block: emit the `# data: ...` comment to the
         * main stream and toggle active off.  We never actually left `.text`
         * on the main stream, so no real `.section .text` needed here.
         * Set just-closed so the box-emitted trailing `.intel_syntax noprefix`
         * (which always pairs with `.section .text`) also gets suppressed. */
        data_buf_emit_block_comment(e);
        g_flat_data_active = 0;
        g_flat_data_just_closed = 1;
        return;
    }
    /* Not buffering: legacy behaviour (rare; only for callers outside the
     * pat_inv blob pipeline that may toggle sections directly). */
    flat3c(e, "", ".section", ".text");
}

static void flat_intel_syntax(emitter_v *e)
{
    if (!e->is_text) return;
    /* While buffering data, the main stream stayed in `.intel_syntax noprefix`
     * (every blob's preamble already set it).  Suppress the redundant
     * directive that would normally close a data block — both the in-block
     * call (active==1) and the trailing one right after flat_text_section
     * closed the block (just_closed==1). */
    if (g_flat_data_active) return;
    if (g_flat_data_just_closed) {
        g_flat_data_just_closed = 0;
        return;
    }
    flat3c(e, "", ".intel_syntax", "noprefix");
}

static void flat_data_string(emitter_v *e, const char *s)
{
    if (!e->is_text) return;
    /* Build the escaped quoted form once.  We escape only " and \, and
     * non-printables as \NNN — matches the assembler's expectation and
     * mirrors flat_emit_charset_call's per-byte loop. */
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
    else                    flat3c(e, "", ".string", esc);
}

static void flat_data_quad(emitter_v *e, const char *arg)
{
    if (!e->is_text) return;
    if (g_flat_data_active) data_buf_three_col("", ".quad", arg ? arg : "0");
    else                    flat3c(e, "", ".quad", arg ? arg : "0");
}

static void flat_data_quad_int(emitter_v *e, long long v)
{
    if (!e->is_text) return;
    char buf[32]; snprintf(buf, sizeof(buf), "%lld", v);
    if (g_flat_data_active) data_buf_three_col("", ".quad", buf);
    else                    flat3c(e, "", ".quad", buf);
}

static void flat_data_long(emitter_v *e, long long v)
{
    if (!e->is_text) return;
    char buf[32]; snprintf(buf, sizeof(buf), "%lld", v);
    if (g_flat_data_active) data_buf_three_col("", ".long", buf);
    else                    flat3c(e, "", ".long", buf);
}

static void flat_data_zero(emitter_v *e, int n)
{
    if (!e->is_text) return;
    char buf[16]; snprintf(buf, sizeof(buf), "%d", n);
    if (g_flat_data_active) data_buf_three_col("", ".zero", buf);
    else                    flat3c(e, "", ".zero", buf);
}

static void flat_globl(emitter_v *e, const char *name)
{
    if (!e->is_text) return;
    flat3c(e, "", ".globl", name);
}

/* Emit the three-line box-call sequence: `lea rdi,[rip+ζ]` / `mov esi,mode` /
 * `call fn@PLT`.  The trailing `test rax, rax` was previously here but is
 * now folded into `flat_box_dispatch_jne_jmp` below so the test concats
 * onto one line with the cond+uncond jmps that follow (per EM-FORMAT-BB-LAW
 * "no jmp instruction with only another jmp instruction on that line"). */
static void flat_box_call(emitter_v *e, const char *rdi_load,
                          const char *fn, int mode)
{
    if (!e->is_text) return;
    flat3c_action(e, "lea", rdi_load);
    char esi_arg[32]; snprintf(esi_arg, sizeof(esi_arg), "esi, %d", mode);
    flat3c_action(e, "mov", esi_arg);
    char call_arg[64]; snprintf(call_arg, sizeof(call_arg), "%s@PLT", fn);
    flat3c_action(e, "call", call_arg);
}

/* Variant: arbno's box call uses a slot pointer dereference rather than
 * lea+rip.  Same three-line shape, different first instruction. */
static void flat_box_call_slot(emitter_v *e, const char *slot_lbl,
                               const char *fn, int mode)
{
    if (!e->is_text) return;
    char rdi_arg[160]; snprintf(rdi_arg, sizeof(rdi_arg),
                                "rdi, qword ptr [rip + %s]", slot_lbl);
    flat3c_action(e, "mov", rdi_arg);
    char esi_arg[32]; snprintf(esi_arg, sizeof(esi_arg), "esi, %d", mode);
    flat3c_action(e, "mov", esi_arg);
    char call_arg[64]; snprintf(call_arg, sizeof(call_arg), "%s@PLT", fn);
    flat3c_action(e, "call", call_arg);
}

/* EM-FORMAT-BB-LAW (TRIPLE-FUSION): emit
 *   test  rax, rax;<pad>jne <succ>; jmp <fail>
 * as ONE line.  Layout:
 *   col-2 = "test"
 *   col-3 = "rax, rax;" + spaces to width 27
 *   col-4 = "jne <succ>; jmp <fail>"
 * Replaces the prior 3-line emission
 *   ev_test_rax_rax(e);
 *   EV_JMP(e, lbl_succ, JMP_JNE);
 *   EV_JMP(e, lbl_fail, JMP_JMP);
 * which violated the LAW.  TEXT mode only. */
static void flat_box_dispatch_jne_jmp(emitter_v *e,
                                      bb_label_t *lbl_succ,
                                      bb_label_t *lbl_fail)
{
    if (!e->is_text) return;
    char buf[512];
    int o = 0;
    o += snprintf(buf + o, sizeof(buf) - o, "rax, rax;");
    while (o < 27 && o < (int)sizeof(buf) - 1) buf[o++] = ' ';
    buf[o] = '\0';
    snprintf(buf + o, sizeof(buf) - o, "jne %s; jmp %s",
             lbl_succ ? lbl_succ->name : "?",
             lbl_fail ? lbl_fail->name : "?");
    flat3c_action(e, "test", buf);
}

/* EM-FORMAT-BB-LAW (TRIPLE-FUSION): emit the entry dispatch
 *   cmp  esi, 0;<pad>je <α_body>; jmp <β>
 * as ONE line.  Used at the top of every BB child sub-proc (capture
 * children, arbno children) where mode 0 = α-entry, mode 1 = β-retry.
 * TEXT mode only. */
static void flat_box_entry_dispatch(emitter_v *e,
                                    bb_label_t *lbl_alpha_body,
                                    bb_label_t *lbl_beta)
{
    if (!e->is_text) return;
    char buf[512];
    int o = 0;
    o += snprintf(buf + o, sizeof(buf) - o, "esi, 0;");
    while (o < 27 && o < (int)sizeof(buf) - 1) buf[o++] = ' ';
    buf[o] = '\0';
    snprintf(buf + o, sizeof(buf) - o, "je %s; jmp %s",
             lbl_alpha_body ? lbl_alpha_body->name : "?",
             lbl_beta       ? lbl_beta->name       : "?");
    flat3c_action(e, "cmp", buf);
}

/* ── EM-FORMAT-BB-BOX-BANNERS helpers (2026-05-09) ─────────────────────────
 *
 * Per the EM-FORMAT-BB rung spec:
 *   - Pattern-blob banner: 120-char `#=` rule + `# pattern <prefix>: <src>`
 *     + 120-char `#=` rule, where <src> is a SNOBOL4-source-style render of
 *     the PATND_t tree.  Emitted once per pat_inv_<id> blob.
 *   - Per-box banner: 120-char `#-` rule + `# BOX <kind>(<args>)  [<prefix>]`,
 *     emitted before each box's α-arm code.
 *
 * Banner lines start with `#` so the audit harness's audit_is_banner short-
 * circuit applies (no I1/I2/I3 violations).  Lines are non-blank, so I0 is
 * also clean.
 *
 * Banners flush any pending bb3c label before writing so a fused label that
 * was buffered from a prior box doesn't appear after this banner.  (The
 * pattern banner runs at function-prologue time before any boxes have
 * written; the per-box banner can follow another box's tail label.)
 *
 * 120-char rule:  '# ' + 118 separator chars = 120 total.
 * ──────────────────────────────────────────────────────────────────────── */

/* xkind_name lives in snobol4_pattern.c as static; we need our own local
 * copy for banner reconstruction.  Names match the diagnostic spelling
 * used by patnd_print's --dump-bb output. */
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

/* Append at most `cap-1` chars from `s` (escaping " and non-printables as `.`)
 * into buf at offset *o, leaving room for the NUL.  Returns 1 on overflow. */
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

/* Render PATND_t tree in SNOBOL4-source style, single-line, into buf.
 * Conventions:
 *   XCHR("foo")             → 'foo'
 *   XSPNC("xyz")            → SPAN('xyz')
 *   XBRKC("xyz")            → BREAK('xyz')
 *   XANYC("xyz")            → ANY('xyz')
 *   XNNYC("xyz")            → NOTANY('xyz')
 *   XBRKX("xyz")            → BREAKX('xyz')
 *   XLNTH(n) / XTB(n) /...  → LEN(n) / TAB(n) / RTAB(n) / POS(n) / RPOS(n)
 *   XFARB / XEPS / XFAIL    → ARB / EPSILON / FAIL
 *   XSTAR / XBAL / XSUCF    → REM / BAL / SUCCEED
 *   XCAT(a,b,c)             → a b c
 *   XOR(a,b,c)              → a | b | c
 *   XARBN(p)                → ARBNO(p)
 *   XFNCE(p)                → FENCE(p)        (or just FENCE if no children)
 *   XFNME(p)/XNME(p)        → p $ var / p . var
 *   XDSAR("name")           → *name
 *   XATP("fn", args...)     → fn(args...)
 *   XVAR                    → <var>           (concrete name not in node)
 *
 * Caps recursion depth at 16 to keep the banner sane on deep trees;
 * deeper nesting renders as "...".
 */
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

/* 120-char banner rule.  Rule chars: `#` then 119 separator
 * chars — total 120 visible columns.  No space between `#` and rule. */
#define BB_BANNER_RULE_LEN 119

/* EM-FORMAT-BB-PORT-COMPLETION-LONE-LABEL-FIX (sess 2026-05-09):
 * Banner emission must NOT flush the bb3c pending-label buffer.  A pending
 * label belongs to the address that the NEXT content line will define;
 * the banner is comments that physically precede that label.  Flushing
 * here writes the label as a standalone lone-label line BEFORE the banner,
 * regressing the EM-FORMAT-BB-LONE-LABELS rung.  Pending label simply
 * sits in the buffer through banner emission (which goes directly to the
 * FILE* via fprintf_raw); the next flat3c_action call consumes it via
 * bb3c_format's empty-col-1 fusion path.  Result: banner first, then
 * `<label>:    <first content line>` on a single fused line.
 */
static void flat_emit_banner_rule(emitter_v *e, char ch)
{
    if (!e->is_text) return;
    char buf[BB_BANNER_RULE_LEN + 4];
    buf[0] = '#';
    for (int i = 0; i < BB_BANNER_RULE_LEN; i++) buf[1 + i] = ch;
    buf[1 + BB_BANNER_RULE_LEN] = '\0';
    EV_TEXT(e, "%s\n", buf);
}

/* Pattern-blob banner: emit at the top of each pat_inv_<id> blob.
 *
 *   #=====================================================================
 *   # pattern <prefix>: <reconstructed source>
 *   #=====================================================================
 */
static void flat_emit_pat_banner(emitter_v *e, const char *prefix, PATND_t *p)
{
    if (!e->is_text) return;
    (void)prefix; (void)p;
    flat_emit_banner_rule(e, '=');
}

/* Per-box banner: emit before the α-arm of a box.
 *
 *   #---------------------------------------------------------------------
 *   # BOX <kind>(<args>)  [<label-prefix>]
 *
 * The trailing `#-` rule of the prior banner serves as the visual
 * separator from any preceding code; we emit a `#-` rule above every
 * box banner for symmetry, no rule below (the box's α label-line
 * follows immediately).
 */
static void flat_emit_box_banner(emitter_v *e, const char *kind,
                                 const char *args, const char *label_prefix)
{
    if (!e->is_text) return;
    flat_emit_banner_rule(e, '-');
    if (args && *args) {
        EV_TEXT(e, "#                       BOX %s(%s)  [%s]\n", kind, args,
                label_prefix ? label_prefix : "");
    } else {
        EV_TEXT(e, "#                       BOX %s  [%s]\n", kind,
                label_prefix ? label_prefix : "");
    }
}

/* ── forward declarations ────────────────────────────────────────────────── */
static void flat_emit_node(emitter_v *e, PATND_t *p,
                           bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                           bb_label_t *lbl_β);

/* ── XCAT ───────────────────────────────────────────────────────────────── */
static void flat_emit_xcat(emitter_v *e, PATND_t *p,
                           bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                           bb_label_t *lbl_β)
{
    int id = g_flat_node_id++;
    bb_label_t mid_γ, right_ω, left_β, right_β, xcat_ω;
    bb_label_initf(&mid_γ,   "xcat%d_\xCE\xB3",         id);  /* xcat%d_γ */
    bb_label_initf(&right_ω, "xcat%d_right_\xCF\x89",   id);  /* xcat%d_right_ω */
    bb_label_initf(&left_β,  "xcat%d_left_\xCE\xB2",    id);  /* xcat%d_left_β */
    bb_label_initf(&right_β, "xcat%d_right_\xCE\xB2",   id);  /* xcat%d_right_β */
    bb_label_initf(&xcat_ω,  "xcat%d_\xCF\x89",         id);  /* xcat%d_ω */

    if (p->nchildren == 0) {
        EV_JMP(e, lbl_succ, JMP_JMP);
        EV_LABEL(e, lbl_β); EV_JMP(e, lbl_fail, JMP_JMP);
        EV_LABEL(e, &xcat_ω); EV_JMP(e, lbl_fail, JMP_JMP);
        EV_LABEL(e, &mid_γ); EV_LABEL(e, &right_ω);
        EV_LABEL(e, &right_β); EV_LABEL(e, &left_β);
        return;
    }
    if (p->nchildren == 1) {
        flat_emit_node(e, p->children[0], lbl_succ, lbl_fail, &left_β);
        EV_LABEL(e, lbl_β); EV_JMP(e, &left_β, JMP_JMP);
        EV_LABEL(e, &xcat_ω); EV_JMP(e, lbl_fail, JMP_JMP);
        EV_LABEL(e, &mid_γ); EV_LABEL(e, &right_ω); EV_LABEL(e, &right_β);
        return;
    }

    flat_emit_node(e, p->children[0], &mid_γ, &xcat_ω, &left_β);
    EV_LABEL(e, &mid_γ);

    if (p->nchildren == 2) {
        flat_emit_node(e, p->children[1], lbl_succ, &right_ω, &right_β);
    } else {
        int nc = p->nchildren;
        bb_label_t *mids  = alloca(sizeof(bb_label_t) * (nc - 1));
        bb_label_t *betas = alloca(sizeof(bb_label_t) * (nc - 1));
        for (int i = 0; i < nc - 1; i++) {
            bb_label_initf(&mids[i],  "xcat%d_mid%d_\xCE\xB3", id, i+1);  /* xcat%d_mid%d_γ */
            bb_label_initf(&betas[i], "xcat%d_mid%d_\xCE\xB2", id, i+1);  /* xcat%d_mid%d_β */
        }
        for (int i = 1; i < nc; i++) {
            bb_label_t *s = (i < nc-1) ? &mids[i-1] : lbl_succ;
            flat_emit_node(e, p->children[i], s, &right_ω, &betas[i-1]);
            if (i < nc-1) EV_LABEL(e, &mids[i-1]);
        }
    }
    EV_LABEL(e, &right_ω); EV_JMP(e, &left_β, JMP_JMP);
    EV_LABEL(e, lbl_β); EV_JMP(e, &right_β, JMP_JMP);
    EV_LABEL(e, &xcat_ω);  EV_JMP(e, lbl_fail, JMP_JMP);
}

/* ── XOR (alternation) ──────────────────────────────────────────────────── */
static void flat_emit_alt(emitter_v *e, PATND_t *p,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                          bb_label_t *lbl_β)
{
    int id = g_flat_node_id++;
    int nc = p->nchildren;
    if (nc == 0) { EV_LABEL(e, lbl_β); EV_JMP(e, lbl_fail, JMP_JMP); return; }
    if (nc == 1) { flat_emit_node(e, p->children[0], lbl_succ, lbl_fail, lbl_β); return; }

    bb_label_t *ci_βs = alloca((size_t)nc * sizeof(bb_label_t));
    bb_label_t *ci_ωs = alloca((size_t)nc * sizeof(bb_label_t));
    for (int i = 0; i < nc; i++) {
        bb_label_initf(&ci_βs[i], "alt%d_c%d_\xCE\xB2", id, i);   /* alt%d_c%d_β */
        bb_label_initf(&ci_ωs[i], "alt%d_c%d_\xCF\x89", id, i);   /* alt%d_c%d_ω */
    }
    for (int i = 0; i < nc; i++) {
        bb_label_t *f = (i < nc-1) ? &ci_ωs[i] : &ci_ωs[nc-1];
        flat_emit_node(e, p->children[i], lbl_succ, f, &ci_βs[i]);
        if (i < nc-1) EV_LABEL(e, &ci_ωs[i]);
        else          EV_LABEL(e, &ci_ωs[nc-1]);
    }
    EV_JMP(e, lbl_fail, JMP_JMP);
    EV_LABEL(e, lbl_β); EV_JMP(e, &ci_βs[0], JMP_JMP);
}

/* ── leaf: literal string ───────────────────────────────────────────────── */
static void flat_emit_lit(emitter_v *e, const char *lit, int len,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                          bb_label_t *lbl_β)
{
    /* α: if Δ + len > Σlen → fail */
    ev_load_delta(e);                                    /* eax = Δ */
    ev_add_eax_imm32(e, (uint32_t)len);                 /* eax += len */
    ev_cmp_eax_siglen(e, ADDR_SIGLEN);                  /* cmp eax, [Σlen] */
    EV_JMP(e, lbl_fail, JMP_JG);

    /* memcmp(Σ+Δ, lit, len): set up rdi=Σ+Δ, rsi=lit, rdx=len */
    ev_sigma_plus_delta(e, ADDR_SIGMA);                 /* rax = Σ+Δ */
    ev_mov_rdi_rax(e);                                  /* rdi = Σ+Δ */
    ev_mov_rdx_imm64(e, (uint64_t)(uint32_t)len);       /* rdx = len */

    /* rsi = lit ptr: TEXT mode → use strtab label; BINARY → raw ptr */
    if (e->is_text && e->intern_str) {
        const char *lbl = e->intern_str(e, lit);        /* e.g. ".Lstr_N" */
        bb_insn_desc_t d = {BB_INSN_LEA_RCX_SYM, (uint64_t)(uintptr_t)lit, 0, 0, lbl};
        e->emit_insn(e, &d);                            /* lea rcx, [rip + .Lstr_N] */
        /* mov rsi, rcx — route through rcx since we have no LEA_RSI_SYM */
        { bb_insn_desc_t d2 = {BB_INSN_MOV_RSI_IMM64, 0, 0, 0, NULL};
          /* TEXT: emit "mov rsi, rcx" — but we lack MOV_RSI_RCX insn.
           * Use fprintf_raw for this one-off: */
          e->fprintf_raw(e, "    mov     rsi, rcx\n");
        }
    } else {
        /* BINARY / no-strtab: bake raw pointer (in-process mode-3 valid) */
        bb_insn_desc_t d = {BB_INSN_MOV_RSI_IMM64, (uint64_t)(uintptr_t)lit, 0, 0, NULL};
        e->emit_insn(e, &d);
    }

    /* call memcmp — TEXT: call memcmp@PLT; BINARY: mov rax, ptr; call rax */
    ev_call_sym_plt(e, "memcmp", (uint64_t)(uintptr_t)memcmp);
    ev_test_eax_eax(e);                                 /* test eax, eax */
    EV_JMP(e, lbl_fail, JMP_JNE);

    /* success: Δ += len */
    ev_add_delta_imm(e, len);
    EV_JMP(e, lbl_succ, JMP_JMP);

    /* β: Δ -= len; fail */
    EV_LABEL(e, lbl_β);
    ev_sub_delta_imm(e, len);
    EV_JMP(e, lbl_fail, JMP_JMP);
}

/* ── leaf: epsilon ──────────────────────────────────────────────────────── */
static void flat_emit_eps(emitter_v *e, bb_label_t *lbl_succ,
                          bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    if (e->is_text) {
        flat_emit_box_banner(e, "EPS", NULL, lbl_succ->name);
        /* EM-7c-bb-macros: single macro call per port.
         * EM-FORMAT-BB-COL3-COMMENTS: α-line names the box kind. */
        char args[256];
        snprintf(args, sizeof(args), "%s # EPS", lbl_succ->name);
        flat3c_action(e, "EPS_\xCE\xB1", args);             /* EPS_α lbl_succ # EPS */
        EV_LABEL(e, lbl_β);
        flat3c_action(e, "EPS_\xCE\xB2", lbl_fail->name);  /* EPS_β lbl_fail */
    } else {
        EV_JMP(e, lbl_succ, JMP_JMP);
        EV_LABEL(e, lbl_β); EV_JMP(e, lbl_fail, JMP_JMP);
    }
}

/* ── leaf: always-fail ──────────────────────────────────────────────────── */
static void flat_emit_fail(emitter_v *e, bb_label_t *lbl_succ,
                           bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)lbl_succ;
    if (e->is_text) {
        flat_emit_box_banner(e, "FAIL", NULL, lbl_fail->name);
        /* EM-FORMAT-BB-COL3-COMMENTS: α-line names the box kind. */
        char args[256];
        snprintf(args, sizeof(args), "%s # FAIL", lbl_fail->name);
        flat3c_action(e, "FAIL_\xCE\xB1", args);            /* FAIL_α lbl_fail # FAIL */
        EV_LABEL(e, lbl_β);
        flat3c_action(e, "FAIL_\xCE\xB2", lbl_fail->name);  /* FAIL_β lbl_fail */
    } else {
        EV_JMP(e, lbl_fail, JMP_JMP);
        EV_LABEL(e, lbl_β); EV_JMP(e, lbl_fail, JMP_JMP);
    }
}

/* ── leaf: POS(n) ───────────────────────────────────────────────────────── */
static void flat_emit_pos(emitter_v *e, int n, bb_label_t *lbl_succ,
                          bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    if (e->is_text) {
        char banner_args[32]; snprintf(banner_args, sizeof(banner_args), "%d", n);
        flat_emit_box_banner(e, "POS", banner_args, lbl_succ->name);
        /* EM-7c-bb-macros: POS_α n, lbl_succ, lbl_fail
         * EM-FORMAT-BB-COL3-COMMENTS: α-line names the box kind + arg. */
        char args[256];
        snprintf(args, sizeof(args), "%d, %s, %s # POS(%d)",
                 n, lbl_succ->name, lbl_fail->name, n);
        flat3c_action(e, "POS_\xCE\xB1", args);  /* POS_α */
        EV_LABEL(e, lbl_β);
        flat3c_action(e, "POS_\xCE\xB2", lbl_fail->name);  /* POS_β */
    } else {
        ev_load_delta(e);
        ev_cmp_eax_imm32(e, (uint32_t)n);
        EV_JMP(e, lbl_fail, JMP_JNE);
        EV_JMP(e, lbl_succ, JMP_JMP);
        EV_LABEL(e, lbl_β); EV_JMP(e, lbl_fail, JMP_JMP);
    }
}

/* ── leaf: RPOS(n) ──────────────────────────────────────────────────────── */
static void flat_emit_rpos(emitter_v *e, int n, bb_label_t *lbl_succ,
                           bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    if (e->is_text) {
        char banner_args[32]; snprintf(banner_args, sizeof(banner_args), "%d", n);
        flat_emit_box_banner(e, "RPOS", banner_args, lbl_succ->name);
        /* EM-7c-bb-macros: RPOS_α n, lbl_succ, lbl_fail
         * EM-FORMAT-BB-COL3-COMMENTS: α-line names the box kind + arg. */
        char args[256];
        snprintf(args, sizeof(args), "%d, %s, %s # RPOS(%d)",
                 n, lbl_succ->name, lbl_fail->name, n);
        flat3c_action(e, "RPOS_\xCE\xB1", args);  /* RPOS_α */
        EV_LABEL(e, lbl_β);
        flat3c_action(e, "RPOS_\xCE\xB2", lbl_fail->name);  /* RPOS_β */
    } else {
        ev_load_siglen(e, ADDR_SIGLEN);     /* eax = Σlen */
        ev_sub_eax_imm32(e, (uint32_t)n);  /* eax = Σlen - n */
        ev_mov_ecx_eax(e);                 /* ecx = Σlen - n */
        ev_load_delta(e);                  /* eax = Δ */
        ev_cmp_eax_ecx(e);                 /* cmp Δ, Σlen-n */
        EV_JMP(e, lbl_fail, JMP_JNE);
        EV_JMP(e, lbl_succ, JMP_JMP);
        EV_LABEL(e, lbl_β); EV_JMP(e, lbl_fail, JMP_JMP);
    }
}

/* ── leaf: charset (ANY/NOTANY/SPAN/BRK) ───────────────────────────────── */
static void flat_emit_charset_call(emitter_v *e, bb_box_fn c_fn,
                                   const char *c_fn_name,
                                   const char *chars,
                                   bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                                   bb_label_t *lbl_β)
{
    if (e->is_text) {
        /* Map runtime fn name to source-level kind for the banner.
         * bb_span/any/brk/notany → SPAN/ANY/BREAK/NOTANY. */
        const char *kind = "CHARSET";
        if      (c_fn_name && !strcmp(c_fn_name, "bb_span"))   kind = "SPAN";
        else if (c_fn_name && !strcmp(c_fn_name, "bb_any"))    kind = "ANY";
        else if (c_fn_name && !strcmp(c_fn_name, "bb_brk"))    kind = "BREAK";
        else if (c_fn_name && !strcmp(c_fn_name, "bb_notany")) kind = "NOTANY";
        /* Truncate chars-arg preview at 24 chars; some patterns use long
         * charsets that would otherwise blow past the 120-char banner. */
        char preview[40];
        if (chars && *chars) {
            int n = (int)strlen(chars);
            if (n > 24) snprintf(preview, sizeof(preview), "'%.24s...'", chars);
            else        snprintf(preview, sizeof(preview), "'%s'", chars);
        } else {
            preview[0] = '\0';
        }
        flat_emit_box_banner(e, kind, preview, lbl_succ->name);
        /* Static .data: chars string + cs_t {chars*, delta=0} */
        int id = g_flat_node_id++;
        char zlbl[64], slbl[64];
        snprintf(zlbl, sizeof(zlbl), ".Lcs%d_z",     id);
        snprintf(slbl, sizeof(slbl), ".Lcs%d_chars", id);
        flat_data_section(e);
        flat3c_label(e, slbl);
        flat_data_string(e, chars);
        flat3c_label(e, zlbl);
        flat_data_quad(e, slbl);                  /* &chars */
        flat_data_long(e, 0);                     /* delta */
        flat_data_long(e, 0);                     /* padding */
        flat_text_section(e);
        flat_intel_syntax(e);
        char rdi_arg[96]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", zlbl);
        flat3c_action(e, "lea", rdi_arg);
        flat3c_action(e, "mov", "esi, 0");
        ev_call_sym_plt(e, c_fn_name, (uint64_t)(uintptr_t)c_fn);
        flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        EV_LABEL(e, lbl_β);
        flat3c_action(e, "lea", rdi_arg);
        flat3c_action(e, "mov", "esi, 1");
        ev_call_sym_plt(e, c_fn_name, (uint64_t)(uintptr_t)c_fn);
        flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
    } else {
        /* Binary path: heap cs_t */
        typedef struct { const char *chars; int delta; } cs_t;
        cs_t *z = calloc(1, sizeof(cs_t));
        z->chars = chars;
        ev_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
        ev_mov_esi_imm32(e, 0);
        ev_call_sym_plt(e, c_fn_name, (uint64_t)(uintptr_t)c_fn);
        ev_test_rax_rax(e);
        EV_JMP(e, lbl_succ, JMP_JNE);
        EV_JMP(e, lbl_fail, JMP_JMP);
        EV_LABEL(e, lbl_β);
        ev_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
        ev_mov_esi_imm32(e, 1);
        ev_call_sym_plt(e, c_fn_name, (uint64_t)(uintptr_t)c_fn);
        ev_test_rax_rax(e);
        EV_JMP(e, lbl_succ, JMP_JNE);
        EV_JMP(e, lbl_fail, JMP_JMP);
    }
}

/* ── flat_emit_node dispatch ─────────────────────────────────────────────── */

extern DESCR_t bb_span  (void *zeta, int entry);
extern DESCR_t bb_any   (void *zeta, int entry);
extern DESCR_t bb_brk   (void *zeta, int entry);
extern DESCR_t bb_notany(void *zeta, int entry);
extern DESCR_t bb_len   (void *zeta, int entry);
extern DESCR_t bb_tab   (void *zeta, int entry);
extern DESCR_t bb_rtab  (void *zeta, int entry);
extern DESCR_t bb_fence (void *zeta, int entry);
extern DESCR_t bb_arb   (void *zeta, int entry);
extern DESCR_t bb_arbno (void *zeta, int entry);
extern DESCR_t bb_breakx(void *zeta, int entry);
extern DESCR_t bb_rem   (void *zeta, int entry);
extern DESCR_t bb_cap   (void *zeta, int entry);
extern DESCR_t bb_atp   (void *zeta, int entry);
extern DESCR_t bb_deferred_var_exported(void *zeta, int entry);
extern int memcmp(const void *, const void *, size_t);

/* Generic two-call emitter: α calls fn(ζ,0), β calls fn(ζ,1), result nonzero=success */
static void flat_emit_box_call(emitter_v *e, bb_box_fn fn, const char *fn_name,
                               void *z,
                               bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                               bb_label_t *lbl_β)
{
    ev_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    ev_mov_esi_imm32(e, 0);
    ev_call_sym_plt(e, fn_name, (uint64_t)(uintptr_t)fn);
    if (e->is_text) {
        flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
    } else {
        ev_test_rax_rax(e);
        EV_JMP(e, lbl_succ, JMP_JNE);
        EV_JMP(e, lbl_fail, JMP_JMP);
    }
    EV_LABEL(e, lbl_β);
    ev_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    ev_mov_esi_imm32(e, 1);
    ev_call_sym_plt(e, fn_name, (uint64_t)(uintptr_t)fn);
    if (e->is_text) {
        flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
    } else {
        ev_test_rax_rax(e);
        EV_JMP(e, lbl_succ, JMP_JNE);
        EV_JMP(e, lbl_fail, JMP_JMP);
    }
}

static void flat_emit_node(emitter_v *e, PATND_t *p,
                           bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                           bb_label_t *lbl_β)
{
    if (!p) { flat_emit_eps(e, lbl_succ, lbl_fail, lbl_β); return; }
    switch (p->kind) {
    case XCHR: {
        const char *lit = p->STRVAL_fn ? p->STRVAL_fn : "";
        if (e->is_text) {
            int n = (int)strlen(lit);
            char preview[40];
            if (n > 24) snprintf(preview, sizeof(preview), "'%.24s...'", lit);
            else        snprintf(preview, sizeof(preview), "'%s'", lit);
            flat_emit_box_banner(e, "LIT", preview, lbl_succ->name);
        }
        flat_emit_lit(e, lit, (int)strlen(lit), lbl_succ, lbl_fail, lbl_β);
        break;
    }
    case XEPS:  flat_emit_eps (e, lbl_succ, lbl_fail, lbl_β); break;
    case XFAIL: flat_emit_fail(e, lbl_succ, lbl_fail, lbl_β); break;
    case XPOSI: flat_emit_pos (e, (int)p->num, lbl_succ, lbl_fail, lbl_β); break;
    case XRPSI: flat_emit_rpos(e, (int)p->num, lbl_succ, lbl_fail, lbl_β); break;
    case XCAT:  flat_emit_xcat(e, p, lbl_succ, lbl_fail, lbl_β); break;
    case XOR:   flat_emit_alt (e, p, lbl_succ, lbl_fail, lbl_β); break;
    case XSPNC: flat_emit_charset_call(e, bb_span,   "bb_span",    p->STRVAL_fn?p->STRVAL_fn:"", lbl_succ, lbl_fail, lbl_β); break;
    case XANYC: flat_emit_charset_call(e, bb_any,    "bb_any",     p->STRVAL_fn?p->STRVAL_fn:"", lbl_succ, lbl_fail, lbl_β); break;
    case XBRKC: flat_emit_charset_call(e, bb_brk,    "bb_brk",     p->STRVAL_fn?p->STRVAL_fn:"", lbl_succ, lbl_fail, lbl_β); break;
    case XNNYC: flat_emit_charset_call(e, bb_notany, "bb_notany",  p->STRVAL_fn?p->STRVAL_fn:"", lbl_succ, lbl_fail, lbl_β); break;
    case XLNTH: {
        if (e->is_text) {
            char banner_args[32]; snprintf(banner_args, sizeof(banner_args), "%lld", (long long)p->num);
            flat_emit_box_banner(e, "LEN", banner_args, lbl_succ->name);
            int id = g_flat_node_id++;
            char lbl[64]; snprintf(lbl, sizeof(lbl), ".Llen%d_z", id);
            flat_data_section(e);
            flat3c_label(e, lbl);
            flat_data_long(e, (long long)p->num);
            flat_text_section(e);
            flat_intel_syntax(e);
            char rdi_arg[96]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", lbl);
            flat_box_call(e, rdi_arg, "bb_len", 0);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
            EV_LABEL(e, lbl_β);
            flat_box_call(e, rdi_arg, "bb_len", 1);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        } else {
            len_t *z = bb_len_new((int)p->num);
            flat_emit_box_call(e, bb_len, "bb_len", z, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    case XTB: {
        if (e->is_text) {
            char banner_args[32]; snprintf(banner_args, sizeof(banner_args), "%lld", (long long)p->num);
            flat_emit_box_banner(e, "TAB", banner_args, lbl_succ->name);
            int id = g_flat_node_id++;
            char lbl[64]; snprintf(lbl, sizeof(lbl), ".Ltab%d_z", id);
            flat_data_section(e);
            flat3c_label(e, lbl);
            flat_data_long(e, (long long)p->num);
            flat_data_long(e, 0);
            flat_text_section(e);
            flat_intel_syntax(e);
            char rdi_arg[96]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", lbl);
            flat_box_call(e, rdi_arg, "bb_tab", 0);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
            EV_LABEL(e, lbl_β);
            flat_box_call(e, rdi_arg, "bb_tab", 1);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        } else {
            tab_t *z = bb_tab_new((int)p->num);
            flat_emit_box_call(e, bb_tab, "bb_tab", z, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    case XRTB: {
        if (e->is_text) {
            char banner_args[32]; snprintf(banner_args, sizeof(banner_args), "%lld", (long long)p->num);
            flat_emit_box_banner(e, "RTAB", banner_args, lbl_succ->name);
            int id = g_flat_node_id++;
            char lbl[64]; snprintf(lbl, sizeof(lbl), ".Lrtab%d_z", id);
            flat_data_section(e);
            flat3c_label(e, lbl);
            flat_data_long(e, (long long)p->num);
            flat_data_long(e, 0);
            flat_text_section(e);
            flat_intel_syntax(e);
            char rdi_arg[96]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", lbl);
            flat_box_call(e, rdi_arg, "bb_rtab", 0);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
            EV_LABEL(e, lbl_β);
            flat_box_call(e, rdi_arg, "bb_rtab", 1);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        } else {
            rtab_t *z = bb_rtab_new((int)p->num);
            flat_emit_box_call(e, bb_rtab, "bb_rtab", z, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    case XFNCE: {
        if (e->is_text) {
            flat_emit_box_banner(e, "FENCE", NULL, lbl_succ->name);
            int id = g_flat_node_id++;
            char lbl[64]; snprintf(lbl, sizeof(lbl), ".Lfence%d_z", id);
            flat_data_section(e);
            flat3c_label(e, lbl);
            flat_data_long(e, 0);
            flat_text_section(e);
            flat_intel_syntax(e);
            char rdi_arg[96]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", lbl);
            flat_box_call(e, rdi_arg, "bb_fence", 0);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
            EV_LABEL(e, lbl_β);
            flat_box_call(e, rdi_arg, "bb_fence", 1);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        } else {
            fence_t *z = bb_fence_new();
            flat_emit_box_call(e, bb_fence, "bb_fence", z, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    case XFARB: {
        if (e->is_text) {
            flat_emit_box_banner(e, "ARB", NULL, lbl_succ->name);
            int id = g_flat_node_id++;
            char lbl[64]; snprintf(lbl, sizeof(lbl), ".Larb%d_z", id);
            flat_data_section(e);
            flat3c_label(e, lbl);
            flat_data_long(e, 0);
            flat_data_long(e, 0);
            flat_text_section(e);
            flat_intel_syntax(e);
            char rdi_arg[96]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", lbl);
            flat_box_call(e, rdi_arg, "bb_arb", 0);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
            EV_LABEL(e, lbl_β);
            flat_box_call(e, rdi_arg, "bb_arb", 1);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        } else {
            arb_t *z = bb_arb_new();
            flat_emit_box_call(e, bb_arb, "bb_arb", z, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    case XSTAR: {
        if (e->is_text) {
            flat_emit_box_banner(e, "REM", NULL, lbl_succ->name);
            int id = g_flat_node_id++;
            char lbl[64]; snprintf(lbl, sizeof(lbl), ".Lrem%d_z", id);
            flat_data_section(e);
            flat3c_label(e, lbl);
            flat_data_long(e, 0);
            flat_text_section(e);
            flat_intel_syntax(e);
            char rdi_arg[96]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", lbl);
            flat_box_call(e, rdi_arg, "bb_rem", 0);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
            EV_LABEL(e, lbl_β);
            flat_box_call(e, rdi_arg, "bb_rem", 1);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        } else {
            rem_t *z = bb_rem_new();
            flat_emit_box_call(e, bb_rem, "bb_rem", z, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    case XBRKX: {
        if (e->is_text) {
            const char *cs0 = p->STRVAL_fn ? p->STRVAL_fn : "";
            char preview[40];
            int n = (int)strlen(cs0);
            if (n > 24) snprintf(preview, sizeof(preview), "'%.24s...'", cs0);
            else        snprintf(preview, sizeof(preview), "'%s'", cs0);
            flat_emit_box_banner(e, "BREAKX", preview, lbl_succ->name);
            int id = g_flat_node_id++;
            char lbl[64], slbl[64];
            snprintf(lbl,  sizeof(lbl),  ".Lbrkx%d_z",     id);
            snprintf(slbl, sizeof(slbl), ".Lbrkx%d_chars", id);
            const char *cs = p->STRVAL_fn ? p->STRVAL_fn : "";
            flat_data_section(e);
            flat3c_label(e, slbl);
            flat_data_string(e, cs);
            flat3c_label(e, lbl);
            flat_data_quad(e, slbl);                  /* &chars */
            flat_data_long(e, 0);                     /* delta */
            flat_text_section(e);
            flat_intel_syntax(e);
            char rdi_arg[96]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", lbl);
            flat_box_call(e, rdi_arg, "bb_breakx", 0);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
            EV_LABEL(e, lbl_β);
            flat_box_call(e, rdi_arg, "bb_breakx", 1);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        } else {
            brkx_t *z = bb_breakx_new(p->STRVAL_fn?p->STRVAL_fn:"");
            flat_emit_box_call(e, bb_breakx, "bb_breakx", z, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    case XATP: {
        if (e->is_text) {
            const char *vn0 = p->STRVAL_fn ? p->STRVAL_fn : "";
            flat_emit_box_banner(e, "USERPAT", vn0, lbl_succ->name);
            int id = g_flat_node_id++;
            char lbl[64], vlbl[64];
            snprintf(lbl,  sizeof(lbl),  ".Latp%d_z",     id);
            snprintf(vlbl, sizeof(vlbl), ".Latp%d_vname", id);
            const char *vn = p->STRVAL_fn ? p->STRVAL_fn : "";
            flat_data_section(e);
            flat3c_label(e, vlbl);
            flat_data_string(e, vn);
            flat3c_label(e, lbl);
            flat_data_long(e, 0);
            flat_data_long(e, 0);
            flat_data_quad(e, vlbl);
            flat_text_section(e);
            flat_intel_syntax(e);
            char rdi_arg[96]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", lbl);
            flat_box_call(e, rdi_arg, "bb_atp", 0);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
            EV_LABEL(e, lbl_β);
            flat_box_call(e, rdi_arg, "bb_atp", 1);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        } else {
            atp_t *z = bb_atp_new(p->STRVAL_fn?p->STRVAL_fn:"");
            flat_emit_box_call(e, bb_atp, "bb_atp", z, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    case XDSAR: { /* XDSAR: *varname — re-resolves NV[varname] on every α */
        if (e->is_text) {
            const char *vn0 = p->STRVAL_fn ? p->STRVAL_fn : "";
            char banner_args[80]; snprintf(banner_args, sizeof(banner_args), "*%s", vn0);
            flat_emit_box_banner(e, "DEREF", banner_args, lbl_succ->name);
            int id = g_flat_node_id++;
            char zlbl[64], slbl[64];
            snprintf(zlbl, sizeof(zlbl), ".Ldvar%d_z",    id);
            snprintf(slbl, sizeof(slbl), ".Ldvar%d_name", id);
            const char *vn = p->STRVAL_fn ? p->STRVAL_fn : "";
            flat_data_section(e);
            flat3c_label(e, slbl);
            flat_data_string(e, vn);
            flat3c_label(e, zlbl);
            flat_data_quad(e, slbl);                 /* name */
            flat_data_quad(e, "0");                  /* child_fn */
            flat_data_quad(e, "0");                  /* child_state */
            flat_data_quad(e, "0");                  /* child_size */
            flat_data_long(e, 0);                    /* in_progress */
            flat_data_long(e, 0);                    /* padding */
            flat_text_section(e);
            flat_intel_syntax(e);
            char rdi_arg[96]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", zlbl);
            flat_box_call(e, rdi_arg, "bb_deferred_var_exported", 0);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
            EV_LABEL(e, lbl_β);
            flat_box_call(e, rdi_arg, "bb_deferred_var_exported", 1);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        } else {
            void *z = bb_dvar_bin_new(p->STRVAL_fn?p->STRVAL_fn:"");
            flat_emit_box_call(e, bb_deferred_var_exported, "bb_deferred_var_exported",
                               z, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    case XARBN: { /* ARBNO: zero-or-more greedy repetition of child.
                   * Emit child as callable sub-proc; arbno_t allocated at startup
                   * via rt_init_arbno(.Larbno_slot, child_α). */
        if (e->is_text && g_cap_fixup_cb) {
            flat_emit_box_banner(e, "ARBNO", NULL, lbl_succ->name);
            int child_id = g_flat_node_id++;
            char slot_lbl[128], α_lbl[128];
            snprintf(slot_lbl,  sizeof(slot_lbl),  ".Larbno%d_slot", child_id);
            snprintf(α_lbl, sizeof(α_lbl), "arbno%d_child_α", child_id);
            /* Emit arbno_t* slot in .data (holds heap ptr after startup) */
            flat_data_section(e);
            flat3c_label(e, slot_lbl);
            flat_data_quad(e, "0");
            flat_text_section(e);
            flat_intel_syntax(e);
            /* Emit child sub-proc */
            bb_label_t cs, cf, cb;
            bb_label_initf(&cs, "arbno%d_\xCE\xB3",        child_id);  /* _arbno%d_γ */
            bb_label_initf(&cf, "arbno%d_\xCF\x89",        child_id);  /* _arbno%d_ω */
            bb_label_initf(&cb, "arbno%d_\xCE\xB2",        child_id);  /* _arbno%d_β */
            flat_globl(e, α_lbl);
            flat3c_label(e, α_lbl);
            {   bb_insn_desc_t d = {BB_INSN_LEA_R10_SYM, ADDR_DELTA, 0, 0, SYM_DELTA};
                e->emit_insn(e, &d);
            }
            char ab_lbl[128]; snprintf(ab_lbl, sizeof(ab_lbl), "arbno%d_\xCE\xB1_body", child_id);  /* _arbno%d_α_body */
            bb_label_t alpha_body; bb_label_initf(&alpha_body, "%s", ab_lbl);
            flat_box_entry_dispatch(e, &alpha_body, &cb);
            EV_LABEL(e, &alpha_body);
            PATND_t *ch = p->nchildren > 0 ? p->children[0] : NULL;
            flat_emit_node(e, ch, &cs, &cf, &cb);
            EV_LABEL(e, &cs);
            ev_sigma_plus_delta(e, ADDR_SIGMA); ev_mov_rdx_rax(e); ev_mov_eax_imm32(e, 1); ev_ret(e);
            EV_LABEL(e, &cf);
            ev_mov_eax_imm32(e, 99); ev_xor_edx_edx(e); ev_ret(e);
            /* Register arbno startup fixup: flag=(void*)2 → rt_init_arbno */
            g_cap_fixup_cb((void*)(uintptr_t)2, α_lbl);
            /* Emit arbno box call via slot pointer (qword ptr deref, not lea+rip) */
            flat_box_call_slot(e, slot_lbl, "bb_arbno", 0);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
            EV_LABEL(e, lbl_β);
            flat_box_call_slot(e, slot_lbl, "bb_arbno", 1);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        } else {
            /* Binary path: build child via bb_build_binary_node, wire into arbno */
            /* For now fall through to default — binary path uses bb_build */
            EV_LABEL(e, lbl_β);
            EV_JMP(e, lbl_fail, JMP_JMP);
            EV_JMP(e, lbl_fail, JMP_JMP);
        }
        break;
    }
    case XNME:   /* pat . var — conditional capture */
    case XFNME: { /* pat $ var — immediate capture */
        int immediate = (p->kind == XFNME) ? 1 : 0;
        const char *vn = (p->var.v==DT_S && p->var.s) ? p->var.s :
                         (p->var.v==DT_N && p->var.slen==0 && p->var.s) ? p->var.s : "";
        if (e->is_text && g_cap_fixup_cb) {
            const char *kind_name = (p->kind == XFNME) ? "CAP_IMM" : "CAP_COND";
            char banner_args[80]; snprintf(banner_args, sizeof(banner_args), "%s", vn ? vn : "");
            flat_emit_box_banner(e, kind_name, banner_args, lbl_succ->name);
            /* TEXT mode: emit cap_t as static .data (120 bytes) so the emitted
             * binary has a linker-assigned address for it — no heap ptr baked.
             *
             * cap_t layout (offsets from cap_offsets.c):
             *   0:   fn (8)          — .quad 0, patched at startup
             *   8:   state (8)       — .quad 0 (NULL)
             *   16:  immediate (4)   — .long imm
             *   20:  padding (4)
             *   24:  name.kind (4)   — .long 0 (NM_VAR)
             *   28:  padding (4)
             *   32:  name.var_name (8) — .quad .Lvarname_N
             *   40:  rest of NAME_t (56) — .zero 56
             *   96:  pending (16) + has_pending (4) + registered (4) — .zero 24
             */
            int child_id = g_flat_node_id++;
            char cap_lbl[128], vname_lbl[128], α_lbl[128];
            snprintf(cap_lbl,   sizeof(cap_lbl),   ".Lcap%d_data", child_id);
            snprintf(vname_lbl, sizeof(vname_lbl), ".Lcap%d_vname", child_id);
            snprintf(α_lbl, sizeof(α_lbl), "cap%d_child_α", child_id);

            /* Emit varname string + static cap_t in .data */
            flat_data_section(e);
            flat3c_label(e, vname_lbl);
            flat_data_string(e, vn);
            flat3c_label(e, cap_lbl);
            flat_data_quad(e, "0");                       /* fn (patched at startup) */
            flat_data_quad(e, "0");                       /* state = NULL */
            flat_data_long(e, (long long)immediate);      /* immediate */
            flat_data_long(e, 0);                         /* padding */
            flat_data_long(e, 0);                         /* name.kind = NM_VAR = 0 */
            flat_data_long(e, 0);                         /* padding */
            flat_data_quad(e, vname_lbl);                 /* name.var_name = &varname_str */
            flat_data_zero(e, 56);                        /* rest of NAME_t */
            flat_data_zero(e, 24);                        /* pending + has_pending + registered */
            flat_text_section(e);
            flat_intel_syntax(e);

            /* Emit child as callable sub-proc in .text */
            bb_label_t cs, cf, cb;
            bb_label_initf(&cs, "cap%d_\xCE\xB3",   child_id);   /* _cap%d_γ */
            bb_label_initf(&cf, "cap%d_\xCF\x89",   child_id);   /* _cap%d_ω */
            bb_label_initf(&cb, "cap%d_\xCE\xB2",   child_id);   /* _cap%d_β */
            flat_globl(e, α_lbl);
            flat3c_label(e, α_lbl);
            {   bb_insn_desc_t d = {BB_INSN_LEA_R10_SYM, ADDR_DELTA, 0, 0, SYM_DELTA};
                e->emit_insn(e, &d);
            }
            char ab_lbl[128]; snprintf(ab_lbl, sizeof(ab_lbl), "cap%d_\xCE\xB1_body", child_id);  /* _cap%d_α_body */
            bb_label_t alpha_body; bb_label_initf(&alpha_body, "%s", ab_lbl);
            flat_box_entry_dispatch(e, &alpha_body, &cb);
            EV_LABEL(e, &alpha_body);
            PATND_t *ch = p->nchildren > 0 ? p->children[0] : NULL;
            flat_emit_node(e, ch, &cs, &cf, &cb);
            /* Success epilogue: return DT_S with Σ+Δ */
            EV_LABEL(e, &cs);
            ev_sigma_plus_delta(e, ADDR_SIGMA); ev_mov_rdx_rax(e); ev_mov_eax_imm32(e, 1); ev_ret(e);
            /* Fail epilogue: return DT_FAIL */
            EV_LABEL(e, &cf);
            ev_mov_eax_imm32(e, 99); ev_xor_edx_edx(e); ev_ret(e);

            /* Register fixup: pass cap_lbl name as (void*) — emit_file_header
             * treats NULL cap_ptr as "use symbolic label in child_label field" */
            /* Actually: store cap_lbl in child_label[64..128], α in [0..64] */
            /* For simplicity: re-use fixup struct with α_lbl in child_label
             * and cap_lbl as a separate static string (it IS static — stack local,
             * but we copy it). Store cap_lbl as the "cap_ptr" encoded as string. */
            /* Use a parallel fixup: pack both labels via g_cap_fixup_cb with
             * the convention that if cap_ptr has bit 0 set, it's a sym-label ptr */
            /* CLEANEST: just inline the patch call here in emit_file_header
             * by appending directly. Use a separate global for sym-cap fixups. */
            /* Store in g_cap_fixups with cap_ptr = NULL and encode cap_lbl in
             * the child_label field after a NUL separator */
            {
                char combined[256];
                snprintf(combined, sizeof(combined), "%s", α_lbl);
                /* We need cap_lbl too — store it in a way emit_file_header can use.
                 * Abuse: pass cap_ptr = (void*)(uintptr_t)1 as a flag for symbolic */
                g_cap_fixup_cb((void*)(uintptr_t)1, combined);
                /* Also store cap_lbl — need a second call or a separate mechanism.
                 * For now: cap_data_lbl is encoded right before α_lbl by convention
                 * since child_id is shared. emit_file_header reconstructs it. */
            }

            /* Emit cap box call using RIP-relative address for static cap_t */
            char rdi_arg[160]; snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", cap_lbl);
            flat_box_call(e, rdi_arg, "bb_cap", 0);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
            EV_LABEL(e, lbl_β);
            flat_box_call(e, rdi_arg, "bb_cap", 1);
            flat_box_dispatch_jne_jmp(e, lbl_succ, lbl_fail);
        } else {
            /* Binary path: heap cap_t */
            cap_t *z = bb_cap_new(NULL, NULL, vn, NULL, immediate);
            flat_emit_box_call(e, bb_cap, "bb_cap", z, lbl_succ, lbl_fail, lbl_β);
        }
        break;
    }
    default:
        /* Unknown or excluded kind — emit β→fail stub */
        EV_LABEL(e, lbl_β);
        EV_JMP(e, lbl_fail, JMP_JMP);
        EV_JMP(e, lbl_fail, JMP_JMP);
        break;
    }
}

/* ── invariance check ────────────────────────────────────────────────────── */
/* A pattern is invariant when its BB graph structure is fully known at
 * emit time. The only non-invariant node is XVAR: a runtime DESCR_t used
 * directly as a pattern component whose graph is unknown at emit time.
 *
 * XARBN (ARBNO) and XFNME/XNME (captures with child subtrees) also require
 * recursive child-blob emission not yet supported in bb_flat TEXT path.
 * They fall back to the runtime variant path until that infrastructure lands. */
static int flat_is_eligible(PATND_t *p)
{
    if (!p) return 1;
    if (p->kind == XVAR) return 0;  /* runtime DESCR_t as pattern — graph unknown */
    if (p->kind == XCAT && p->nchildren > 2) return 0;
    for (int i = 0; i < p->nchildren; i++)
        if (!flat_is_eligible(p->children[i])) return 0;
    return 1;
}

/* ── shared emission body ─────────────────────────────────────────────────── */
static int flat_emit_body_v(emitter_v *e, PATND_t *p,
                            const char *prefix, int text_externalise)
{
    bb_label_t lbl_α, lbl_α_body, lbl_succ, lbl_fail, lbl_β;
    bb_label_initf(&lbl_α,      "%s_α",      prefix);
    bb_label_initf(&lbl_α_body, "%s_α_body", prefix);
    bb_label_initf(&lbl_succ,       "%s_γ",      prefix);
    bb_label_initf(&lbl_fail,       "%s_ω",      prefix);
    bb_label_initf(&lbl_β,       "%s_β",       prefix);

    /* EM-FORMAT-BB-DATA-CONSOLIDATE: reset per-blob deferred-data state.  In
     * BINARY mode, helpers gate on `e->is_text` so the reset is harmless. */
    if (text_externalise) data_buf_reset();

    /* TEXT mode: external _α label must be at the TRUE function entry
     * (before the r10-setup preamble), so that bb_broker can call fn(ζ,0)
     * or fn(ζ,1) and have r10 = &Δ ready.  The dispatch then jumps to
     * lbl_α_body (α path) or lbl_β (β path).
     * BINARY mode: the function starts at offset 0 which IS the preamble;
     * no external symbols emitted, so the label placement doesn't matter. */
    if (text_externalise) {
        /* EM-FORMAT-BB-BOX-BANNERS: pattern-level banner above the
         * blob's exported symbols.  Reconstructed PATND_t source. */
        flat_emit_pat_banner(e, prefix, p);
        EV_GLOBAL(e, lbl_α.name);
        EV_GLOBAL(e, lbl_β.name);
        EV_GLOBAL(e, lbl_succ.name);
        EV_GLOBAL(e, lbl_fail.name);
        /* External α = function entry (before preamble) */
        EV_LABEL(e, &lbl_α);
    }

    /* entry: r10 = &Δ; cmp esi, 0; je α_body (α path); else jmp β
     * TEXT:   lea r10, [rip + Δ]   (via BB_INSN_LEA_R10_SYM)
     * BINARY: mov r10, imm64       (via ev_load_r10_delta_ptr)           */
    {   bb_insn_desc_t d = {BB_INSN_LEA_R10_SYM, ADDR_DELTA, 0, 0, SYM_DELTA};
        e->emit_insn(e, &d);
    }
    flat_box_entry_dispatch(e, &lbl_α_body, &lbl_β);

    /* Internal α_body label (dispatch target within function).
     * In BINARY mode, lbl_α is also defined here (same offset = function
     * start + preamble size; fine since binary blobs are called at offset 0). */
    EV_LABEL(e, &lbl_α_body);
    if (!text_externalise) EV_LABEL(e, &lbl_α);   /* binary: α = α_body */
    flat_emit_node(e, p, &lbl_succ, &lbl_fail, &lbl_β);

    /* PAT_γ: success → return DESCR_t{v=DT_S=1, rdx=Σ+Δ} */
    EV_LABEL(e, &lbl_succ);
    ev_sigma_plus_delta(e, ADDR_SIGMA);  /* rax = Σ+Δ */
    ev_mov_rdx_rax(e);                  /* rdx = Σ+Δ (σ) */
    ev_mov_eax_imm32(e, 1);             /* rax = DT_S=1 */
    ev_ret(e);

    /* PAT_ω: failure → return DT_FAIL=99 */
    EV_LABEL(e, &lbl_fail);
    ev_mov_eax_imm32(e, 99);
    ev_xor_edx_edx(e);
    ev_ret(e);

    /* EM-FORMAT-BB-DATA-CONSOLIDATE: flush all deferred data as ONE
     * `.section .data` block at end of blob, then restore `.section .text`.
     * `.intel_syntax noprefix` persists across `.section` switches in GAS,
     * so we don't re-emit it. */
    if (text_externalise && e->is_text && g_flat_data_any) {
        /* Flush any unfused pending data-label (rare — end of buffer with a
         * trailing label and no content line), so it lands inside the .data
         * block, not after the section switches. */
        data_buf_flush_pending_label();
        /* Make sure any pending bb3c labels flush before we emit the section
         * directive (defends against EM-FORMAT-BB-LONE-LABELS regression). */
        bb3c_flush_pending();
        flat3c(e, "", ".section", ".data");
        /* Dump the buffered content directly (already three-column shaped). */
        EV_TEXT(e, "%.*s", (int)g_flat_data_len, g_flat_data_buf);
        flat3c(e, "", ".section", ".text");
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
    emitter_v *e = emitter_binary_new(buf, FLAT_BUF_MAX);
    if (!e) { bb_free(buf, FLAT_BUF_MAX); return NULL; }
    flat_emit_body_v(e, p, "pat_flat", 0);
    int nbytes = emitter_end(e);
    emitter_free(e);
    if (nbytes <= 0 || nbytes > FLAT_BUF_MAX) { bb_free(buf, FLAT_BUF_MAX); return NULL; }
    bb_seal(buf, (size_t)nbytes);
    return (bb_box_fn)buf;
}

int bb_build_flat_text(PATND_t *p, FILE *out, const char *prefix)
{
    if (!flat_is_eligible(p)) return -1;
    /* EM-7c: do NOT reset g_flat_node_id here — multiple patterns
     * emitted into the same `.s` rely on the monotonic counter to
     * avoid internal-label collisions (xcat0_mid_g, xcat1_mid_g, ...).
     * Use bb_build_flat_text_reset() between unrelated emit runs. */
    emitter_v *e = emitter_text_new(out);
    if (!e) return -1;
    e->intern_str = g_flat_intern_str;  /* wire strtab callback if set */
    int rc = flat_emit_body_v(e, p, prefix, 1);
    emitter_end(e);
    emitter_free(e);
    /* EM-FORMAT-BB lone-label fusion: flush any pending label before returning,
     * so callers (notably bb_flat_text_test) that don't go through the
     * sm_codegen_x64_emit path still get every label written to disk. */
    bb3c_flush_pending();
    return rc;
}

void bb_build_flat_text_reset(void)
{
    /* EM-7c: called by sm_codegen_x64_emit at the start of each emit run
     * so internal label IDs (xcatN, altN, ...) start from 0 per output `.s`.
     * Within a single emit run, the counter MUST monotonically increment
     * to keep labels unique across patterns sharing the same namespace. */
    g_flat_slot_count = 0;
    g_flat_node_id    = 0;
}

/* ── EM-7c-bb-macros: BB macro library writer ────────────────────────────
 * Writes bb_macros.s to path (typically "bb_macros.s" in CWD).
 * One named .macro/.endm per leaf-box α/β port.  Three-column shape.
 * Returns 0 on success, -1 on I/O error. */

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
    fprintf(f, "# One named .macro/.endm per leaf-box port.  Three-column shape.\n");
    fprintf(f, "# GAS expands to byte-identical inline x86 (same as ev_* emissions).\n");
    /* DELTA_LOAD: eax = [r10] */
    bm_macro(f, "DELTA_LOAD", "");
    bm_op   (f, "mov", "eax, [r10]");
    bm_endm (f);
    /* SIGLEN_LOAD: eax = *Σlen */
    bm_macro(f, "SIGLEN_LOAD", "");
    bm_op   (f, "lea", "rcx, [rip + \xCE\xA3len]");
    bm_op   (f, "mov", "eax, [rcx]");
    bm_endm (f);
    /* EPS_α lbl_succ */
    bm_macro(f, "EPS_\xCE\xB1", "lbl_succ");
    bm_jmp  (f, "jmp", "lbl_succ");
    bm_endm (f);
    /* EPS_β lbl_fail */
    bm_macro(f, "EPS_\xCE\xB2", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    /* FAIL_α lbl_fail */
    bm_macro(f, "FAIL_\xCE\xB1", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    /* FAIL_β lbl_fail */
    bm_macro(f, "FAIL_\xCE\xB2", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    /* RPOS_α n, lbl_succ, lbl_fail */
    bm_macro(f, "RPOS_\xCE\xB1", "n, lbl_succ, lbl_fail");
    bm_op   (f, "SIGLEN_LOAD", "");
    bm_op   (f, "sub", "eax, \\n");
    bm_op   (f, "mov", "ecx, eax");
    bm_op   (f, "DELTA_LOAD", "");
    bm_op   (f, "cmp", "eax, ecx");
    bm_jmp  (f, "jne", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_succ");
    bm_endm (f);
    /* RPOS_β lbl_fail */
    bm_macro(f, "RPOS_\xCE\xB2", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    /* POS_α n, lbl_succ, lbl_fail */
    bm_macro(f, "POS_\xCE\xB1", "n, lbl_succ, lbl_fail");
    bm_op   (f, "DELTA_LOAD", "");
    bm_op   (f, "cmp", "eax, \\n");
    bm_jmp  (f, "jne", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_succ");
    bm_endm (f);
    /* POS_β lbl_fail */
    bm_macro(f, "POS_\xCE\xB2", "lbl_fail");
    bm_jmp  (f, "jmp", "lbl_fail");
    bm_endm (f);
    fprintf(f, "# === END bb macro library ===\n");
    return fclose(f) == 0 ? 0 : -1;
}
