#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_icmp_gt(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_ICMP_GT — integer compare > (M5)",
                       "ICMP_GT", "rt_unhandled_sm");
}
