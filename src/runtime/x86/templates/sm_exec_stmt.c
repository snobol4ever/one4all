#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_exec_stmt(emitter_t *e,
                        const char *subj_lbl, uint64_t subj_ptr,
                        int has_repl)
{
    (void)e;
    t_comment("SM_EXEC_STMT — execute pattern statement via rt_match_variant");
    static const char *const params[] = { "has_repl", "subj_lbl" };
    t_macro_begin("EXEC_STMT_VARIANT", params, 2);
    t_lea_rdi_strtab_sym(subj_lbl, subj_ptr);
    t_mov_esi_imm32(has_repl);
    t_call_sym_plt("rt_match_variant", 0);
    t_macro_end();
    t_pad_to_blob_size();
}
