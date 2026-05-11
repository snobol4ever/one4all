/*
 * templates/sm_jump.c — SM_JUMP / SM_JUMP_S / SM_JUMP_F per-opcode templates.
 *
 * SM_JUMP:   unconditional jump to target PC.
 *   Emits: jmp .L<target>
 *
 * SM_JUMP_S: jump to target PC if last_ok (success).
 *   Emits: call rt_last_ok@PLT; test rax,rax; jnz .L<target>
 *
 * SM_JUMP_F: jump to target PC if NOT last_ok (failure).
 *   Emits: call rt_last_ok@PLT; test rax,rax; jz .L<target>
 *
 * No mode-3/mode-4 divergence: both modes use the same runtime symbols.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-j (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-j / GOAL-MODE4-EMIT
 */

#include "../emitter.h"
#include "../bb_emit.h"     /* bb_label_t, bb_label_initf, JMP_*, t_* helpers */
#include "templates.h"

/* Helper: build a bb_label_t for a SM PC target (.L<pc>). */
static void make_pc_label(bb_label_t *lbl, int target_pc)
{
    bb_label_initf(lbl, ".L%d", target_pc);
}

/* emit_sm_jump — unconditional jump to target_pc. */
void emit_sm_jump(emitter_t *e, int target_pc)
{
    (void)e;
    t_comment("SM_JUMP");
    bb_label_t tgt;
    make_pc_label(&tgt, target_pc);
    t_emit_jmp(&tgt, JMP_JMP);
}

/* emit_sm_jump_s — conditional jump: take if rt_last_ok() != 0 (success). */
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

/* emit_sm_jump_f — conditional jump: take if rt_last_ok() == 0 (failure). */
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
