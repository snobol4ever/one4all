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

    emit_load_delta(e);
    emit_cmp_eax_imm32(e, (uint32_t)n);
    EMIT_JMP(e, lbl_fail, JMP_JNE);
    EMIT_JMP(e, lbl_succ, JMP_JMP);

    EMIT_LABEL(e, lbl_β);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}

void emit_bb_xrpsi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    if (!e) return;

    char args[32]; snprintf(args, sizeof(args), "%d", n);
    EMIT_OPT(e, bb_box_banner, e, "RPOS", args);
    EMIT_OPT(e, comment,       e, "RPOS(n): succeed iff Δ == Σlen - n");

    emit_load_siglen(e, ADDR_SIGLEN);
    emit_sub_eax_imm32(e, (uint32_t)n);
    emit_mov_ecx_eax(e);
    emit_load_delta(e);
    emit_cmp_eax_ecx(e);
    EMIT_JMP(e, lbl_fail, JMP_JNE);
    EMIT_JMP(e, lbl_succ, JMP_JMP);

    EMIT_LABEL(e, lbl_β);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}
