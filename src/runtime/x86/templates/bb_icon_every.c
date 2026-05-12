#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"
#include "../../frontend/icon/icon_gen.h"

extern DESCR_t coro_bb_every(void *zeta, int entry);
extern icn_every_state_t *icon_every_new(void);

void emit_bb_icon_every(emitter_t *e,
                        bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_EVERY", "");
    icn_every_state_t *z = icon_every_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_every",
                   (uint64_t)(uintptr_t)coro_bb_every,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_every",
                   (uint64_t)(uintptr_t)coro_bb_every,
                   1, lbl_succ, lbl_fail);
}
