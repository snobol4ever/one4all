#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_push_lit_i(emitter_t *e, int64_t val)
{
    (void)e;

    t_comment("SM_PUSH_LIT_I — push integer literal");

    static const char *const params[] = { "val" };
    t_macro_begin("PUSH_INT", params, 1);

    t_mov_rdi_imm64((uint64_t)val);
    t_call_sym_plt("rt_push_int", 0);

    t_macro_end();
    t_pad_to_blob_size();
}
