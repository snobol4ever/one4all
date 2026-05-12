#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_arb(void *zeta, int entry);
extern arb_t  *bb_arb_new(void);

void emit_bb_xfarb(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    if (!e) return;

    EMIT_OPT(e, bb_box_banner, e, "ARB", "");
    EMIT_OPT(e, comment,       e, "ARB: greedy zero-or-more via bb_arb");

    arb_t *z = bb_arb_new();

    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 0);
    emit_call_sym_plt(e, "bb_arb", (uint64_t)(uintptr_t)bb_arb);
    emit_test_rax_rax(e);
    EMIT_JMP(e, lbl_succ, JMP_JNE);
    EMIT_JMP(e, lbl_fail, JMP_JMP);

    EMIT_LABEL(e, lbl_β);
    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 1);
    emit_call_sym_plt(e, "bb_arb", (uint64_t)(uintptr_t)bb_arb);
    emit_test_rax_rax(e);
    EMIT_JMP(e, lbl_succ, JMP_JNE);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}
