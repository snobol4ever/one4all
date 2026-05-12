#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_abort(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_ABORT — push ABORT (terminate match) pattern",
                           "PAT_ABORT", "rt_pat_abort");
}
