#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_push_null(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_PUSH_NULL — push null descriptor",
                       "PUSH_NULL", "rt_push_null");
}
