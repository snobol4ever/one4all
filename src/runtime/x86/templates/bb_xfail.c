#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

void emit_bb_xfail(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)lbl_succ;
    if (!e) return;

    EMIT_OPT(e, bb_box_banner, e, "FAIL", "");
    EMIT_OPT(e, comment,       e, "FAIL: always fail");

    EMIT_JMP(e, lbl_fail, JMP_JMP);

    EMIT_LABEL(e, lbl_β);
    EMIT_JMP(e, lbl_fail, JMP_JMP);
}
