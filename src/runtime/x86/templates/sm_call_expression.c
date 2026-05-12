#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_call_expression(emitter_t *e, const char *tgt_sym)
{
    (void)e;
    t_comment("SM_CALL_EXPRESSION — call expression chunk directly");
    static const char *const params[] = { "tgt" };
    t_macro_begin("CALL_EXPRESSION", params, 1);
    t_call_sym_param(tgt_sym);
    t_macro_end();
    t_pad_to_blob_size();
}
