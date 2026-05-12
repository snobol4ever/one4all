#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

/* XOR: alternation node — handled inline in flat_emit_node via flat_emit_alt().
 * This template file exists for completeness (one-file-per-box law). */
void emit_bb_xor(emitter_t *e,
                 bb_label_t *lbl_succ, bb_label_t *lbl_fail, bb_label_t *lbl_β)
{
    (void)e;
    t_bb_box_banner("ALT", "");
    t_label_define(lbl_β);
    t_emit_jmp(lbl_fail, JMP_JMP);
    t_emit_jmp(lbl_fail, JMP_JMP);
}
