#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "../sm_prog.h"

void emit_sm_add(emitter_t *e)
{
    emit_sm_arith_op(e, SM_ADD, "ADD_NUM");
}
