#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_push_expr(emitter_t *e, uint64_t ptr_val)
{
    (void)e;
    t_comment("SM_PUSH_EXPR — push frozen DT_E expression descriptor");
    static const char *const params[] = { "ptr" };
    t_macro_begin("PUSH_EXPR", params, 1);
    t_mov_rdi_imm64(ptr_val);
    t_call_sym_plt("rt_push_expr", 0);
    t_macro_end();
    t_pad_to_blob_size();
}
