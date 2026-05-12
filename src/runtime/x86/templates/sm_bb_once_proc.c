#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_bb_once_proc(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_BB_ONCE_PROC — Prolog proc BB once (M5)",
                       "BB_ONCE_PROC", "rt_unhandled_sm");
}
