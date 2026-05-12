#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_store_glocal(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_STORE_GLOCAL — pop into gen-local slot (M5)",
                       "STORE_GLOCAL", "rt_unhandled_sm");
}
