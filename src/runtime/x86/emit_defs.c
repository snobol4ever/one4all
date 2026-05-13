

#include "emit.h"

void emitter_init_macro_def(FILE *out)
{
    emitter_init_text(out, TEXT_MODE_DEFINITION);
}
