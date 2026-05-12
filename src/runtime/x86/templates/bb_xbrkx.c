#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

extern DESCR_t  bb_breakx(void *zeta, int entry);
extern brkx_t  *bb_breakx_new(const char *chars);

void emit_bb_xbrkx(emitter_t *e,
                   const char *chars,
                   bb_label_t *lbl_succ,
                   bb_label_t *lbl_fail,
                   bb_label_t *lbl_β)
{
    (void)e;
    brkx_t *z = bb_breakx_new(chars);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_breakx", (uint64_t)(uintptr_t)bb_breakx,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, "bb_breakx", (uint64_t)(uintptr_t)bb_breakx,
                   1, lbl_succ, lbl_fail);
}
