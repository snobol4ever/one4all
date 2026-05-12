#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_define_entry(emitter_t *e)
{
    emit_sm_rtcall(e, "SM_DEFINE_ENTRY — no-op: function entry marker",
                       "DEFINE_ENTRY", "rt_define_entry");
}
