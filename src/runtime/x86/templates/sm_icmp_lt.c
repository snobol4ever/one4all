#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_icmp_lt(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_ICMP_LT — integer compare < (M5)",
                       "ICMP_LT", "rt_unhandled_sm");
}
