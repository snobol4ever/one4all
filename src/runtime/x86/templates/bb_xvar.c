#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_bb_xvar(emitter_t *e,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    /* XVAR is never flat-eligible (runtime DESCR_t as pattern — graph unknown).
     * Emit β→fail stub; flat_is_eligible returns 0 for XVAR so this is
     * reached only from the binary C-fallback path, not from flat emission. */
    (void)e; (void)lbl_succ;
    t_bb_box_banner("VAR", "");
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
