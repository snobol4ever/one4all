#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_break(emitter_t *e)
{
    emit_sm_pat_rtcall(e, "SM_PAT_BREAK — pop charset string, push BREAK pattern",
                           "PAT_BREAK", "rt_pat_break");
}
