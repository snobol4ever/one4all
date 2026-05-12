#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_len(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_LEN — pop integer, push LEN(n) pattern",
                           "PAT_LEN", "rt_pat_len");
}
