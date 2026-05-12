#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_bb_pump_case(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_BB_PUMP_CASE — Raku case BB pump (M5)",
                       "BB_PUMP_CASE", "rt_unhandled_sm");
}
