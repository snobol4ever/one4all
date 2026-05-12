#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_bb_xfail(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e; (void)lbl_succ;
    t_bb_box_banner("FAIL", "");
    t_emit_jmp(lbl_fail, JMP_JMP);
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
