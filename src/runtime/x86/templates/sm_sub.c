#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_sub(emitter_t *e)
{
    emit_sm_arith_op(e, SM_SUB, "SUB_NUM");
}
