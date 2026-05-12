#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_bb_xposi(emitter_t *e, int n,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    char args[32]; snprintf(args, sizeof(args), "%d", n);
    t_bb_box_banner("POS", args);
    t_load_delta_cmp_imm(n, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
