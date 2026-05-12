#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"
#include "sm_helpers.h"

void emit_sm_store_var(emitter_t *e, const char *name_lbl, uint64_t name_ptr)
{
    emit_sm_lbl_rt(e,
                   "SM_STORE_VAR — store TOS into named variable via rt_nv_set",
                   "STORE_VAR", "rt_nv_set",
                   name_lbl, name_ptr);
}
