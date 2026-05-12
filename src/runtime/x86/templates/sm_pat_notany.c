#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_notany(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_NOTANY — pop charset string, push NOTANY pattern",
                           "PAT_NOTANY", "rt_pat_notany");
}
