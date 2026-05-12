#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "../sm_prog.h"

void emit_sm_div(emitter_t *e)
{
    emit_sm_arith_op(e, SM_DIV, "DIV_NUM");
}
