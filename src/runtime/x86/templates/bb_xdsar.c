#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_deferred_var_exported(void *zeta, int entry);
extern void   *bb_dvar_bin_new(const char *name);

void emit_bb_xdsar(emitter_t *e, const char *varname,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    char banner[80]; snprintf(banner, sizeof(banner), "*%s", varname ? varname : "");
    t_bb_box_banner("DEREF", banner);
    void *z = bb_dvar_bin_new(varname ? varname : "");
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_deferred_var_exported",
                   (uint64_t)(uintptr_t)bb_deferred_var_exported,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_deferred_var_exported",
                   (uint64_t)(uintptr_t)bb_deferred_var_exported,
                   1, lbl_succ, lbl_fail);
}
