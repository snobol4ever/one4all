/*
 * templates/bb_xbrkx.c — XBRKX (BREAKX) BB box template.
 *
 * XBRKX matches the longest prefix of the subject string that does NOT
 * contain any character in the variable charset `chars`.  At runtime,
 * `chars` is the string value at pattern-build time (baked into zeta).
 *
 * Runtime function: bb_breakx(zeta, port)
 * Zeta layout: brkx_t { const char *chars; int δ; }
 *
 * Unlike the charset-family (XSPNC/XBRKC/XANYC/XNNYC), XBRKX's zeta
 * has TWO data words (.quad chars_ptr + .long delta) not one, so it
 * gets its own template rather than sharing bb_xspnc.c.  The binary
 * path is otherwise structurally identical.
 *
 * No mode-3-vs-mode-4 divergence: both modes call bb_breakx at runtime.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-i (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-i / GOAL-MODE4-EMIT
 */

#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

/* Forward declarations — defined in bb_boxes.c / bb_box.h. */
extern DESCR_t  bb_breakx(void *zeta, int entry);
extern brkx_t  *bb_breakx_new(const char *chars);

/*
 * emit_bb_xbrkx — XBRKX box template.
 *
 * Parameters:
 *   e             — emitter (binary or text backend)
 *   chars         — charset string (NUL-terminated), baked at emit time
 *   lbl_succ      — gamma port (succeed) jump target
 *   lbl_fail      — omega port (fail) jump target
 *   lbl_β         — beta port (retry) entry label
 *   text_body_fn  — callback into bb_flat.c for text-path body
 *   text_body_arg — opaque arg forwarded to text_body_fn
 */
void emit_bb_xbrkx(emitter_t *e,
                   const char *chars,
                   bb_label_t *lbl_succ,
                   bb_label_t *lbl_fail,
                   bb_label_t *lbl_β,
                   bb_brkx_text_fn text_body_fn,
                   void *text_body_arg)
{
    if (!e) return;

    if (e->is_text) {
        if (text_body_fn) text_body_fn(e, lbl_succ, lbl_fail, lbl_β, text_body_arg);
        return;
    }

    /* ── Binary path (mode-3 in-process JIT) ────────────────────────────── */

    brkx_t *z = bb_breakx_new(chars);

    /* alpha port: call bb_breakx(z, 0) */
    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 0);
    emit_call_sym_plt(e, "bb_breakx", (uint64_t)(uintptr_t)bb_breakx);
    emit_test_rax_rax(e);
    EMIT_JMP(e, lbl_succ, JMP_JNE);
    EMIT_JMP(e, lbl_fail, JMP_JMP);

    /* beta port: call bb_breakx(z, 1) */
    EMIT_LABEL(e, lbl_β);
    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 1);
    emit_call_sym_plt(e, "bb_breakx", (uint64_t)(uintptr_t)bb_breakx);
    emit_test_rax_rax(e);
    EMIT_JMP(e, lbl_succ, JMP_JNE);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}
