#include "../emitter.h"
#include "../bb_flat.h"
#include "../bb_emit.h"
#include "templates.h"

extern tab_t  *bb_tab_new(int n);
extern DESCR_t bb_tab(void *zeta, int entry);

void emit_bb_xtb(emitter_t *e, long long num,
                 bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β,
                 bb_intcur_text_fn text_fn, void *text_arg)
{
    emit_bb_intcur(e, bb_tab, "bb_tab", "TAB", num, lbl_succ, lbl_fail, lbl_β, text_fn, text_arg);
}
