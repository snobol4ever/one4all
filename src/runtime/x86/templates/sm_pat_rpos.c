#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_rpos(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_RPOS — pop integer, push RPOS(n) pattern",
                           "PAT_RPOS", "rt_pat_rpos");
}
