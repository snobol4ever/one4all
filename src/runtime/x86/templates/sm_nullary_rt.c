#include "../emitter.h"
#include "../bb_emit.h"

static void emit_sm_nullary_rt(emitter_t *e,
                                const char *comment_str,
                                const char *macro_name,
                                const char *rt_sym)
{
    (void)e;
    t_comment(comment_str);
    t_macro_begin(macro_name, NULL, 0);
    t_call_sym_plt(rt_sym, 0);
    t_macro_end();
    t_pad_to_blob_size();
}

void emit_sm_concat(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_CONCAT — pop right+left, push concat result",
                       "CONCAT", "rt_concat");
}

void emit_sm_push_null(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_PUSH_NULL — push null descriptor",
                       "PUSH_NULL", "rt_push_null");
}

void emit_sm_coerce_num(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_COERCE_NUM — coerce TOS string to number",
                       "COERCE_NUM", "rt_coerce_num");
}
