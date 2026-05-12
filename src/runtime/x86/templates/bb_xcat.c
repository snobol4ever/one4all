#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

/* XCAT: concatenation node — handled inline in flat_emit_node via flat_emit_xcat().
 * This template file exists for completeness (one-file-per-box law).
 * Direct callers should use flat_emit_xcat (bb_flat.c) instead. */
void emit_bb_xcat(emitter_t *e,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    /* Composite: should not be called directly — flat_emit_node recurses. */
    (void)e;
    t_bb_box_banner("CAT", "");
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
