#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_span(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_SPAN — pop charset string, push SPAN pattern",
                           "PAT_SPAN", "rt_pat_span");
}
