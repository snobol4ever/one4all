#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_bb_xeps(emitter_t *e,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("EPS", "");
    t_emit_jmp(lbl_succ, JMP_JMP);
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
