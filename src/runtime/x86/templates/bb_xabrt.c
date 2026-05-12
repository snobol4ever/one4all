#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_abort(void *zeta, int entry);
extern abort_t *bb_abort_new(void);

void emit_bb_xabrt(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ABORT", "");
    abort_t *z = bb_abort_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_abort", (uint64_t)(uintptr_t)bb_abort,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_abort", (uint64_t)(uintptr_t)bb_abort,
                   1, lbl_succ, lbl_fail);
}
