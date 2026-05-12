#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_load_frame(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_LOAD_FRAME — push IcnFrame slot (M5)",
                       "LOAD_FRAME", "rt_unhandled_sm");
}
