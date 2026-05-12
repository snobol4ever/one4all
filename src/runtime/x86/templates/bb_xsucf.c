#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_succeed(void *zeta, int entry);
extern succeed_t *bb_succeed_new(void);

void emit_bb_xsucf(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("SUCCEED", "");
    succeed_t *z = bb_succeed_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_succeed", (uint64_t)(uintptr_t)bb_succeed,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_succeed", (uint64_t)(uintptr_t)bb_succeed,
                   1, lbl_succ, lbl_fail);
}
