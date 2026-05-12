#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_bb_charset(emitter_t *e,
                     bb_box_fn c_fn,
                     const char *c_fn_name,
                     const char *kind_name,
                     const char *chars,
                     bb_label_t *lbl_succ,
                     bb_label_t *lbl_fail,
                     bb_label_t *lbl_β)
{
    (void)kind_name; (void)e;
    typedef struct { const char *chars; int delta; } cs_t;
    cs_t *z = calloc(1, sizeof(cs_t));
    z->chars = chars;

    t_bb_port_call((uint64_t)(uintptr_t)z, c_fn_name, (uint64_t)(uintptr_t)c_fn,
                   0, lbl_succ, lbl_fail);
    t_label_define(lbl_β);
    t_bb_port_call((uint64_t)(uintptr_t)z, c_fn_name, (uint64_t)(uintptr_t)c_fn,
                   1, lbl_succ, lbl_fail);
}
