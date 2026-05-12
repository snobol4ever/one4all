#include "../emitter.h"
#include "../bb_emit.h"
#include "templates.h"

void emit_sm_return(emitter_t *e)
{
    (void)e;
    t_comment("SM_RETURN — native return");
    t_macro_begin("RETURN", NULL, 0);
    t_ret();
    t_macro_end();
    t_pad_to_blob_size();
}

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

void emit_sm_freturn(emitter_t *e, int pc)    { emit_sm_return_variant(e, 1, 0, pc); }
void emit_sm_nreturn(emitter_t *e, int pc)    { emit_sm_return_variant(e, 2, 0, pc); }
void emit_sm_return_s(emitter_t *e, int pc)   { emit_sm_return_variant(e, 0, 1, pc); }
void emit_sm_return_f(emitter_t *e, int pc)   { emit_sm_return_variant(e, 0, 2, pc); }
void emit_sm_freturn_s(emitter_t *e, int pc)  { emit_sm_return_variant(e, 1, 1, pc); }
void emit_sm_freturn_f(emitter_t *e, int pc)  { emit_sm_return_variant(e, 1, 2, pc); }
void emit_sm_nreturn_s(emitter_t *e, int pc)  { emit_sm_return_variant(e, 2, 1, pc); }
void emit_sm_nreturn_f(emitter_t *e, int pc)  { emit_sm_return_variant(e, 2, 2, pc); }
