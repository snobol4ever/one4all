#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_coerce_num(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_COERCE_NUM — coerce TOS string to number",
                       "COERCE_NUM", "rt_coerce_num");
}
