#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_cap(void *zeta, int entry);
extern cap_t  *bb_cap_new(bb_box_fn child_fn, void *child_state,
                           const char *varname, DESCR_t *var_ptr, int immediate);

/* XFNME: pat $ var — immediate capture (immediate=1). */
void emit_bb_xfnme(emitter_t *e, bb_box_fn child_fn, const char *varname,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("CAP_IMM", varname ? varname : "");
    cap_t *z = bb_cap_new(child_fn, NULL, varname, NULL, 1 /*immediate=1*/);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_cap", (uint64_t)(uintptr_t)bb_cap,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_cap", (uint64_t)(uintptr_t)bb_cap,
                   1, lbl_succ, lbl_fail);
}
