#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_suspend(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_SUSPEND — generator suspend (M5)",
                       "SUSPEND", "rt_unhandled_sm");
}
