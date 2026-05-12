#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_mod(emitter_t *e)
{
    emit_sm_arith_op(e, SM_MOD, "MOD_NUM");
}
