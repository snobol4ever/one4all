#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_pat_alt(emitter_t *e)
{
    emit_sm_pat_rtcall(e, "SM_PAT_ALT — pop right+left patterns, push ALT pattern",
                           "PAT_ALT", "rt_pat_alt");
}
