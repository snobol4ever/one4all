#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_call_fn(emitter_t *e, const char *name_lbl,
                     uint64_t name_ptr, int nargs)
{
    (void)e;
    (void)name_lbl;
    (void)name_ptr;
    (void)nargs;

    t_comment("SM_CALL_FN — call named function via rt_call(name, nargs)");

    static const char *const params[] = { "lbl", "n" };
    t_macro_begin("CALL_FN", params, 2);

    t_lea_rdi_strtab_sym(NULL, 0);
    t_mov_esi_imm32(0);
    t_call_sym_plt("rt_call", 0);

    t_macro_end();
    t_pad_to_blob_size();
}
