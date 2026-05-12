#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_any(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_ANY — pop charset string, push ANY pattern",
                           "PAT_ANY", "rt_pat_any");
}
