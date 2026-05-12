#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_rem(emitter_t *e)
{
    emit_sm_pat_rtcall(e, "SM_PAT_REM — push REM (rest of subject) pattern",
                           "PAT_REM", "rt_pat_rem");
}
