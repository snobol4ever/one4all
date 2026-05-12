#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_push_expression(emitter_t *e, uint64_t entry_ptr, int arity)
{
    (void)e;
    t_comment("SM_PUSH_EXPRESSION — push expression descriptor (entry, arity)");
    static const char *const params[] = { "entry", "arity" };
    t_macro_begin("PUSH_EXPRESSION", params, 2);
    t_movabs_rdi_entry(entry_ptr);
    t_mov_esi_imm32(arity);
    t_call_sym_plt("rt_push_expression_descr", 0);
    t_macro_end();
    t_pad_to_blob_size();
}
