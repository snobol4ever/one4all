#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_return_s(emitter_t *e, int pc)
{
    emit_sm_return_variant(e, 0, 1, pc);
}
