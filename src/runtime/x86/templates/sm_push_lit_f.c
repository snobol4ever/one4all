#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_push_lit_f(emitter_t *e, double val)
{
    (void)e;
    t_comment("SM_PUSH_LIT_F — push real literal");
    static const char *const params[] = { "val" };
    t_macro_begin("PUSH_REAL", params, 1);
    /* Pass the double as uint64 bit-pattern via rdi (ABI: xmm0 for double,
     * but we pass through int reg and rt_push_real takes double — use movabs
     * of bit pattern then call via a wrapper that moves rdi→xmm0). */
    uint64_t bits;
    __builtin_memcpy(&bits, &val, 8);
    t_mov_rdi_imm64(bits);
    t_call_sym_plt("rt_push_real_bits", 0);
    t_macro_end();
    t_pad_to_blob_size();
}
