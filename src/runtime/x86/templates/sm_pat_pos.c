#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_pos(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_POS — pop integer, push POS(n) pattern",
                           "PAT_POS", "rt_pat_pos");
}
