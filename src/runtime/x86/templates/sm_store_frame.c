#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_store_frame(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_STORE_FRAME — pop into IcnFrame slot (M5)",
                       "STORE_FRAME", "rt_unhandled_sm");
}
