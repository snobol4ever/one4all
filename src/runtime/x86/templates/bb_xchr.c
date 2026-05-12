#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_flat.h"
#include "../bb_box.h"
#include "../snobol4_patnd.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define TEMPLATE_ADDR_SIGMA   ((uint64_t)(uintptr_t)&Σ)
#define TEMPLATE_ADDR_SIGLEN  ((uint64_t)(uintptr_t)&Σlen)

void emit_bb_xchr(emitter_t *e, PATND_t *p,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                  bb_label_t *lbl_β)
{
    const char *lit = (p && p->STRVAL_fn) ? p->STRVAL_fn : "";
    int len = (int)strlen(lit);

    if (e->is_text) {
        char preview[40];
        if (len > 24) snprintf(preview, sizeof(preview), "'%.24s...'", lit);
        else          snprintf(preview, sizeof(preview), "'%s'", lit);
        flat_emit_box_banner(e, "LIT", preview, lbl_succ->name);
    }

    emit_load_delta(e);
    emit_add_eax_imm32(e, (uint32_t)len);
    emit_cmp_eax_siglen(e, TEMPLATE_ADDR_SIGLEN);
    EMIT_JMP(e, lbl_fail, JMP_JG);

    emit_sigma_plus_delta(e, TEMPLATE_ADDR_SIGMA);
    emit_mov_rdi_rax(e);
    emit_mov_rdx_imm64(e, (uint64_t)(uint32_t)len);

    if (e->is_text && e->intern_str) {
        const char *lbl = e->intern_str(e, lit);
        bb_insn_desc_t d = {BB_INSN_LEA_RCX_SYM,
                            (uint64_t)(uintptr_t)lit, 0, 0, lbl};
        e->emit_insn(e, &d);

        e->fprintf_raw(e, "    mov     rsi, rcx\n");
    } else {
        bb_insn_desc_t d = {BB_INSN_MOV_RSI_IMM64,
                            (uint64_t)(uintptr_t)lit, 0, 0, NULL};
        e->emit_insn(e, &d);
    }

    emit_call_sym_plt(e, "memcmp", (uint64_t)(uintptr_t)memcmp);
    emit_test_eax_eax(e);
    EMIT_JMP(e, lbl_fail, JMP_JNE);

    emit_add_delta_imm(e, len);
    EMIT_JMP(e, lbl_succ, JMP_JMP);

    EMIT_LABEL(e, lbl_β);
    emit_sub_delta_imm(e, len);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}
