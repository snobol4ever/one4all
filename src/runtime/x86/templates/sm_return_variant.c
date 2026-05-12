#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_return_variant(emitter_t *e, int kind, int cond, int pc)
{
    (void)e;
    t_comment("SM_RETURN_VARIANT — conditional/typed return via rt_do_return");
    static const char *const params[] = { "kind", "cond", "pc" };
    t_macro_begin("RETURN_VARIANT", params, 3);
    t_mov_edi_imm32(kind);
    t_mov_esi_imm32(cond);
    t_call_sym_plt("rt_do_return", 0);
    t_test_eax_eax();
    t_jz_retskip(pc);
    t_ret();
    t_retskip_label(pc);
    t_macro_end();
    t_pad_to_blob_size();
}
