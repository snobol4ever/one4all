#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_concat(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_CONCAT — pop right+left, push concat result",
                       "CONCAT", "rt_concat");
}
