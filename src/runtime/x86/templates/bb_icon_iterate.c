#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"
#include "../../frontend/icon/icon_gen.h"

extern DESCR_t coro_bb_iterate    (void *zeta, int entry);
extern DESCR_t coro_bb_list_iterate(void *zeta, int entry);
extern DESCR_t coro_bb_tbl_iterate (void *zeta, int entry);
extern DESCR_t coro_bb_record_iterate(void *zeta, int entry);

extern icn_iterate_state_t     *icon_iterate_new(void);
extern icn_list_iterate_state_t *icon_list_iterate_new(void);
extern icn_tbl_iterate_state_t  *icon_tbl_iterate_new(void);
extern icn_record_iterate_state_t *icon_record_iterate_new(void);

/* String / generic !E — runtime dispatches on subject type via coro_bb_iterate */
void emit_bb_icon_iterate(emitter_t *e,
                          bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ICN_ITERATE", "");
    icn_iterate_state_t *z = icon_iterate_new();
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_iterate",
                   (uint64_t)(uintptr_t)coro_bb_iterate,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "coro_bb_iterate",
                   (uint64_t)(uintptr_t)coro_bb_iterate,
                   1, lbl_succ, lbl_fail);
}
