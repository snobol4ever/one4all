#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

extern int Σlen;
#define ADDR_SIGLEN ((uint64_t)(uintptr_t)&Σlen)

void emit_bb_xrpsi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    char args[32]; snprintf(args, sizeof(args), "%d", n);
    t_bb_box_banner("RPOS", args);
    t_load_siglen_sub_cmp_delta(n, ADDR_SIGLEN, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
