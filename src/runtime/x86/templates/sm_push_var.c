#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_push_var(emitter_t *e, const char *name_lbl, uint64_t name_ptr)
{
    emit_sm_lbl_rt(e,
                   "SM_PUSH_VAR — push value of named variable via rt_nv_get",
                   "PUSH_VAR", "rt_nv_get",
                   name_lbl, name_ptr);
}
