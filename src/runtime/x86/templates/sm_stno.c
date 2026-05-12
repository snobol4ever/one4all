#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_stno(emitter_t *e, int stno, int lineno, const char *src_text)
{
    (void)e;
    t_banner_stno(stno, lineno, src_text);
    t_noop_macro("STNO");
}
