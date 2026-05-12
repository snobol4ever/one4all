#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_bb_once(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_BB_ONCE — run BB once (M5)",
                       "BB_ONCE", "rt_unhandled_sm");
}
