#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_jump(emitter_t *e, int target_pc)
{
    (void)e;
    t_comment("SM_JUMP — unconditional jump");
    bb_label_t tgt;
    bb_label_initf(&tgt, ".L%d", target_pc);
    t_emit_jmp(&tgt, JMP_JMP);
}
