#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_arbno(void *zeta, int entry);
/* bb_arbno_new returns a heap-allocated arbno_t* — type is private to bb_boxes.c */
extern void   *bb_arbno_new(bb_box_fn fn, void *state);

void emit_bb_xarbn(emitter_t *e, bb_box_fn child_fn,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ARBNO", "");
    void *z = bb_arbno_new(child_fn, NULL);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_arbno", (uint64_t)(uintptr_t)bb_arbno,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_arbno", (uint64_t)(uintptr_t)bb_arbno,
                   1, lbl_succ, lbl_fail);
}
