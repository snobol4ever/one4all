/*
 * templates/bb_xposi.c — XPOSI (POS) and XRPSI (RPOS) BB box templates.
 *
 * POS(n):  succeeds iff cursor position Δ == n.
 * RPOS(n): succeeds iff cursor position Δ == Σlen - n.
 *
 * Both are pure inline-native boxes: binary path uses load_delta /
 * cmp / jmp sequences; text path emits POS_α / RPOS_α macro invocations.
 *
 * No mode-3/mode-4 divergence: binary and text paths compute identically.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-k (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-k / GOAL-MODE4-EMIT
 */

#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

/* Σlen and Δ are globals in snobol4.c; same pattern as bb_flat.c. */
extern int Σlen;
#define ADDR_SIGLEN ((uint64_t)(uintptr_t)&Σlen)

/* emit_bb_xposi — POS(n): succeed iff Δ == n. */
void emit_bb_xposi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β,
                   bb_pos_text_fn text_body_fn, void *text_body_arg)
{
    if (!e) return;
    if (e->is_text) {
        if (text_body_fn) text_body_fn(e, n, lbl_succ, lbl_fail, lbl_β, text_body_arg);
        return;
    }
    /* Binary path: if Δ != n → fail, else → succ. β → fail (no retry). */
    emit_load_delta(e);
    emit_cmp_eax_imm32(e, (uint32_t)n);
    EMIT_JMP(e, lbl_fail, JMP_JNE);
    EMIT_JMP(e, lbl_succ, JMP_JMP);
    EMIT_LABEL(e, lbl_β);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}

/* emit_bb_xrpsi — RPOS(n): succeed iff Δ == Σlen - n. */
void emit_bb_xrpsi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β,
                   bb_pos_text_fn text_body_fn, void *text_body_arg)
{
    if (!e) return;
    if (e->is_text) {
        if (text_body_fn) text_body_fn(e, n, lbl_succ, lbl_fail, lbl_β, text_body_arg);
        return;
    }
    /* Binary path: ecx = Σlen - n; if Δ != ecx → fail, else → succ. */
    emit_load_siglen(e, ADDR_SIGLEN);    /* eax = Σlen */
    emit_sub_eax_imm32(e, (uint32_t)n); /* eax = Σlen - n */
    emit_mov_ecx_eax(e);                /* ecx = Σlen - n */
    emit_load_delta(e);                 /* eax = Δ */
    emit_cmp_eax_ecx(e);               /* cmp Δ, Σlen-n */
    EMIT_JMP(e, lbl_fail, JMP_JNE);
    EMIT_JMP(e, lbl_succ, JMP_JMP);
    EMIT_LABEL(e, lbl_β);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}
