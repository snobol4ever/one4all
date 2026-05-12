#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_bb_pump_sm(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_BB_PUMP_SM — migrated SM BB pump (M5)",
                       "BB_PUMP_SM", "rt_unhandled_sm");
}
