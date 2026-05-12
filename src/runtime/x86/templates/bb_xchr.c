#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "../snobol4_patnd.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define TEMPLATE_ADDR_SIGMA   ((uint64_t)(uintptr_t)&Σ)
#define TEMPLATE_ADDR_SIGLEN  ((uint64_t)(uintptr_t)&Σlen)

extern const char *Σ;
extern int         Σlen;

static void t_mov_rdx_imm32(int v)
{
    uint64_t val = (uint64_t)(uint32_t)v;
    switch (bb_emit_mode) {
    case EMIT_BINARY_WIRED:
    case EMIT_BINARY_BROKERED:  /* EM-BB-PURGE-1 pending */
        /* mov rdx, imm64 — 48 BA <8> (zero-extended from imm32) */
        bb_emit_byte(0x48); bb_emit_byte(0xBA);
        bb_emit_byte((uint8_t)(val      )); bb_emit_byte((uint8_t)(val >>  8));
        bb_emit_byte((uint8_t)(val >> 16)); bb_emit_byte((uint8_t)(val >> 24));
        bb_emit_byte(0); bb_emit_byte(0); bb_emit_byte(0); bb_emit_byte(0);
        return;
    case EMIT_TEXT:
    case EMIT_MACRO_DEF: {
        char args[32]; snprintf(args, sizeof(args), "rdx, %d", v);
        bb3c_format(bb_emit_out ? bb_emit_out : stdout, "", "mov", args);
        return;
    }
    }
}

void emit_bb_xchr(emitter_t *e, PATND_t *p,
                  const char *lit_label,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                  bb_label_t *lbl_β)
{
    (void)e;
    const char *lit = (p && p->STRVAL_fn) ? p->STRVAL_fn : "";
    int len = (int)strlen(lit);

    char preview[40];
    if (len > 24) snprintf(preview, sizeof(preview), "'%.24s...'", lit);
    else          snprintf(preview, sizeof(preview), "'%s'", lit);
    t_bb_box_banner("LIT", preview);

    /* Bounds: Δ + len <= Σlen */
    t_bounds_check_delta_plus_len(len, TEMPLATE_ADDR_SIGLEN, lbl_fail);

    /* rdi = Σ + Δ  (subject pointer at cursor) */
    t_sigma_plus_delta_to_rdi(TEMPLATE_ADDR_SIGMA, TEMPLATE_ADDR_SIGLEN);

    /* rsi = &lit  (pattern string) */
    t_lea_rsi_strtab_sym(lit_label, (uint64_t)(uintptr_t)lit);

    /* rdx = len */
    t_mov_rdx_imm32(len);

    /* call memcmp(rdi=subject, rsi=lit, rdx=len) */
    t_call_sym_plt("memcmp", (uint64_t)(uintptr_t)memcmp);
    t_test_eax_eax();
    t_emit_jmp(lbl_fail, JMP_JNE);

    /* α success: advance cursor */
    t_add_delta_imm(len);
    t_emit_jmp(lbl_succ, JMP_JMP);

    /* β: restore cursor (undo prior advance), fail */
    t_label_define(lbl_β);
    t_sub_delta_imm(len);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
