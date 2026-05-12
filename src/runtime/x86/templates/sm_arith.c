#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_arith_op(emitter_t *e, int op_enum, const char *macro_name)
{
    (void)e;
    t_comment(macro_name ? macro_name : "SM_ARITH");
    t_macro_begin(macro_name ? macro_name : "ARITH", NULL, 0);
    t_mov_rdi_imm64((uint64_t)(unsigned)op_enum);
    t_call_sym_plt("rt_arith", 0);
    t_macro_end();
    t_pad_to_blob_size();
}
