#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_cap(void *zeta, int entry);
extern cap_t  *bb_cap_new(bb_box_fn child_fn, void *child_state,
                           const char *varname, DESCR_t *var_ptr, int immediate);

/* XNME: pat . var — conditional capture (immediate=0).
 * child_fn is the compiled child pattern; varname is the target variable. */
void emit_bb_xnme(emitter_t *e, bb_box_fn child_fn, const char *varname,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("CAP_COND", varname ? varname : "");
    cap_t *z = bb_cap_new(child_fn, NULL, varname, NULL, 0 /*immediate=0*/);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_cap", (uint64_t)(uintptr_t)bb_cap,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_cap", (uint64_t)(uintptr_t)bb_cap,
                   1, lbl_succ, lbl_fail);
}
