#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_push_lit_s(emitter_t *e,
                         const char *str_lbl, uint64_t str_ptr, int len)
{
    (void)e;
    t_comment("SM_PUSH_LIT_S — push string literal via rt_push_str(s, len)");

    static const char *const params[] = { "lbl", "n" };
    t_macro_begin("PUSH_STR", params, 2);

    t_lea_rdi_strtab_sym(str_lbl, str_ptr);
    t_mov_esi_imm32(len);
    t_call_sym_plt("rt_push_str", 0);

    t_macro_end();
    t_pad_to_blob_size();
}
