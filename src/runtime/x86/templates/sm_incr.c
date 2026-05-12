#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_incr(emitter_t *e, int64_t n)
{
    (void)e;
    t_comment("SM_INCR — increment TOS by immediate n");
    static const char *const params[] = { "n" };
    t_macro_begin("INCR", params, 1);
    t_mov_rdi_imm64((uint64_t)n);
    t_call_sym_plt("rt_incr", 0);
    t_macro_end();
    t_pad_to_blob_size();
}
