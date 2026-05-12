#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_arb(void *zeta, int entry);
extern arb_t  *bb_arb_new(void);

void emit_bb_xfarb(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ARB", "");
    arb_t *z = bb_arb_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_arb", (uint64_t)(uintptr_t)bb_arb,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_arb", (uint64_t)(uintptr_t)bb_arb,
                   1, lbl_succ, lbl_fail);
}
