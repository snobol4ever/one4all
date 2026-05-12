#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_atp(void *zeta, int entry);
extern atp_t  *bb_atp_new(const char *varname);

void emit_bb_xatp(emitter_t *e, const char *varname,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("USERPAT", varname ? varname : "");
    atp_t *z = bb_atp_new(varname ? varname : "");
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_atp", (uint64_t)(uintptr_t)bb_atp,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_atp", (uint64_t)(uintptr_t)bb_atp,
                   1, lbl_succ, lbl_fail);
}
