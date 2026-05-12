#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_neg(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_NEG — negate TOS",
                       "NEGATE", "rt_neg");
}
