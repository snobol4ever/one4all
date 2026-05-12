#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_arb(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_ARB — push ARB (greedy 0+) pattern",
                           "PAT_ARB", "rt_pat_arb");
}
