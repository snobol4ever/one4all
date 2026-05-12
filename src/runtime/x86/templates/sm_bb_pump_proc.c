#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_bb_pump_proc(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_BB_PUMP_PROC — Icon proc BB pump (M5)",
                       "BB_PUMP_PROC", "rt_unhandled_sm");
}
