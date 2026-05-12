#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_bal(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_BAL — push BAL (balanced string) pattern",
                           "PAT_BAL", "rt_pat_bal");
}
