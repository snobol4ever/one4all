#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_bb_pump_every(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_BB_PUMP_EVERY — every-generator BB pump (M5)",
                       "BB_PUMP_EVERY", "rt_unhandled_sm");
}
