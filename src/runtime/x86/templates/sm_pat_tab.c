#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_pat_tab(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_TAB — pop integer, push TAB(n) pattern",
                           "PAT_TAB", "rt_pat_tab");
}
