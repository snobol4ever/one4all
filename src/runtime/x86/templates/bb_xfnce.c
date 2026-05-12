#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_fence(void *zeta, int entry);
extern fence_t *bb_fence_new(void);

/* XFNCE with no children: bare FENCE0 box. XFNCE with child (FENCE1) is
 * handled as a composite in flat_emit_node using bb_seq; this template
 * covers only the leaf (no-child) case. */
void emit_bb_xfnce(emitter_t *e,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("FENCE", "");
    fence_t *z = bb_fence_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_fence", (uint64_t)(uintptr_t)bb_fence,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_fence", (uint64_t)(uintptr_t)bb_fence,
                   1, lbl_succ, lbl_fail);
}
