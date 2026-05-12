#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_jump_s(emitter_t *e, int target_pc)
{
    (void)e;
    t_comment("SM_JUMP_S — jump if last_ok");
    t_call_sym_plt("rt_last_ok", 0);
    t_test_rax_rax();
    bb_label_t tgt;
    bb_label_initf(&tgt, ".L%d", target_pc);
    t_emit_jmp(&tgt, JMP_JNE);
}
