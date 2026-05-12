#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_exp(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_EXP — pop 2, push base**exp",
                       "EXP_NUM", "rt_exp");
}
