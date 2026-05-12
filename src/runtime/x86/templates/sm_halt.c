#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_halt(emitter_t *e)
{
    (void)e;

    t_comment("SM_HALT — exit sm_jit_run via ret");
    t_inc_mem_r13_disp8(20);
    t_ret();
    t_pad_to_blob_size();
}
