/*
 * templates/bb_xfarb.c — XFARB (ARB), XEPS (EPS/epsilon), XFAIL (FAIL) templates.
 *
 * Three degenerate boxes with no operands:
 *
 * XEPS  — always succeeds on alpha; beta → fail. Binary: jmp succ; β: jmp fail.
 * XFAIL — always fails.   Binary: jmp fail; β: jmp fail.
 * XFARB — ARB (match zero or more chars). Needs arb_t zeta; calls bb_arb(z,port).
 *
 * No mode-3/mode-4 divergence: all binary paths use inline jumps or PLT calls.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-m (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-m / GOAL-MODE4-EMIT
 */

#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"   /* arb_t */
#include "templates.h"

extern DESCR_t bb_arb(void *zeta, int entry);
extern arb_t  *bb_arb_new(void);

/* emit_bb_xeps — EPS: always succeed on alpha; beta fails. */
void emit_bb_xeps(emitter_t *e,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β,
                  bb_nullary_text_fn text_body_fn, void *text_body_arg)
{
    if (!e) return;
    if (e->is_text) {
        if (text_body_fn) text_body_fn(e, lbl_succ, lbl_fail, lbl_β, text_body_arg);
        return;
    }
    EV_JMP(e, lbl_succ, JMP_JMP);
    EV_LABEL(e, lbl_β);
    EV_JMP(e, lbl_fail, JMP_JMP);
}

/* emit_bb_xfail — FAIL: always fail (both ports). */
void emit_bb_xfail(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β,
                   bb_nullary_text_fn text_body_fn, void *text_body_arg)
{
    (void)lbl_succ;
    if (!e) return;
    if (e->is_text) {
        if (text_body_fn) text_body_fn(e, lbl_succ, lbl_fail, lbl_β, text_body_arg);
        return;
    }
    EV_JMP(e, lbl_fail, JMP_JMP);
    EV_LABEL(e, lbl_β);
    EV_JMP(e, lbl_fail, JMP_JMP);
}

/* emit_bb_xfarb — ARB: greedy zero-or-more via bb_arb(z, port). */
void emit_bb_xfarb(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β,
                   bb_nullary_text_fn text_body_fn, void *text_body_arg)
{
    if (!e) return;
    if (e->is_text) {
        if (text_body_fn) text_body_fn(e, lbl_succ, lbl_fail, lbl_β, text_body_arg);
        return;
    }
    arb_t *z = bb_arb_new();
    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 0);
    emit_call_sym_plt(e, "bb_arb", (uint64_t)(uintptr_t)bb_arb);
    emit_test_rax_rax(e);
    EV_JMP(e, lbl_succ, JMP_JNE);
    EV_JMP(e, lbl_fail, JMP_JMP);
    EV_LABEL(e, lbl_β);
    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 1);
    emit_call_sym_plt(e, "bb_arb", (uint64_t)(uintptr_t)bb_arb);
    emit_test_rax_rax(e);
    EV_JMP(e, lbl_succ, JMP_JNE);
    EV_JMP(e, lbl_fail, JMP_JMP);
}
