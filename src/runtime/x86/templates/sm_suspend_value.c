#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_suspend_value(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_SUSPEND_VALUE — coroutine yield (M5)",
                       "SUSPEND_VALUE", "rt_unhandled_sm");
}
