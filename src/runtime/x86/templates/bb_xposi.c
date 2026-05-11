/*
 * templates/bb_xposi.c — XPOSI (POS) and XRPSI (RPOS) BB box templates.
 *
 * POS(n):  succeeds iff cursor Δ == n.
 * RPOS(n): succeeds iff cursor Δ == Σlen - n.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-k (GOAL-MODE4-EMIT).
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

extern int Σlen;
#define ADDR_SIGLEN ((uint64_t)(uintptr_t)&Σlen)

void emit_bb_xposi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    if (!e) return;

    char args[32]; snprintf(args, sizeof(args), "%d", n);
    EMIT_OPT(e, bb_box_banner, e, "POS", args);
    EMIT_OPT(e, comment,       e, "POS(n): succeed iff Δ == n");

    emit_load_delta(e);                        /* eax = Δ */
    emit_cmp_eax_imm32(e, (uint32_t)n);       /* cmp Δ, n */
    EMIT_JMP(e, lbl_fail, JMP_JNE);           /* Δ != n → ω */
    EMIT_JMP(e, lbl_succ, JMP_JMP);           /* Δ == n → γ */

    EMIT_LABEL(e, lbl_β);
    EMIT_JMP(e, lbl_fail, JMP_JMP);           /* β → ω (no retry) */
}

void emit_bb_xrpsi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    if (!e) return;

    char args[32]; snprintf(args, sizeof(args), "%d", n);
    EMIT_OPT(e, bb_box_banner, e, "RPOS", args);
    EMIT_OPT(e, comment,       e, "RPOS(n): succeed iff Δ == Σlen - n");

    emit_load_siglen(e, ADDR_SIGLEN);          /* eax = Σlen */
    emit_sub_eax_imm32(e, (uint32_t)n);       /* eax = Σlen - n */
    emit_mov_ecx_eax(e);                       /* ecx = Σlen - n */
    emit_load_delta(e);                        /* eax = Δ */
    emit_cmp_eax_ecx(e);                       /* cmp Δ, Σlen-n */
    EMIT_JMP(e, lbl_fail, JMP_JNE);           /* Δ != target → ω */
    EMIT_JMP(e, lbl_succ, JMP_JMP);           /* Δ == target → γ */

    EMIT_LABEL(e, lbl_β);
    EMIT_JMP(e, lbl_fail, JMP_JMP);           /* β → ω (no retry) */
}
