#include "../emitter.h"
#include "../bb_emit.h"
#include "../bb_box.h"
#include "templates.h"

extern DESCR_t bb_cap(void *zeta, int entry);
extern cap_t  *bb_cap_new_call(bb_box_fn child_fn, void *child_state,
                                const char *fnc_name,
                                DESCR_t *fnc_args, int fnc_nargs,
                                char **fnc_arg_names, int fnc_n_arg_names,
                                int immediate);

/* XCALLCAP: pat . *func() — deferred-call capture. */
void emit_bb_xcallcap(emitter_t *e, bb_box_fn child_fn,
                      const char *fnc_name,
                      bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("CALLCAP", fnc_name ? fnc_name : "");
    cap_t *z = bb_cap_new_call(child_fn, NULL, fnc_name,
                                NULL, 0, NULL, 0, 0 /*deferred*/);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_cap", (uint64_t)(uintptr_t)bb_cap,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_cap", (uint64_t)(uintptr_t)bb_cap,
                   1, lbl_succ, lbl_fail);
}
