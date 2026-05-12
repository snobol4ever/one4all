#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_push_null_noflip(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_PUSH_NULL_NOFLIP — push null, preserve last_ok",
                       "PUSH_NULL_NOFLIP", "rt_push_null_noflip");
}
