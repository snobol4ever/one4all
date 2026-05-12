#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_concat(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_CONCAT — pop right+left, push concat result",
                       "CONCAT", "rt_concat");
}
