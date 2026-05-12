#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

static void make_pc_label(bb_label_t *lbl, int target_pc)
{
    bb_label_initf(lbl, ".L%d", target_pc);
}

void emit_sm_jump(emitter_t *e, int target_pc)
{
    (void)e;
    t_comment("SM_JUMP — unconditional jump");
    bb_label_t tgt;
    make_pc_label(&tgt, target_pc);
    t_emit_jmp(&tgt, JMP_JMP);
}

void emit_sm_jump_s(emitter_t *e, int target_pc)
{
    (void)e;
    t_comment("SM_JUMP_S — jump if last_ok");
    t_call_sym_plt("rt_last_ok", 0);
    t_test_rax_rax();
    bb_label_t tgt;
    make_pc_label(&tgt, target_pc);
    t_emit_jmp(&tgt, JMP_JNE);
}

void emit_sm_jump_f(emitter_t *e, int target_pc)
{
    (void)e;
    t_comment("SM_JUMP_F — jump if not last_ok");
    t_call_sym_plt("rt_last_ok", 0);
    t_test_rax_rax();
    bb_label_t tgt;
    make_pc_label(&tgt, target_pc);
    t_emit_jmp(&tgt, JMP_JE);
}
