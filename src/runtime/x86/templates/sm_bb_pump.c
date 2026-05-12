#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_bb_pump(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_BB_PUMP — drive BB generator (M5)",
                       "BB_PUMP", "rt_unhandled_sm");
}
