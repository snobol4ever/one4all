#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_rem(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_REM — push REM (rest of subject) pattern",
                           "PAT_REM", "rt_pat_rem");
}
