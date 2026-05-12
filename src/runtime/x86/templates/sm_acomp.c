#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_acomp(emitter_t *e, int op)
{
    (void)e;
    t_comment("SM_ACOMP — numeric compare, op=EKind");
    static const char *const params[] = { "op" };
    t_macro_begin("ACOMP", params, 1);
    t_mov_edi_imm32(op);
    t_call_sym_plt("rt_acomp", 0);
    t_macro_end();
    t_pad_to_blob_size();
}
