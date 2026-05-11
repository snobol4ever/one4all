/*
 * templates/bb_xfarb.c — XFARB (ARB), XEPS (EPS), XFAIL (FAIL) templates.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-m (GOAL-MODE4-EMIT).
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_arb(void *zeta, int entry);
extern arb_t  *bb_arb_new(void);

void emit_bb_xeps(emitter_t *e,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    if (!e) return;

    EMIT_OPT(e, bb_box_banner, e, "EPS", "");
    EMIT_OPT(e, comment,       e, "EPS: always succeed");

    EMIT_JMP(e, lbl_succ, JMP_JMP);           /* α → γ always */

    EMIT_LABEL(e, lbl_β);
    EMIT_JMP(e, lbl_fail, JMP_JMP);           /* β → ω (no retry) */
}

void emit_bb_xfail(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)lbl_succ;
    if (!e) return;

    EMIT_OPT(e, bb_box_banner, e, "FAIL", "");
    EMIT_OPT(e, comment,       e, "FAIL: always fail");

    EMIT_JMP(e, lbl_fail, JMP_JMP);           /* α → ω always */

    EMIT_LABEL(e, lbl_β);
    EMIT_JMP(e, lbl_fail, JMP_JMP);           /* β → ω */
}

void emit_bb_xfarb(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    if (!e) return;

    EMIT_OPT(e, bb_box_banner, e, "ARB", "");
    EMIT_OPT(e, comment,       e, "ARB: greedy zero-or-more via bb_arb");

    arb_t *z = bb_arb_new();

    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);  /* rdi = &zeta */
    emit_mov_esi_imm32(e, 0);                         /* esi = 0 (α) */
    emit_call_sym_plt(e, "bb_arb", (uint64_t)(uintptr_t)bb_arb);
    emit_test_rax_rax(e);                             /* test result */
    EMIT_JMP(e, lbl_succ, JMP_JNE);                  /* nonzero → γ */
    EMIT_JMP(e, lbl_fail, JMP_JMP);                  /* zero → ω */

    EMIT_LABEL(e, lbl_β);
    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);  /* rdi = &zeta */
    emit_mov_esi_imm32(e, 1);                         /* esi = 1 (β) */
    emit_call_sym_plt(e, "bb_arb", (uint64_t)(uintptr_t)bb_arb);
    emit_test_rax_rax(e);                             /* test result */
    EMIT_JMP(e, lbl_succ, JMP_JNE);                  /* nonzero → γ */
    EMIT_JMP(e, lbl_fail, JMP_JMP);                  /* zero → ω */
}
