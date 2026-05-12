#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

extern DESCR_t  bb_breakx(void *zeta, int entry);
extern brkx_t  *bb_breakx_new(const char *chars);

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

    brkx_t *z = bb_breakx_new(chars);

    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 0);
    emit_call_sym_plt(e, "bb_breakx", (uint64_t)(uintptr_t)bb_breakx);
    emit_test_rax_rax(e);
    EMIT_JMP(e, lbl_succ, JMP_JNE);
    EMIT_JMP(e, lbl_fail, JMP_JMP);

    EMIT_LABEL(e, lbl_β);
    emit_mov_rdi_imm64(e, (uint64_t)(uintptr_t)z);
    emit_mov_esi_imm32(e, 1);
    emit_call_sym_plt(e, "bb_breakx", (uint64_t)(uintptr_t)bb_breakx);
    emit_test_rax_rax(e);
    EMIT_JMP(e, lbl_succ, JMP_JNE);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}
