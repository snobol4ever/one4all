#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_load_glocal(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_LOAD_GLOCAL — push gen-local slot (M5)",
                       "LOAD_GLOCAL", "rt_unhandled_sm");
}
