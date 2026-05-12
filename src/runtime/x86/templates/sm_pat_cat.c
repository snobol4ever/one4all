#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_cat(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_CAT — pop right+left patterns, push CAT pattern",
                           "PAT_CAT", "rt_pat_cat");
}
