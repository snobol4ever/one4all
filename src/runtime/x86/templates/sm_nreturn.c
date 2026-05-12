#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_nreturn(emitter_t *e, int pc)
{
    emit_sm_return_variant(e, 2, 0, pc);
}
