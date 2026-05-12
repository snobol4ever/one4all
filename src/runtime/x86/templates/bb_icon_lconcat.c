#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"
#include "../../frontend/icon/icon_gen.h"

/* TT_LCONCAT / TT_CAT: generative list/string concat — semantic ref is coro_bb_cat */
extern DESCR_t coro_bb_cat(void *zeta, int entry);
extern icn_cat_gen_state_t *icon_lconcat_new(void);

void emit_bb_icon_lconcat(emitter_t *e,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_LCONCAT", "");
    icn_cat_gen_state_t *z = icon_lconcat_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_cat",
                   (uint64_t)(uintptr_t)coro_bb_cat,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_cat",
                   (uint64_t)(uintptr_t)coro_bb_cat,
                   1, lbl_succ, lbl_fail);
}
