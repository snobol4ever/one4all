/*
 * bb_flat.c — Flat-Glob Invariant Pattern Emitter (M-DYN-FLAT)
 *
 * Emits an entire invariant PATND_t tree as one contiguous x86-64 blob.
 * All sub-boxes are inlined flat; control flows via direct jmp, never call/ret.
 *
 * EM-7b'': Zero byte knowledge in this file.  Every emission is a named
 * helper call (emit_load_delta, emit_mov_rax_imm64, etc.) that routes through
 * Global emitter state -> TEXT (readable mnemonic) or BINARY (bytes).
 * The walker reads as a description of pattern-matcher semantics, not x86.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  RT-129 / M-DYN-FLAT (EM-7b'')
 */

#include "emit_bb_flat.h"
#include "emit_bb_gen.h"
#include "emitter.h"
#include "snobol4.h"
#include "bb_box.h"
#include "templates.h"   /* EM-MODE4-IS-MODE3-DUMP-d: emit_bb_xchr */
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

/* EM-MODE4-IS-MODE3-DUMP-e: promoted from static to extern so
 * template files (templates/bb_xspnc.c etc.) can generate unique
 * per-node static-data labels without including bb_flat.c's internals.
 * Declaration lives in bb_flat.h. */
int g_flat_node_id   = 0;
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
 * At end of `emit_flat_body`: `flat_data_consolidate_flush(e)` emits ONE
 * `.section .data` directive, dumps the buffer, restores `.section .text`
 * and `.intel_syntax noprefix`, then resets state.
 *
 * BINARY mode: state is unused; helpers gate on `g_is_text` first.         */

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
 * Called from emit_flat_node when XNME/XFNME emits a child sub-blob. */
static void (*g_cap_fixup_cb)(void *cap_ptr, const char *child_α_label) = NULL;

void bb_flat_set_cap_fixup_cb(void (*cb)(void *cap_ptr, const char *child_α_label))
{
    g_cap_fixup_cb = cb;
}

/* ── address constants — EM-7c-symbolic ──────────────────────────────────── */
/* Symbol names for globals exported by libscrip_rt.so.                      */
/* TEXT mode: emit_lea_rcx_sym emits  lea rcx, [rip + sym]                     */
/* BINARY mode: emit_lea_rcx_sym emits mov rcx, imm64  (process-address)       */
#define SYM_SIGMA   "\xCE\xA3"          /* UTF-8: Σ */
#define SYM_SIGLEN  "\xCE\xA3""len"     /* UTF-8: Σlen */
#define SYM_DELTA   "\xCE\x94"          /* UTF-8: Δ */
#define ADDR_SIGMA   ((uint64_t)(uintptr_t)&Σ)
#define ADDR_SIGLEN  ((uint64_t)(uintptr_t)&Σlen)
#define ADDR_DELTA   ((uint64_t)(uintptr_t)&Δ)

/* ── intern_str hook — set by sm_codegen_x64_emit before calling bb_build_flat_text */
/* NULL = no strtab available (BINARY mode or standalone use).               */
static const char *(*g_flat_intern_str)(const char *s) = NULL;

void bb_flat_set_intern_str(const char *(*fn)(const char *))
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
 * flat3c_*       — three-column line emitters routed through the emitter_t's
 *                  fprintf_raw to keep TEXT-mode shape uniform.
 * flat_data_*    — convenience wrappers for `.section .data` directives.
 * flat_text_*    — convenience wrappers for `.section .text` / `.intel_syntax`.
 * flat_box_call  — emits the four-line (lea / mov esi / call / test) sequence.
 *
 * All helpers are no-ops in BINARY mode (g_is_text == 0).  Callers gate on
 * `g_is_text` already; the helpers stay safe for callers that don't.
 * ──────────────────────────────────────────────────────────────────────── */

static void flat3c(const char *lbl, const char *act, const char *got)
{
    if (!g_is_text) return;
    /* EM-FORMAT-BB lone-label fusion (2026-05-09): route through
     * bb3c_format so the pending-label buffer covers data-section labels
     * (e.g. .Lcap1_data:, .Llen2_z:) and child-entry labels (cap1_child_α:)
     * that previously emitted standalone label-only lines. */
    FILE *f = bb_emit_out;
    if (!f) return;
    bb3c_format(f, lbl ? lbl : "", act ? act : "", got ? got : "");
}

static void flat3c_action(const char *act, const char *args)
{
    flat3c("", act, args ? args : "");
}

/* Forward decls for data-buffer helpers used by flat3c_label below; full
 * definitions sit just after the label helper. */
static void data_buf_remember_label(const char *name);

void flat3c_label(const char *name)
{
    if (!g_is_text) return;
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
    flat3c(buf, "", "");
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

static void data_buf_emit_block_comment(void)
{
    g_flat_data_block_nlbls = 0;
}

void flat_data_section(void)
{
    if (!g_is_text) return;
    /* Begin a new deferred-data block.  The main text stream stays in `.text`;
     * everything until the next flat_text_section() is buffered. */
    g_flat_data_active = 1;
    g_flat_data_any    = 1;
    g_flat_data_block_nlbls = 0;
}

void flat_text_section(void)
{
    if (!g_is_text) return;
    if (g_flat_data_active) {
        /* End of a buffered data block: emit the `# data: ...` comment to the
         * main stream and toggle active off.  We never actually left `.text`
         * on the main stream, so no real `.section .text` needed here.
         * Set just-closed so the box-emitted trailing `.intel_syntax noprefix`
         * (which always pairs with `.section .text`) also gets suppressed. */
        data_buf_emit_block_comment();
        g_flat_data_active = 0;
        g_flat_data_just_closed = 1;
        return;
    }
    /* Not buffering: legacy behaviour (rare; only for callers outside the
     * pat_inv blob pipeline that may toggle sections directly). */
    flat3c("", ".section", ".text");
}

void flat_intel_syntax(void)
{
    if (!g_is_text) return;
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
    flat3c("", ".intel_syntax", "noprefix");
}

void flat_data_string(const char *s)
{
    if (!g_is_text) return;
    /* Build the escaped quoted form once.  We escape only " and \, and
     * non-printables as \NNN — matches the assembler's expectation and
     * mirrors emit_flat_charset_call's per-byte loop. */
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

/* Emit the three-line box-call sequence: `lea rdi,[rip+ζ]` / `mov esi,mode` /
 * `call fn@PLT`.  The trailing `test rax, rax` was previously here but is
 * now folded into `flat_box_dispatch_jne_jmp` below so the test concats
 * onto one line with the cond+uncond jmps that follow (per EM-FORMAT-BB-LAW
 * "no jmp instruction with only another jmp instruction on that line"). */
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

/* Variant: arbno's box call uses a slot pointer dereference rather than
 * lea+rip.  Same three-line shape, different first instruction. */
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

/* EM-FORMAT-BB-LAW (TRIPLE-FUSION): emit
 *   test  rax, rax;<pad>jne <succ>; jmp <fail>
 * as ONE line.  Layout:
 *   col-2 = "test"
 *   col-3 = "rax, rax;" + spaces to width 27
 *   col-4 = "jne <succ>; jmp <fail>"
 * Replaces the prior 3-line emission
 *   emit_test_rax_rax();
 *   emit_jmp_label(lbl_succ, JMP_JNE);
 *   emit_jmp_label(lbl_fail, JMP_JMP);
 * which violated the LAW.  TEXT mode only. */
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

/* EM-FORMAT-BB-LAW (TRIPLE-FUSION): emit the entry dispatch
 *   cmp  esi, 0;<pad>je <α_body>; jmp <β>
 * as ONE line.  Used at the top of every BB child sub-proc (capture
 * children, arbno children) where mode 0 = α-entry, mode 1 = β-retry.
 * TEXT mode only. */
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
/* EM-MODE4-IS-MODE3-DUMP-d: external linkage (was static).
 * Used from the per-box template files for the BB-box banner shape. */
void emit_flat_banner_rule(char ch)
{
    if (!g_is_text) return;
    char buf[BB_BANNER_RULE_LEN + 4];
    buf[0] = '#';
    for (int i = 0; i < BB_BANNER_RULE_LEN; i++) buf[1 + i] = ch;
    buf[1 + BB_BANNER_RULE_LEN] = '\0';
    emit_fprintf_raw("%s\n", buf);
}

/* Pattern-blob banner: emit at the top of each pat_inv_<id> blob.
 *
 *   #=====================================================================
 *   # pattern <prefix>: <reconstructed source>
 *   #=====================================================================
 */
static void emit_flat_pat_banner(const char *prefix, PATND_t *p)
{
    if (!g_is_text) return;
    (void)prefix; (void)p;
    emit_flat_banner_rule('=');
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
/* EM-MODE4-IS-MODE3-DUMP-d: external linkage (was static).
 * Used from the per-box template files for the BB-box banner shape. */
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
    bb_label_initf(&mid_γ,   "xcat%d_\xCE\xB3",         id);  /* xcat%d_γ */
    bb_label_initf(&right_ω, "xcat%d_right_\xCF\x89",   id);  /* xcat%d_right_ω */
    bb_label_initf(&left_β,  "xcat%d_left_\xCE\xB2",    id);  /* xcat%d_left_β */
    bb_label_initf(&right_β, "xcat%d_right_\xCE\xB2",   id);  /* xcat%d_right_β */
    bb_label_initf(&xcat_ω,  "xcat%d_\xCF\x89",         id);  /* xcat%d_ω */

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
            bb_label_initf(&mids[i],  "xcat%d_mid%d_\xCE\xB3", id, i+1);  /* xcat%d_mid%d_γ */
            bb_label_initf(&betas[i], "xcat%d_mid%d_\xCE\xB2", id, i+1);  /* xcat%d_mid%d_β */
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
        bb_label_initf(&ci_βs[i], "alt%d_c%d_\xCE\xB2", id, i);   /* alt%d_c%d_β */
        bb_label_initf(&ci_ωs[i], "alt%d_c%d_\xCF\x89", id, i);   /* alt%d_c%d_ω */
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
/* EM-MODE4-IS-MODE3-DUMP-d (sess 2026-05-11): retained as rollback
 * reference but no longer called.  XCHR emission now goes through
 * `emit_bb_xchr` in `templates/bb_xchr.c`, which has byte-for-byte
 * the same body.  See sub-rung -d watermark.  Single-line revert:
 * change the call in emit_flat_node's case XCHR back to
 * emit_flat_lit(lit, (int)strlen(lit), lbl_succ, lbl_fail, lbl_β). */
static void emit_flat_lit(const char *lit, int len,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                          bb_label_t *lbl_β) __attribute__((unused));
static void emit_flat_lit(const char *lit, int len,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                          bb_label_t *lbl_β)
{
    /* α: if Δ + len > Σlen → fail */
    emit_load_delta();                                    /* eax = Δ */
    emit_add_eax_imm32((uint32_t)len);                 /* eax += len */
    emit_cmp_eax_siglen(ADDR_SIGLEN);                  /* cmp eax, [Σlen] */
    emit_jmp_label(lbl_fail, JMP_JG);

    /* memcmp(Σ+Δ, lit, len): set up rdi=Σ+Δ, rsi=lit, rdx=len */
    emit_sigma_plus_delta(ADDR_SIGMA);                 /* rax = Σ+Δ */
    emit_mov_rdi_rax();                                  /* rdi = Σ+Δ */
    emit_mov_rdx_imm64((uint64_t)(uint32_t)len);       /* rdx = len */

    /* rsi = lit ptr: TEXT mode → use strtab label; BINARY → raw ptr */
    if (g_is_text && g_flat_intern_str) {
        const char *lbl = g_flat_intern_str(lit);        /* e.g. ".Lstr_N" */
        emit_sym_lea_rcx(lbl, (uint64_t)(uintptr_t)lit); /* lea rcx, [rip + .Lstr_N] */
        emit_fprintf_raw("    mov     rsi, rcx\n");       /* mov rsi, rcx (no LEA_RSI_SYM form) */
    } else {
        emit_mov_rsi_imm64((uint64_t)(uintptr_t)lit);    /* BINARY: bake raw pointer */
    }

    /* call memcmp — TEXT: call memcmp@PLT; BINARY: mov rax, ptr; call rax */
    emit_call_sym_plt("memcmp", (uint64_t)(uintptr_t)memcmp);
    emit_test_eax_eax();                                 /* test eax, eax */
    emit_jmp_label(lbl_fail, JMP_JNE);

    /* success: Δ += len */
    emit_add_delta_imm(len);
    emit_jmp_label(lbl_succ, JMP_JMP);

    /* β: Δ -= len; fail */
    emit_label_define_bb(lbl_β);
    emit_sub_delta_imm(len);
    emit_jmp_label(lbl_fail, JMP_JMP);
}

/* ── leaf: epsilon ──────────────────────────────────────────────────────── */
/* ── XEPS/XFAIL/XFARB text-body callbacks (EM-MODE4-IS-MODE3-DUMP-m) ──────── */





/* ── leaf: POS(n) ───────────────────────────────────────────────────────── */
/* ── XPOSI/XRPSI text-body callbacks (EM-MODE4-IS-MODE3-DUMP-k) ─────────── */




/* ── leaf: charset (ANY/NOTANY/SPAN/BRK) ───────────────────────────────── */
/* EM-MODE4-IS-MODE3-DUMP-e: charset family now routed through
 * emit_bb_charset() in templates/bb_xspnc.c.  Kept as a rollback
 * reference and byte-identity oracle.  Do not call from emit_flat_node. */
__attribute__((unused))
static void emit_flat_charset_call(bb_box_fn c_fn,
                                   const char *c_fn_name,
                                   const char *chars,
                                   bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                                   bb_label_t *lbl_β)
{
    if (g_is_text) {
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
        emit_flat_box_banner(kind, preview, lbl_succ->name);
        /* Static .data: chars string + cs_t {chars*, delta=0} */
        int id = g_flat_node_id++;
        char zlbl[64], slbl[64];
        snprintf(zlbl, sizeof(zlbl), ".Lcs%d_z",     id);
        snprintf(slbl, sizeof(slbl), ".Lcs%d_chars", id);
        flat_data_section();
        flat3c_label(slbl);
        flat_data_string(chars);
        flat3c_label(zlbl);
        flat_data_quad(slbl);                  /* &chars */
        flat_data_long(0);                     /* delta */
        flat_data_long(0);                     /* padding */
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
        /* Binary path: heap cs_t */
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

/* ── charset text-body callback (EM-MODE4-IS-MODE3-DUMP-e) ──────────────── */


/* Arg struct passed to the text-body callback from the per-kind wrappers. */
typedef struct {
    bb_box_fn   c_fn;
    const char *c_fn_name;
    const char *kind_name;
    const char *chars;
} charset_text_arg_t;

/* Text-path body for emit_bb_charset.  Has access to all of bb_flat.c's
 * static helpers (flat_data_section, flat3c_action, etc.).  Called only
 * when g_is_text; binary path lives in templates/bb_xspnc.c. */
static void charset_text_body(bb_label_t *lbl_succ,
                              bb_label_t *lbl_fail,
                              bb_label_t *lbl_β,
                              void *arg_)
{
    const charset_text_arg_t *a = (const charset_text_arg_t *)arg_;
    const char *chars     = a->chars     ? a->chars     : "";
    const char *c_fn_name = a->c_fn_name ? a->c_fn_name : "";
    const char *kind      = a->kind_name ? a->kind_name : "CHARSET";

    /* Banner preview: truncate at 24 chars. */
    char preview[40];
    if (chars && *chars) {
        int n = (int)strlen(chars);
        if (n > 24) snprintf(preview, sizeof(preview), "'%.24s...'", chars);
        else        snprintf(preview, sizeof(preview), "'%s'", chars);
    } else {
        preview[0] = '\0';
    }
    emit_flat_box_banner(kind, preview, lbl_succ->name);

    /* Static .data: charset string + cs_t {chars*, delta=0}. */
    int id = g_flat_node_id++;
    char zlbl[64], slbl[64];
    snprintf(zlbl, sizeof(zlbl), ".Lcs%d_z",     id);
    snprintf(slbl, sizeof(slbl), ".Lcs%d_chars", id);
    flat_data_section();
    flat3c_label(slbl);
    flat_data_string(chars);
    flat3c_label(zlbl);
    flat_data_quad(slbl);   /* &chars  */
    flat_data_long(0);      /* delta   */
    flat_data_long(0);      /* padding */

    /* .text: alpha port — lea rdi,[rip+z]; mov esi,0; call fn@PLT */
    flat_text_section();
    flat_intel_syntax();
    char rdi_arg[96];
    snprintf(rdi_arg, sizeof(rdi_arg), "rdi, [rip + %s]", zlbl);
    flat3c_action("lea", rdi_arg);
    flat3c_action("mov", "esi, 0");
    emit_call_sym_plt(c_fn_name, (uint64_t)(uintptr_t)a->c_fn);
    flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);

    /* beta port */
    emit_label_define_bb(lbl_β);
    flat3c_action("lea", rdi_arg);
    flat3c_action("mov", "esi, 1");
    emit_call_sym_plt(c_fn_name, (uint64_t)(uintptr_t)a->c_fn);
    flat_box_dispatch_jne_jmp(lbl_succ, lbl_fail);
}

/* Per-kind wrappers called from emit_flat_node.  Each builds a
 * charset_text_arg_t and delegates to emit_bb_charset (template), which
 * owns the binary path and dispatches the text path back here. */

/* ── XBRKX text-body callback (EM-MODE4-IS-MODE3-DUMP-i) ─────────────────── */

extern brkx_t  *bb_breakx_new(const char *chars);

/* Arg struct passed to the brkx text-body callback. */
typedef struct {
    const char *chars;
} brkx_text_arg_t;

/* Text-path body for emit_bb_xbrkx.  Access to bb_flat.c static helpers.
 * Structurally identical to charset_text_body but uses brkx label prefix
 * and bb_breakx runtime function. */
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
    flat_data_quad(slbl);   /* &chars */
    flat_data_long(0);      /* delta  */

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


/* ── integer-cursor text-body callback (EM-MODE4-IS-MODE3-DUMP-g) ────────── */


/* Arg struct for the text-body callback. */
typedef struct {
    bb_box_fn   c_fn;
    const char *c_fn_name;
    const char *kind_name;
    const char *lbl_prefix; /* "len" / "tab" / "rtab" — for .Llen%d_z etc. */
    long long   num;
    int         data_pad;   /* 0 = one long only (LEN); 1 = add padding long */
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
/* EM-MODE4-IS-MODE3-DUMP-g: promoted from static to extern so
 * templates/bb_xlnth.c can reuse the alpha/beta dispatch logic. */
void emit_flat_box_call(bb_box_fn fn, const char *fn_name,
                               void *z,
                               bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                               bb_label_t *lbl_β)
{
    /* Save/restore r10 around each runtime call.  r10 holds the flat-BB
     * BLOB's LOCAL (Δ pointer, loaded by lea r10, [rip + Δ_data] in the
     * α-preamble) and must persist across the call so the γ-body can
     * dereference it via `movslq (%r10), %rcx`.  r10 is caller-saved by
     * the AMD64 SysV ABI — the called runtime function is free to clobber
     * it.  Per ARCH-x86.md §"Intra-BLOB vs extra-BLOB jumps": source BLOB
     * push/pops r10 around any outbound call where control resumes inside
     * the same BLOB.  Port calls always resume inside the BLOB. */
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
    /* EDP-5: thin dispatcher — every case calls the matching emit_bb_* template. */
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
    /* XNME/XFNME/XARBN/XCALLCAP: excluded from flat_is_eligible — these cases
     * are never reached via bb_build_flat.  Fall to β→fail stub. */
    default:
        emit_label_define_bb(lbl_β);
        emit_jmp_label(lbl_fail, JMP_JMP);
        emit_jmp_label(lbl_fail, JMP_JMP);
        break;
    }
}
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
    /* SN-33b: XNME/XFNME/XARBN binary path in emit_flat_node passes child_fn=NULL
     * to bb_cap_new (or emits a fail-stub for ARBNO).  bb_cap then dereferences
     * NULL at bb_boxes.c:541.  Until recursive child-blob emission lands in the
     * binary path, fall back to bb_build_binary (which builds the child correctly
     * via bb_build_binary_node — see bb_build.c:bb_nme_emit_binary). */
    /* SL-13c (sess 2026-05-12, Claude Opus 4.7): XCALLCAP also has no
     * emit_flat_node case — falls to `default` which emits a β→fail stub,
     * silently dropping pat . *Fn() deferred calls.  bb_build_binary has a
     * proper bb_callcap_emit_binary handler (bb_build.c:1219) that wires the
     * trampoline to bb_cap with NM_CALL.  Excluding XCALLCAP from flat
     * eligibility routes us there until a real flat XCALLCAP emitter lands. */
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

    /* EM-FORMAT-BB-DATA-CONSOLIDATE: reset per-blob deferred-data state.  In
     * BINARY mode, helpers gate on `g_is_text` so the reset is harmless. */
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
        emit_flat_pat_banner(prefix, p);
        emit_global_sym(lbl_α.name);
        emit_global_sym(lbl_β.name);
        emit_global_sym(lbl_succ.name);
        emit_global_sym(lbl_fail.name);
        /* External α = function entry (before preamble) */
        emit_label_define_bb(&lbl_α);
    }

    /* entry: r10 = &Δ; cmp esi, 0; je α_body (α path); else jmp β
     * TEXT:   lea r10, [rip + Δ]   (via BB_INSN_LEA_R10_SYM)
     * BINARY: mov r10, imm64       (via emit_load_r10_delta_ptr)           */
    {   emit_sym_lea_r10("delta", ADDR_DELTA);
    }
    flat_box_entry_dispatch(&lbl_α_body, &lbl_β);

    /* Internal α_body label (dispatch target within function).
     * In BINARY mode, lbl_α is also defined here (same offset = function
     * start + preamble size; fine since binary blobs are called at offset 0). */
    emit_label_define_bb(&lbl_α_body);
    if (!text_externalise) emit_label_define_bb(&lbl_α);   /* binary: α = α_body */
    emit_flat_node(p, &lbl_succ, &lbl_fail, &lbl_β);

    /* PAT_γ: success → return DESCR_t{v=DT_S=1, rdx=Σ+Δ} */
    emit_label_define_bb(&lbl_succ);
    emit_sigma_plus_delta(ADDR_SIGMA);  /* rax = Σ+Δ */
    emit_mov_rdx_rax();                  /* rdx = Σ+Δ (σ) */
    emit_mov_eax_imm32(1);             /* rax = DT_S=1 */
    if (brokered) emit_pop_rbp();
    emit_ret();

    /* PAT_ω: failure → return DT_FAIL=99 */
    emit_label_define_bb(&lbl_fail);
    emit_mov_eax_imm32(99);
    emit_xor_edx_edx();
    if (brokered) emit_pop_rbp();
    emit_ret();

    /* EM-FORMAT-BB-DATA-CONSOLIDATE: flush all deferred data as ONE
     * `.section .data` block at end of blob, then restore `.section .text`.
     * `.intel_syntax noprefix` persists across `.section` switches in GAS,
     * so we don't re-emit it. */
    if (text_externalise && g_is_text && g_flat_data_any) {
        /* Flush any unfused pending data-label (rare — end of buffer with a
         * trailing label and no content line), so it lands inside the .data
         * block, not after the section switches. */
        data_buf_flush_pending_label();
        /* Make sure any pending bb3c labels flush before we emit the section
         * directive (defends against EM-FORMAT-BB-LONE-LABELS regression). */
        bb3c_flush_pending();
        flat3c("", ".section", ".data");
        /* Dump the buffered content directly (already three-column shaped). */
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

/* ── bb_build_brokered — EM-BB-PURGE-1 / EDP-7 ───────────────────────────
 * Emit the whole invariant pattern tree as one contiguous x86-64 blob
 * wrapped in a C-ABI frame so bb_broker can call fn(ζ, port) via C call.
 * Shape: push rbp; mov rbp, rsp; <flat BB body (brokered=1)>
 * The flat BB body's γ/ω exits emit pop rbp; ret instead of plain ret.
 * ζ=NULL at call time — all state is baked into the blob (r10=&Δ).
 * Returns NULL if the pattern is not flat-eligible. */
bb_box_fn bb_build_brokered(PATND_t *p)
{
    if (!flat_is_eligible(p)) return NULL;
    bb_buf_t buf = bb_alloc(FLAT_BUF_MAX);
    if (!buf) return NULL;
    g_flat_slot_count = 0; g_flat_node_id = 0;
    emit_mode_set(EMIT_BINARY_BROKERED, NULL);
    emitter_init_binary(buf, FLAT_BUF_MAX);
    /* C-ABI prologue: push rbp; mov rbp, rsp — emitted before flat BB body */
    bb_emit_byte(0x55);                                          /* push rbp */
    bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0xE5); /* mov rbp, rsp */
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
    /* EM-7c: do NOT reset g_flat_node_id here — multiple patterns
     * emitted into the same `.s` rely on the monotonic counter to
     * avoid internal-label collisions (xcat0_mid_g, xcat1_mid_g, ...).
     * Use bb_build_flat_text_reset() between unrelated emit runs. */
    emitter_init_text(out, TEXT_MODE_INVOCATION);
    /* intern_str now wired via bb_flat_set_intern_str global */
    int rc = emit_flat_body(p, prefix, 1, 0);
    emitter_end();
    
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
    fprintf(f, "                        .intel_syntax    noprefix\n");
    fprintf(f, "# One named .macro/.endm per leaf-box port.  Three-column shape.\n");
    fprintf(f, "# GAS expands to byte-identical inline x86 (same as emit_* emissions).\n");
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

/* EDP-9: static helpers moved from bb_build.c */
static void emit_load_int_global(const int *addr)
{
    /* mov rax, imm64 */
    bb_emit_byte(0x48); bb_emit_byte(0xB8);
    bb_emit_u64((uint64_t)(uintptr_t)addr);
    /* mov eax, [rax] */
    bb_emit_byte(0x8B); bb_emit_byte(0x00);
}

/* emit:  mov rax, imm64(&global_ptr) / mov rax, [rax]
 * Loads a 64-bit pointer global (const char *).
 * Result in rax. */
static void emit_load_ptr_global(const char **addr)
{
    /* mov rax, imm64 */
    bb_emit_byte(0x48); bb_emit_byte(0xB8);
    bb_emit_u64((uint64_t)(uintptr_t)addr);
    /* mov rax, [rax]  — 48 8B 00 */
    bb_emit_byte(0x48); bb_emit_byte(0x8B); bb_emit_byte(0x00);
}

/* emit:  mov rax, imm64(&global) / add eax, imm32(val) / mov [rax], eax
 * Adds val to a 32-bit global. Clobbers rax, ecx. */
static void emit_add_int_global(const int *addr, int32_t val)
{
    /* mov rcx, imm64(&global) */
    bb_emit_byte(0x48); bb_emit_byte(0xB9);
    bb_emit_u64((uint64_t)(uintptr_t)addr);
    /* mov eax, [rcx] */
    bb_emit_byte(0x8B); bb_emit_byte(0x01);
    /* add eax, imm32 */
    bb_emit_byte(0x05); bb_emit_u32((uint32_t)val);
    /* mov [rcx], eax */
    bb_emit_byte(0x89); bb_emit_byte(0x01);
}

/* emit:  mov rax, imm64(&global) / sub eax, imm32(val) / mov [rax], eax */
static void emit_sub_int_global(const int *addr, int32_t val)
{
    /* mov rcx, imm64(&global) */
    bb_emit_byte(0x48); bb_emit_byte(0xB9);
    bb_emit_u64((uint64_t)(uintptr_t)addr);
    /* mov eax, [rcx] */
    bb_emit_byte(0x8B); bb_emit_byte(0x01);
    /* sub eax, imm32 */
    bb_emit_byte(0x2D); bb_emit_u32((uint32_t)val);
    /* mov [rcx], eax */
    bb_emit_byte(0x89); bb_emit_byte(0x01);
}



/* EDP-9: moved from bb_build.c — no C box deps */

/*
 * Emits the LIT box as x86-64 binary, mirroring bb_lit.s exactly.
 *
 * bb_lit.s structure (annotated):
 *
 *   bb_lit(rdi=ζ, esi=entry):
 *     push rbx; push r12             ; callee-saves
 *     sub  rsp, 16                   ; stack slot for spec_t (σ@0, δ@8)
 *     cmp  esi, 0
 *     je   LIT_α
 *     jmp  LIT_β
 *
 *   LIT_α:
 *     Δ + len > Σlen  →  LIT_ω          ; bounds check
 *     memcmp(Σ+Δ, lit, len) ≠ 0 → LIT_ω
 *     [rsp+0] = Σ+Δ                  ; σ
 *     [rsp+8] = len                  ; δ
 *     Δ += len
 *     jmp LIT_γ
 *
 *   LIT_β:
 *     Δ -= len
 *     jmp LIT_ω
 *
 *   LIT_γ:
 *     rax = [rsp+0]; rdx = [rsp+8]  ; return spec(σ, δ)
 *     add rsp,16; pop r12; pop rbx; ret
 *
 *   LIT_ω:
 *     xor eax,eax; xor edx,edx      ; return spec_empty
 *     add rsp,16; pop r12; pop rbx; ret
 *
 * Binary adaptation:
 *   - ζ->lit / ζ->len replaced by baked imm64/imm32
 *   - Σ/Δ/Σlen accessed via absolute imm64 pointer loads
 *   - memcmp called via mov rax,imm64(&memcmp) / call rax
 *   - push rbx/r12 replaced by push rbx/push r12 (same bytes, kept for ABI)
 *
 * Returns NULL on allocation failure.
 */

/* forward declaration of memcmp for address capture */
extern int memcmp(const void *, const void *, size_t);

/* forward declarations */
extern DESCR_t bb_seq(void *zeta, int entry);
extern DESCR_t bb_tab(void *zeta, int entry);
extern DESCR_t bb_rtab(void *zeta, int entry);

/* M-DYN-B7: capture box — unified in bb_boxes.c (SN-20 session 17).
 * Single source across all three modes; no _exported wrapper needed. */
extern DESCR_t bb_cap(void *zeta, int entry);

/* cap_t + bb_cap_new canonical declarations come from bb_box.h
 * (SN-21c: bb_capture → bb_cap port). */

/* M-DYN-B10: exported shim for static box function in stmt_exec.c.
 * SN-21e: bb_callcap_exported is gone — the XCALLCAP emitter now trampolines
 * directly to bb_cap with an NM_CALL NAME_t, just like XNME / XFNME. */
extern DESCR_t bb_deferred_var_exported(void *zeta, int entry);

/* Mirror of deferred_var_t from stmt_exec.c */
typedef struct {
    const char *name;
    bb_box_fn   child_fn;
    void       *child_state;
    size_t      child_size;
    int         in_progress;
} deferred_var_t_bin;
extern void *bb_dvar_bin_new(const char *name);

/* bb_arbno, bb_fail, bb_atp are in separate .c files — directly linkable */
extern DESCR_t bb_arbno(void *zeta, int entry);
extern void   *bb_arbno_new(bb_box_fn fn, void *state);
extern DESCR_t bb_fail(void *zeta, int entry);
extern DESCR_t bb_atp(void *zeta, int entry);

static bb_box_fn bb_build_binary_node(PATND_t *p);

/* ── DESCR_t return ABI helpers ─────────────────────────────────────────────
 * The bb_broker (U-3/U-9) casts bb_box_fn to univ_box_fn returning DESCR_t.
 * On x86-64 SysV ABI a 16-byte struct return goes rax=first8, rdx=second8.
 *
 *   DESCR_t layout: { DTYPE_t v(4), uint32_t slen(4), union { char *s ... }(8) }
 *     → rax = (slen << 32) | v ,  rdx = s
 *   FAILDESCR  = { v=DT_FAIL=99, slen=0, .i=0 }
 *     → rax = 0x63 (99), rdx = 0
 *
 * Old (broken) ABI was raw spec_t { const char *σ; int δ; } → rax=σ, rdx=δ.
 * On the broken path IS_FAIL_fn(spec_empty) = (0 == DT_FAIL) = false, so every
 * failure looked like a match.  Session #72 root cause.
 *
 * These helpers replace the previous LIT_γ/LIT_ω epilogue stubs.
 *--------------------------------------------------------------------------*/

/* Emit the success-path return:  given [rsp+stk_off+0]=σ, [rsp+stk_off+8]=δ,
 * pack DESCR_t{v=DT_S, slen=δ, s=σ} into rax:rdx.  Does NOT emit the
 * stack-cleanup epilogue or ret — caller is responsible for those. */
static void emit_descr_success_from_stack(int stk_off)
{
    /* mov rdx, [rsp+stk_off+0]      48 8B 54 24 <off>            — rdx = σ */
    bb_emit_byte(0x48); bb_emit_byte(0x8B); bb_emit_byte(0x54); bb_emit_byte(0x24);
    bb_emit_byte((uint8_t)stk_off);
    /* mov eax, [rsp+stk_off+8]      8B 44 24 <off+8>             — eax = δ (low 32) */
    bb_emit_byte(0x8B); bb_emit_byte(0x44); bb_emit_byte(0x24);
    bb_emit_byte((uint8_t)(stk_off + 8));
    /* shl rax, 32                   48 C1 E0 20                  — rax = δ << 32 */
    bb_emit_byte(0x48); bb_emit_byte(0xC1); bb_emit_byte(0xE0); bb_emit_byte(0x20);
    /* or  al, 1                     0C 01                        — rax |= DT_S */
    bb_emit_byte(0x0C); bb_emit_byte(0x01);
}

/* Emit the failure-path return: rax = DT_FAIL (99), rdx = 0.
 * Does NOT emit the stack-cleanup epilogue or ret — caller is responsible. */
static void emit_descr_fail(void)
{
    /* mov eax, 99                   B8 63 00 00 00              — rax = DT_FAIL */
    bb_emit_byte(0xB8); bb_emit_u32(99);
    /* xor edx, edx                  31 D2                       — rdx = 0 */
    bb_emit_byte(0x31); bb_emit_byte(0xD2);
}

bb_box_fn bb_lit_emit_binary(const char *lit, int len)
{
#define BUF_SIZE 768

    bb_buf_t buf = bb_alloc(BUF_SIZE);
    if (!buf) return NULL;

    bb_emit_mode = EMIT_BINARY_WIRED;
    bb_emit_begin(buf, BUF_SIZE);

    /* ── labels ──────────────────────────────────────────────────────── */
    bb_label_t lbl_α, lbl_β, lbl_γ, lbl_ω;
    bb_label_init(&lbl_α, "LIT_α");
    bb_label_init(&lbl_β, "LIT_β");
    bb_label_init(&lbl_γ, "LIT_γ");
    bb_label_init(&lbl_ω, "LIT_ω");

    /* ── prologue ─────────────────────────────────────────────────────
     *   push rbx          53
     *   push r12          41 54
     *   sub  rsp, 16      48 83 EC 10   (stack slot for spec_t)
     *   cmp  esi, 0       83 FE 00
     *   je   LIT_α        74 xx
     *   jmp  LIT_β        EB xx
     * ─────────────────────────────────────────────────────────────── */
    bb_emit_byte(0x53);                         /* push rbx */
    bb_emit_byte(0x41); bb_emit_byte(0x54);     /* push r12 */
    bb_insn_sub_rsp_imm8(16);                   /* sub rsp, 16 */
    bb_insn_cmp_esi_imm8(0);                    /* cmp esi, 0 */
    bb_insn_je_rel8(&lbl_α);                    /* je LIT_α  (α is always nearby — backward) */
    bb_insn_jmp_rel32(&lbl_β);                  /* jmp LIT_β (β is far — use rel32) */

    /* ── LIT_α: bounds check + memcmp ────────────────────────────────
     *   eax = Δ
     *   eax += len
     *   cmp eax, Σlen
     *   jg  LIT_ω
     *   call memcmp(Σ+Δ, lit, len)
     *   test eax, eax
     *   jne LIT_ω
     *   [rsp+0] = Σ+Δ  (rax = Σ, movsxd rcx,Δ, lea rax,[rax+rcx])
     *   [rsp+8] = len
     *   Δ += len
     *   jmp LIT_γ
     * ─────────────────────────────────────────────────────────────── */
    bb_label_define(&lbl_α);

    /* eax = Δ */
    emit_load_int_global(&Δ);           /* mov rax,&Δ; mov eax,[rax] */
    /* eax += len */
    bb_emit_byte(0x05); bb_emit_u32((uint32_t)len);  /* add eax, imm32(len) */
    /* cmp eax, Σlen  →  mov rcx,&Σlen; cmp eax,[rcx] */
    bb_emit_byte(0x48); bb_emit_byte(0xB9);
    bb_emit_u64((uint64_t)(uintptr_t)&Σlen);  /* mov rcx, imm64(&Σlen) */
    bb_emit_byte(0x3B); bb_emit_byte(0x01); /* cmp eax, [rcx] */
    /* jg LIT_ω  (rel32 — α body is ~160 bytes, beyond rel8 range) */
    bb_emit_byte(0x0F); bb_emit_byte(0x8F); bb_emit_patch_rel32(&lbl_ω);

    /* memcmp(Σ+Δ, lit, len):
     *   rdi = Σ+Δ
     *   rsi = lit (imm64)
     *   rdx = len (imm32)
     *   call [imm64(&memcmp)]
     */

    /* compute Σ+Δ into rdi */
    emit_load_ptr_global(&Σ);           /* rax = Σ */
    /* movsxd rcx, [&Δ] */
    bb_emit_byte(0x48); bb_emit_byte(0xB9);
    bb_emit_u64((uint64_t)(uintptr_t)&Δ);   /* mov rcx, imm64(&Δ) */
    bb_emit_byte(0x48); bb_emit_byte(0x63); bb_emit_byte(0x09); /* movsxd rcx,[rcx] */
    /* lea rdi, [rax+rcx] */
    bb_emit_byte(0x48); bb_emit_byte(0x8D); bb_emit_byte(0x3C); bb_emit_byte(0x08);

    /* rsi = lit (imm64) */
    bb_emit_byte(0x48); bb_emit_byte(0xBE);
    bb_emit_u64((uint64_t)(uintptr_t)lit);

    /* rdx = len (imm32, zero-extended) */
    bb_emit_byte(0x48); bb_emit_byte(0xBA);
    bb_emit_u64((uint64_t)(uint32_t)len);

    /* mov rax, imm64(&memcmp); call rax */
    bb_emit_byte(0x48); bb_emit_byte(0xB8);
    bb_emit_u64((uint64_t)(uintptr_t)memcmp);
    bb_insn_call_rax();

    /* test eax, eax */
    bb_emit_byte(0x85); bb_emit_byte(0xC0);
    /* jne LIT_ω */
    bb_insn_jne_rel32(&lbl_ω);      /* jne LIT_ω (rel32) */

    /* σ = Σ+Δ  →  store at [rsp+0] */
    emit_load_ptr_global(&Σ);           /* rax = Σ */
    bb_emit_byte(0x48); bb_emit_byte(0xB9);
    bb_emit_u64((uint64_t)(uintptr_t)&Δ);
    bb_emit_byte(0x48); bb_emit_byte(0x63); bb_emit_byte(0x09); /* movsxd rcx,[rcx] */
    bb_emit_byte(0x48); bb_emit_byte(0x8D); bb_emit_byte(0x04); bb_emit_byte(0x08); /* lea rax,[rax+rcx] */
    /* mov [rsp+0], rax */
    bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0x04); bb_emit_byte(0x24);

    /* δ = len  →  store at [rsp+8] */
    /* mov dword [rsp+8], imm32(len) */
    bb_emit_byte(0xC7); bb_emit_byte(0x44); bb_emit_byte(0x24); bb_emit_byte(0x08);
    bb_emit_u32((uint32_t)len);

    /* Δ += len */
    emit_add_int_global(&Δ, len);

    /* jmp LIT_γ (rel32) */
    bb_insn_jmp_rel32(&lbl_γ);

    /* ── LIT_β: Δ -= len; jmp LIT_ω ──────────────────────────────── */
    bb_label_define(&lbl_β);
    emit_sub_int_global(&Δ, len);
    /* jmp LIT_ω (rel32) */
    bb_insn_jmp_rel32(&lbl_ω);

    /* ── LIT_γ: return DESCR_t{v=DT_S, slen=δ, s=σ} ──────────────── */
    bb_label_define(&lbl_γ);
    /* σ is at [rsp+0], δ is at [rsp+8] */
    emit_descr_success_from_stack(0);
    /* epilogue */
    bb_insn_add_rsp_imm8(16);
    bb_emit_byte(0x41); bb_emit_byte(0x5C);  /* pop r12 */
    bb_emit_byte(0x5B);                       /* pop rbx */
    bb_insn_ret();

    /* ── LIT_ω: return FAILDESCR ──────────────────────────────────── */
    bb_label_define(&lbl_ω);
    emit_descr_fail();
    /* epilogue */
    bb_insn_add_rsp_imm8(16);
    bb_emit_byte(0x41); bb_emit_byte(0x5C);  /* pop r12 */
    bb_emit_byte(0x5B);                       /* pop rbx */
    bb_insn_ret();

    /* ── seal ─────────────────────────────────────────────────────── */
    int nbytes = bb_emit_end();
    if (nbytes <= 0 || nbytes > BUF_SIZE) {
        bb_free(buf, BUF_SIZE);
        return NULL;
    }
    bb_seal(buf, (size_t)nbytes);
    return (bb_box_fn)buf;

#undef BUF_SIZE
}

/* ── bb_eps_emit_binary ─────────────────────────────────────────────────── */
/*
 * Emits the EPS box as x86-64 binary.  EPS has a `done` flag in its zeta,
 * but since in the binary path we bake all args at emit time and EPS needs
 * no per-instance state (binary path: stateless single-shot), we emit the
 * simplest correct version:
 *
 *   EPS α: always succeeds — return spec(Σ+Δ, 0)
 *   EPS β: always fails    — return spec_empty
 *
 * This is correct for the binary path because bb_build_binary is called
 * fresh per statement execution (no cross-statement state).  The done flag
 * in the C path guards against double-γ on backtrack, but our binary boxes
 * are rebuilt each time, so this is safe.
 *
 * ABI: DESCR_t fn(void *zeta_ignored, int entry)
 *   rdi = zeta (ignored)
 *   esi = entry (0=α → succeed, 1=β → fail)
 *
 * Layout:
 *   prologue: push rbx; push r12; sub rsp,16
 *   cmp esi,0; je EPS_α; jmp EPS_β
 *   EPS_α: rax = Σ+Δ; [rsp+0]=rax; [rsp+8]=0; jmp EPS_γ
 *   EPS_β: jmp EPS_ω
 *   EPS_γ: rax=[rsp+0]; rdx=[rsp+8]; epilogue; ret
 *   EPS_ω: xor eax,eax; xor edx,edx; epilogue; ret
 */
bb_box_fn bb_eps_emit_binary(void)
{
#define EPS_BUF_SIZE 256

    bb_buf_t buf = bb_alloc(EPS_BUF_SIZE);
    if (!buf) return NULL;

    bb_emit_mode = EMIT_BINARY_WIRED;
    bb_emit_begin(buf, EPS_BUF_SIZE);

    bb_label_t lbl_α, lbl_β, lbl_γ, lbl_ω;
    bb_label_init(&lbl_α, "EPS_α");
    bb_label_init(&lbl_β, "EPS_β");
    bb_label_init(&lbl_γ, "EPS_γ");
    bb_label_init(&lbl_ω, "EPS_ω");

    /* prologue */
    bb_emit_byte(0x53);                         /* push rbx */
    bb_emit_byte(0x41); bb_emit_byte(0x54);     /* push r12 */
    bb_insn_sub_rsp_imm8(16);                   /* sub rsp, 16 */
    bb_insn_cmp_esi_imm8(0);                    /* cmp esi, 0 */
    bb_insn_je_rel8(&lbl_α);                    /* je EPS_α */
    bb_insn_jmp_rel32(&lbl_β);                  /* jmp EPS_β */

    /* EPS_α: σ = Σ+Δ, δ = 0 */
    bb_label_define(&lbl_α);

    /* rax = Σ (load ptr global) */
    emit_load_ptr_global(&Σ);                   /* rax = Σ */
    /* movsxd rcx, [&Δ] */
    bb_emit_byte(0x48); bb_emit_byte(0xB9);
    bb_emit_u64((uint64_t)(uintptr_t)&Δ);       /* mov rcx, imm64(&Δ) */
    bb_emit_byte(0x48); bb_emit_byte(0x63); bb_emit_byte(0x09); /* movsxd rcx,[rcx] */
    /* lea rax, [rax+rcx]  →  Σ+Δ */
    bb_emit_byte(0x48); bb_emit_byte(0x8D); bb_emit_byte(0x04); bb_emit_byte(0x08);
    /* mov [rsp+0], rax  — σ */
    bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0x04); bb_emit_byte(0x24);
    /* mov qword [rsp+8], 0  — δ = 0 */
    bb_emit_byte(0x48); bb_emit_byte(0xC7); bb_emit_byte(0x44); bb_emit_byte(0x24);
    bb_emit_byte(0x08); bb_emit_u32(0);
    /* jmp EPS_γ */
    bb_insn_jmp_rel32(&lbl_γ);

    /* EPS_β: → ω */
    bb_label_define(&lbl_β);
    bb_insn_jmp_rel32(&lbl_ω);

    /* EPS_γ: return DESCR_t{v=DT_S, slen=0, s=Σ+Δ} */
    bb_label_define(&lbl_γ);
    emit_descr_success_from_stack(0);
    bb_insn_add_rsp_imm8(16);
    bb_emit_byte(0x41); bb_emit_byte(0x5C);      /* pop r12 */
    bb_emit_byte(0x5B);                          /* pop rbx */
    bb_insn_ret();

    /* EPS_ω: return FAILDESCR */
    bb_label_define(&lbl_ω);
    emit_descr_fail();
    bb_insn_add_rsp_imm8(16);
    bb_emit_byte(0x41); bb_emit_byte(0x5C);      /* pop r12 */
    bb_emit_byte(0x5B);                          /* pop rbx */
    bb_insn_ret();

    int nbytes = bb_emit_end();
    if (nbytes <= 0 || nbytes > EPS_BUF_SIZE) {
        bb_free(buf, EPS_BUF_SIZE);
        return NULL;
    }
    bb_seal(buf, (size_t)nbytes);
    return (bb_box_fn)buf;

#undef EPS_BUF_SIZE
}
