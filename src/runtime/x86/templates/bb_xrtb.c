#include "../emitter.h"
#include "../bb_flat.h"
#include "../bb_emit.h"
#include "templates.h"

extern rtab_t *bb_rtab_new(int n);
extern DESCR_t bb_rtab(void *zeta, int entry);

void emit_bb_xrtb(emitter_t *e, long long num,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    emit_bb_intcur(e, bb_rtab, "bb_rtab", "RTAB", num, lbl_succ, lbl_fail, lbl_β);
}
