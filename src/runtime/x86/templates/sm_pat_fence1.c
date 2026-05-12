#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_fence1(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_FENCE1 — push FENCE(p) (conditional fence) pattern",
                           "PAT_FENCE1", "rt_pat_fence1");
}
