/*
 * bb_emit.c — Dual-Mode x86-64 Emitter (M-DYN-1)
 *
 * See bb_emit.h for design notes.
 */

#include "bb_emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

/* ── global state ───────────────────────────────────────────────────────── */

bb_emit_mode_t  bb_emit_mode = EMIT_TEXT;
FILE           *bb_emit_out  = NULL;   /* set by caller; defaults to stdout */

bb_buf_t        bb_emit_buf  = NULL;
int             bb_emit_pos  = 0;
int             bb_emit_size = 0;

bb_patch_t      bb_patch_list[BB_PATCH_MAX];
int             bb_patch_count = 0;

/* ── label ──────────────────────────────────────────────────────────────── */

void bb_label_init(bb_label_t *lbl, const char *name)
{
    strncpy(lbl->name, name, BB_LABEL_NAME_MAX - 1);
    lbl->name[BB_LABEL_NAME_MAX - 1] = '\0';
    lbl->offset = BB_LABEL_UNRESOLVED;
}

void bb_label_initf(bb_label_t *lbl, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(lbl->name, BB_LABEL_NAME_MAX, fmt, ap);
    va_end(ap);
    lbl->offset = BB_LABEL_UNRESOLVED;
}

void bb_label_define(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        FILE *f = bb_emit_out ? bb_emit_out : stdout;
        char lbuf[256]; snprintf(lbuf, sizeof(lbuf), "%s:", lbl->name);
        bb3c_format(f, lbuf, "", "");
        return;
    }

    /* Binary mode: record offset, patch all pending forward refs */
    lbl->offset = bb_emit_pos;

    for (int i = 0; i < bb_patch_count; i++) {
        bb_patch_t *p = &bb_patch_list[i];
        if (p->label != lbl) continue;

        int target = lbl->offset;

        if (p->kind == PATCH_REL8) {
            /* rel8: displacement = target - (site + 1) */
            int disp = target - (p->site + 1);
            if (disp < -128 || disp > 127) {
                fprintf(stderr,
                        "bb_label_define: rel8 overflow for '%s': disp=%d\n",
                        lbl->name, disp);
                abort();
            }
            bb_emit_buf[p->site] = (uint8_t)(int8_t)disp;
        } else {
            /* rel32: displacement = target - (site + 4) */
            int disp = target - (p->site + 4);
            uint32_t u;
            memcpy(&u, &disp, 4);
            bb_emit_buf[p->site + 0] = (uint8_t)(u      );
            bb_emit_buf[p->site + 1] = (uint8_t)(u >>  8);
            bb_emit_buf[p->site + 2] = (uint8_t)(u >> 16);
            bb_emit_buf[p->site + 3] = (uint8_t)(u >> 24);
        }

        /* Remove from patch list (swap with last) */
        bb_patch_list[i] = bb_patch_list[--bb_patch_count];
        i--;   /* re-check this slot */
    }
}

/* ── session ────────────────────────────────────────────────────────────── */

void bb_emit_begin(bb_buf_t buf, int size)
{
    bb_emit_buf   = buf;
    bb_emit_pos   = 0;
    bb_emit_size  = size;
    bb_patch_count = 0;
}

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

/* ── patch helpers ──────────────────────────────────────────────────────── */

void bb_emit_patch_rel8(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        /* EM-7c-bb-three-column: a direct rel8 patch from TEXT mode
         * means the emitter is in BINARY-byte mode by mistake.  TEXT
         * jumps go via bb_insn_*_rel8 / EV_JMP which emit "jmp <name>"
         * by mnemonic. */
        fprintf(stderr,
                "bb_emit_patch_rel8: TEXT-mode reach (target='%s') — "
                "use bb_insn_*_rel8 mnemonic helpers\n",
                lbl->name);
        abort();
    }
    if (bb_label_defined(lbl)) {
        /* Already resolved — emit directly */
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
    /* Unresolved: record patch site, emit placeholder */
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

void bb_emit_patch_rel32(bb_label_t *lbl)
{
    if (bb_emit_mode == EMIT_TEXT) {
        /* EM-7c-bb-three-column: a direct rel32 patch from TEXT mode
         * means the emitter is in BINARY-byte mode by mistake.  TEXT
         * jumps go via bb_insn_*_rel32 / EV_JMP which emit "jmp <name>"
         * by mnemonic. */
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

/* ── byte primitives ────────────────────────────────────────────────────── */

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

void bb_emit_byte(uint8_t b)
{
    if (bb_emit_mode == EMIT_TEXT) {
        /* EM-7c-bb-three-column: TEXT mode never emits .byte hex walls.
         * Every reachable instruction routes through a named bb_insn_*
         * helper (mnemonic) or emitter_v->emit_insn (mnemonic).  A
         * direct bb_emit_byte from TEXT mode is a bug — caller is
         * leaking BINARY-mode byte knowledge into the text path. */
        fprintf(stderr,
                "bb_emit_byte: TEXT-mode reach (b=0x%02x) — "
                "convert caller to a named bb_insn_* helper\n",
                (unsigned)b);
        abort();
    }
    if (bb_emit_pos >= bb_emit_size) {
        fprintf(stderr, "bb_emit_byte: buffer overflow at pos=%d size=%d\n",
                bb_emit_pos, bb_emit_size);
        abort();
    }
    bb_emit_buf[bb_emit_pos++] = b;
}

void bb_emit_u16(uint16_t v)
{
    bb_emit_byte((uint8_t)(v     ));
    bb_emit_byte((uint8_t)(v >> 8));
}

void bb_emit_u32(uint32_t v)
{
    bb_emit_byte((uint8_t)(v      ));
    bb_emit_byte((uint8_t)(v >>  8));
    bb_emit_byte((uint8_t)(v >> 16));
    bb_emit_byte((uint8_t)(v >> 24));
}

void bb_emit_u64(uint64_t v)
{
    bb_emit_u32((uint32_t)(v      ));
    bb_emit_u32((uint32_t)(v >> 32));
}

void bb_emit_i8(int8_t v)   { bb_emit_byte((uint8_t)v); }
void bb_emit_i32(int32_t v) { uint32_t u; memcpy(&u, &v, 4); bb_emit_u32(u); }

/* ── text mode helpers ───────────────────────────────────────────────────── */

void bb_text(const char *fmt, ...)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
}

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
 * sm_emit_template's `render_call_line`) — one printf format
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
static int bb3c_visual_width(const char *s)
{
    int w = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if ((*p & 0xC0) != 0x80) w++;
    }
    return w;
}

/* Write s to buf, then pad with spaces so the visual width reaches `target`.
 * Returns number of bytes written to buf. */
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

static void bb3c_write_line(FILE *out, const char *L, const char *A, const char *G)
{
    /* EM-7c-no-trailing-ws: build + right-trim.
     * EM-FORMAT-BB UTF-8 visual width: pad on visual columns, not bytes,
     * so labels and macros containing α/β/γ/ω align correctly with ASCII.
     * Cols: 24 for label, 16 for action.  Single space then col-3. */
    char buf[768];
    int o = 0;
    o += bb3c_pad_to_width(buf + o, sizeof(buf) - o, L ? L : "", 24);
    o += bb3c_pad_to_width(buf + o, sizeof(buf) - o, A ? A : "", 16);
    /* Single space between col-2 and col-3 (matches prior format) */
    if (o < (int)sizeof(buf) - 1) buf[o++] = ' ';
    if (G && *G) {
        int gl = (int)strlen(G);
        if (gl > (int)sizeof(buf) - 1 - o) gl = (int)sizeof(buf) - 1 - o;
        memcpy(buf + o, G, (size_t)gl);
        o += gl;
    }
    /* Right-trim trailing whitespace before the newline. */
    while (o > 0 && (buf[o-1] == ' ' || buf[o-1] == '\t')) o--;
    buf[o] = '\0';
    fputs(buf, out);
    fputc('\n', out);
}

/* EM-FORMAT-BB-FUSED-GOTOS: flush deferred conditional jmp as standalone
 * (col 1 empty, col 2 = jne/je/jl/jge/jg, col 3 = target).  Writes via
 * the low-level `bb3c_write_line` to avoid recursing through
 * `bb3c_format`'s pending-label path.  Called from bb3c_format at its
 * top so the cond-jmp lands BEFORE any subsequent label/action/banner. */
static void bb3c_flush_pending_cond_jmp(void)
{
    if (g_bb3c_pending_cjmp_mn[0] && g_bb3c_pending_cjmp_out) {
        /* EM-FORMAT-BB-LAW: standalone cond-jmp (no uncond partner).
         * col-2 = cond mnemonic, col-3 = target, col-4 empty. */
        bb3c_write_line(g_bb3c_pending_cjmp_out, "",
                        g_bb3c_pending_cjmp_mn,
                        g_bb3c_pending_cjmp_target);
        g_bb3c_pending_cjmp_mn[0]     = '\0';
        g_bb3c_pending_cjmp_target[0] = '\0';
        g_bb3c_pending_cjmp_out       = NULL;
    }
}

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

/* EM-FORMAT-BB-FUSED-GOTOS: flush only the deferred cond-jmp (NOT the
 * pending label).  Used by raw-write paths (banners, EV_TEXT) that
 * physically write to the FILE* without going through bb3c_format —
 * they MUST land AFTER any cond-jmp emission, but the pending label is
 * a different concern (handled by bb3c_format's own logic when the
 * next bb3c content arrives).  Symmetric with bb3c_flush_pending which
 * flushes the label only. */
void bb3c_flush_pending_cjmp_only(void)
{
    bb3c_flush_pending_cond_jmp();
}

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

void bb3c_emit_jmp(FILE *out, const char *mn, const char *target)
{
    const char *m = mn ? mn : "";
    const char *t = target ? target : "";

    if (bb3c_is_cond_jmp(m)) {
        /* Stream change: flush any pending cjmp on prior stream first. */
        if (g_bb3c_pending_cjmp_mn[0] && g_bb3c_pending_cjmp_out != out) {
            bb3c_flush_pending_cond_jmp();
        }
        /* Two cond-jmps in a row (no intervening uncond): the prior one
         * must flush standalone — it will not get fused. */
        if (g_bb3c_pending_cjmp_mn[0]) {
            bb3c_flush_pending_cond_jmp();
        }
        /* Defer this one. */
        snprintf(g_bb3c_pending_cjmp_mn,     sizeof(g_bb3c_pending_cjmp_mn),     "%s", m);
        snprintf(g_bb3c_pending_cjmp_target, sizeof(g_bb3c_pending_cjmp_target), "%s", t);
        g_bb3c_pending_cjmp_out = out;
        return;
    }

    /* Unconditional jmp (or any non-cond mnemonic that arrived through here). */
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
        /* Pad col3 to BB_COL3_WIDTH visual cols.  bb3c_pad_to_width handles
         * UTF-8 multi-byte codepoints (Greek labels) correctly. */
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
    /* No pending: standalone unconditional jmp.  EM-FORMAT-BB-LAW (4-column):
     * Bare label+jmp / trampoline:
     *   col-2 empty, col-3 empty (padded to 27 visual cols), col-4 = "jmp <target>".
     * The `jmp` lands at file vcol 68 (= col-2 16 + 1 sep + col-3 27 + 0 because
     * col-3 has no content — but we pad it out so col-4 starts at the anchor). */
    char rest[512];
    int o = bb3c_pad_to_width(rest, sizeof(rest), "", BB_COL3_WIDTH);
    snprintf(rest + o, sizeof(rest) - o, "%s %s", m, t);
    bb3c_format(out, "", "", rest);
}

void bb3c_format(FILE *out, const char *label, const char *action, const char *goto_)
{
    /* EM-FORMAT-BB-FUSED-GOTOS: any non-jmp content arriving here means
     * a deferred cond-jmp (if any) cannot fuse and must flush standalone. */
    if (g_bb3c_pending_cjmp_mn[0]) {
        if (g_bb3c_pending_cjmp_out == out || g_bb3c_pending_cjmp_out == NULL) {
            bb3c_flush_pending_cond_jmp();
        } else {
            /* Different stream: flush to original stream first. */
            bb3c_flush_pending_cond_jmp();
        }
    }

    const char *L = label  ? label  : "";
    const char *A = action ? action : "";
    const char *G = goto_  ? goto_  : "";

    int label_only = (*L) && !(*A) && !(*G);
    int has_content = (*A) || (*G);

    /* If switching streams mid-buffer, flush to the old stream first. */
    if (g_bb3c_pending_label[0] && g_bb3c_pending_out && g_bb3c_pending_out != out) {
        bb3c_write_line(g_bb3c_pending_out, g_bb3c_pending_label, "", "");
        g_bb3c_pending_label[0] = '\0';
        g_bb3c_pending_out = NULL;
    }

    if (label_only) {
        /* Label-only emission: flush any prior pending label as standalone
         * (multi-label chain), then buffer this one. */
        if (g_bb3c_pending_label[0]) {
            bb3c_write_line(out, g_bb3c_pending_label, "", "");
        }
        snprintf(g_bb3c_pending_label, sizeof(g_bb3c_pending_label), "%s", L);
        g_bb3c_pending_out = out;
        return;
    }

    if (has_content) {
        /* Snapshot pending label to a local buffer BEFORE clearing the
         * static, otherwise eff_L would be a dangling reference once the
         * static is zeroed. */
        char fused_lbl[256];
        const char *eff_L = L;
        if (g_bb3c_pending_label[0]) {
            if (!*eff_L) {
                /* Caller didn't supply col-1; fuse pending label into this line. */
                snprintf(fused_lbl, sizeof(fused_lbl), "%s", g_bb3c_pending_label);
                eff_L = fused_lbl;
            } else {
                /* Caller supplied own col-1 AND we have pending — flush pending
                 * standalone, keep caller's col-1. */
                bb3c_write_line(out, g_bb3c_pending_label, "", "");
            }
            g_bb3c_pending_label[0] = '\0';
            g_bb3c_pending_out = NULL;
        }
        bb3c_write_line(out, eff_L, A, G);
        return;
    }

    /* All-empty call: no-op (shouldn't happen, but defensively safe). */
}

void bb3c_text(const char *label, const char *action, const char *goto_)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    bb3c_format(f, label, action, goto_);
}

/* ── x86-64 instruction helpers ─────────────────────────────────────────── */

/* EM-7c-bb-three-column-split: split (mnemonic, args) emission.
 * Col 1 empty, col 2 = mnemonic ONLY (16-wide), col 3 = formatted args.
 * Replaces the prior bb3c_action which fused mnemonic+args into col 2 and
 * overflowed the 16-wide field on every load-immediate / memory-op line. */
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

/* Goto-only line: col 1 empty, col 2 = jmp/je/jne/..., col 3 = target.
 * EM-FORMAT-BB-FUSED-GOTOS: routes through bb3c_emit_jmp so a conditional
 * jmp followed immediately by an unconditional jmp fuses onto one line. */
static void bb3c_jmp(const char *mn, const char *target)
{
    if (bb_emit_mode != EMIT_TEXT) return;
    FILE *f = bb_emit_out ? bb_emit_out : stdout;
    bb3c_emit_jmp(f, mn ? mn : "", target ? target : "");
}

void bb_insn_mov_eax_imm32(uint32_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("mov", "eax, %u", imm);
        return;
    }
    bb_emit_byte(0xB8);
    bb_emit_u32(imm);
}

void bb_insn_mov_rax_imm64(uint64_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("mov", "rax, 0x%llx", (unsigned long long)imm);
        return;
    }
    /* REX.W + B8 /r  — mov rax, imm64 */
    bb_emit_byte(0x48);
    bb_emit_byte(0xB8);
    bb_emit_u64(imm);
}

void bb_insn_ret(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("ret", NULL); return; }
    bb_emit_byte(0xC3);
}

void bb_insn_nop(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("nop", NULL); return; }
    bb_emit_byte(0x90);
}

void bb_insn_call_rax(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("call", "rax"); return; }
    bb_emit_byte(0xFF); bb_emit_byte(0xD0);
}

void bb_insn_jmp_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jmp", target->name); return;
    }
    bb_emit_byte(0xEB);
    bb_emit_patch_rel8(target);
}

void bb_insn_jmp_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jmp", target->name); return;
    }
    bb_emit_byte(0xE9);
    bb_emit_patch_rel32(target);
}

void bb_insn_jl_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jl", target->name); return;
    }
    bb_emit_byte(0x7C);
    bb_emit_patch_rel8(target);
}

void bb_insn_jge_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jge", target->name); return;
    }
    bb_emit_byte(0x7D);
    bb_emit_patch_rel8(target);
}

void bb_insn_je_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("je", target->name); return;
    }
    bb_emit_byte(0x74);
    bb_emit_patch_rel8(target);
}

void bb_insn_jne_rel8(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jne", target->name); return;
    }
    bb_emit_byte(0x75);
    bb_emit_patch_rel8(target);
}

void bb_insn_jne_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jne", target->name); return;
    }
    bb_emit_byte(0x0F); bb_emit_byte(0x85);
    bb_emit_patch_rel32(target);
}

/* jg rel32 — jump if greater (signed), near, forward ref (EM-7b) */
void bb_insn_jg_rel32(bb_label_t *target)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_jmp("jg", target->name); return;
    }
    bb_emit_byte(0x0F); bb_emit_byte(0x8F);
    bb_emit_patch_rel32(target);
}

void bb_insn_cmp_esi_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("cmp", "esi, %u", (unsigned)imm); return;
    }
    bb_emit_byte(0x83); bb_emit_byte(0xFE); bb_emit_byte(imm);
}

void bb_insn_cmp_esi_imm32(uint32_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("cmp", "esi, %u", imm); return;
    }
    bb_emit_byte(0x81); bb_emit_byte(0xFE); bb_emit_u32(imm);
}

void bb_insn_movzx_eax_rdi_off8(uint8_t off)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("movzx", "eax, byte [rdi + %u]", (unsigned)off); return;
    }
    /* 0F B6 47 imm8 */
    bb_emit_byte(0x0F); bb_emit_byte(0xB6);
    bb_emit_byte(0x47); bb_emit_byte(off);
}

void bb_insn_cmp_al_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("cmp", "al, %u", (unsigned)imm); return;
    }
    bb_emit_byte(0x3C); bb_emit_byte(imm);
}

void bb_insn_xor_eax_eax(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("xor", "eax, eax"); return; }
    bb_emit_byte(0x31); bb_emit_byte(0xC0);
}

void bb_insn_push_rbp(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("push", "rbp"); return; }
    bb_emit_byte(0x55);
}

void bb_insn_pop_rbp(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("pop", "rbp"); return; }
    bb_emit_byte(0x5D);
}

void bb_insn_mov_rbp_rsp(void)
{
    if (bb_emit_mode == EMIT_TEXT) { bb3c_op("mov", "rbp, rsp"); return; }
    /* REX.W + 89 /r  — mov rbp, rsp  (89 E5) */
    bb_emit_byte(0x48); bb_emit_byte(0x89); bb_emit_byte(0xE5);
}

void bb_insn_sub_rsp_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("sub", "rsp, %u", (unsigned)imm); return;
    }
    bb_emit_byte(0x48); bb_emit_byte(0x83); bb_emit_byte(0xEC);
    bb_emit_byte(imm);
}

void bb_insn_add_rsp_imm8(uint8_t imm)
{
    if (bb_emit_mode == EMIT_TEXT) {
        bb3c_op("add", "rsp, %u", (unsigned)imm); return;
    }
    bb_emit_byte(0x48); bb_emit_byte(0x83); bb_emit_byte(0xC4);
    bb_emit_byte(imm);
}
