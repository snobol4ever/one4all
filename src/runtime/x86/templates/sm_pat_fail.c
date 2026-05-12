#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_fail(emitter_t *e)
{
    emit_sm_pat_rtcall(e, "SM_PAT_FAIL — push FAIL (always fail) pattern",
                           "PAT_FAIL", "rt_pat_fail");
}
