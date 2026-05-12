#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_fence(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_FENCE — push FENCE (no-backtrack barrier) pattern",
                           "PAT_FENCE", "rt_pat_fence");
}
