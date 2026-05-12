#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_void_pop(emitter_t *e)
{
    (void)e;

    t_comment("SM_VOID_POP — pop and discard TOS");
    t_macro_begin("VOID_POP", NULL, 0);

    t_call_sym_plt("rt_pop_void", 0);

    t_macro_end();
    t_pad_to_blob_size();
}
