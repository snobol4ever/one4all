#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_resume(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_RESUME — generator resume marker (M5)",
                       "RESUME", "rt_unhandled_sm");
}
