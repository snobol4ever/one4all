#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_fence(emitter_t *e)
{
    emit_sm_pat_rtcall(e, "SM_PAT_FENCE0 — push FENCE (no-backtrack barrier) pattern",
                           "PAT_FENCE", "rt_pat_fence");
}
