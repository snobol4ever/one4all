#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_gen_tick(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_GEN_TICK — drive generator one tick (M5)",
                       "GEN_TICK", "rt_unhandled_sm");
}
