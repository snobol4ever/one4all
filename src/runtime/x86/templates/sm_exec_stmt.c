#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_push_expression(emitter_t *e, uint64_t entry_ptr, int arity)
{
    (void)e;
    t_comment("SM_PUSH_EXPRESSION — push expression descriptor (entry, arity)");

    static const char *const params[] = { "entry", "arity" };
    t_macro_begin("PUSH_EXPRESSION", params, 2);

    t_movabs_rdi_entry(entry_ptr);

    t_mov_esi_imm32(arity);
    t_call_sym_plt("rt_push_expression_descr", 0);

    t_macro_end();
    t_pad_to_blob_size();
}

void emit_sm_call_expression(emitter_t *e, const char *tgt_sym)
{
    (void)e;
    t_comment("SM_CALL_EXPRESSION — call expression chunk directly");

    static const char *const params[] = { "tgt" };
    t_macro_begin("CALL_EXPRESSION", params, 1);

    t_call_sym_param(tgt_sym);

    t_macro_end();
    t_pad_to_blob_size();
}

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
