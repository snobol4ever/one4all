#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_succeed(emitter_t *e)
{
    emit_sm_pat_rtcall(e, "SM_PAT_SUCCEED — push SUCCEED (always succeed) pattern",
                           "PAT_SUCCEED", "rt_pat_succeed");
}
