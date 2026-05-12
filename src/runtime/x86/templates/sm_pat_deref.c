#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_deref(emitter_t *e)
{
    emit_sm_pat_rtcall(e, "SM_PAT_DEREF — pop descriptor, push variable-deref pattern",
                           "PAT_DEREF", "rt_pat_deref");
}
