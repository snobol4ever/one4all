#include "../emitter.h"
#include "../bb_flat.h"
#include "../bb_emit.h"
#include "templates.h"

extern len_t  *bb_len_new (int n);
extern tab_t  *bb_tab_new (int n);
extern rtab_t *bb_rtab_new(int n);

extern DESCR_t bb_len  (void *zeta, int entry);
extern DESCR_t bb_tab  (void *zeta, int entry);
extern DESCR_t bb_rtab (void *zeta, int entry);

void emit_bb_intcur(emitter_t *e,
                    bb_box_fn c_fn,
                    const char *c_fn_name,
                    const char *kind_name,
                    long long num,
                    bb_label_t *lbl_succ,
                    bb_label_t *lbl_fail,
                    bb_label_t *lbl_β)
{
    (void)kind_name; (void)e;
    void *z;
    if      (c_fn == bb_len)  z = bb_len_new ((int)num);
    else if (c_fn == bb_tab)  z = bb_tab_new ((int)num);
    else if (c_fn == bb_rtab) z = bb_rtab_new((int)num);
    else {
        int *raw = calloc(2, sizeof(int));
        raw[0] = (int)num;
        z = raw;
    }
    t_bb_port_call((uint64_t)(uintptr_t)z, c_fn_name, (uint64_t)(uintptr_t)c_fn,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, c_fn_name, (uint64_t)(uintptr_t)c_fn,
                   1, lbl_succ, lbl_fail);
}

void emit_bb_xlnth(emitter_t *e, long long num,
                   bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    emit_bb_intcur(e, bb_len, "bb_len", "LEN", num, lbl_succ, lbl_fail, lbl_β);
}
