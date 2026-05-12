#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_alt(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_ALT — pop right+left patterns, push ALT pattern",
                           "PAT_ALT", "rt_pat_alt");
}
