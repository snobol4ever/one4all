#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_define(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_DEFINE — no-op: function definition prescan",
                       "DEFINE", "rt_define");
}
