#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_rtab(emitter_t *e)
{
    emit_sm_pat_rtcall(e, "SM_PAT_RTAB — pop integer, push RTAB(n) pattern",
                           "PAT_RTAB", "rt_pat_rtab");
}
