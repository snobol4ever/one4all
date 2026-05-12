#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_arbno(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_ARBNO — pop inner pattern, push ARBNO pattern",
                           "PAT_ARBNO", "rt_pat_arbno");
}
