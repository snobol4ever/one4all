#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_eps(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_EPS — push epsilon pattern",
                           "PAT_EPS", "rt_pat_eps");
}
