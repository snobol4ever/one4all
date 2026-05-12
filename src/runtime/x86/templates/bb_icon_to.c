#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"
#include "../../frontend/icon/icon_gen.h"

extern DESCR_t coro_bb_to   (void *zeta, int entry);
extern DESCR_t coro_bb_to_by(void *zeta, int entry);
extern icn_to_state_t    *icon_to_new(void);
extern icn_to_by_state_t *icon_to_by_new(void);

void emit_bb_icon_to(emitter_t *e,
                     bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_TO", "");
    icn_to_state_t *z = icon_to_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_to", (uint64_t)(uintptr_t)coro_bb_to,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_to", (uint64_t)(uintptr_t)coro_bb_to,
                   1, lbl_succ, lbl_fail);
}

void emit_bb_icon_to_by(emitter_t *e,
                        bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_TO_BY", "");
    icn_to_by_state_t *z = icon_to_by_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_to_by", (uint64_t)(uintptr_t)coro_bb_to_by,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_to_by", (uint64_t)(uintptr_t)coro_bb_to_by,
                   1, lbl_succ, lbl_fail);
}
