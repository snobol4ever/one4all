/* sm_helpers.c — shared helper functions called by split SM template files.
 * These were formerly static functions in the multi-opcode bundle files. */
#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_nullary_rt(emitter_t *e,
                         const char *comment_str,
                         const char *macro_name,
                         const char *rt_sym)
{
    (void)e;
    t_comment(comment_str);
    t_macro_begin(macro_name, NULL, 0);
    t_call_sym_plt(rt_sym, 0);
    t_macro_end();
    t_pad_to_blob_size();
}

void emit_sm_pat_nullary_rt(emitter_t *e,
                              const char *comment_str,
                              const char *macro_name,
                              const char *rt_sym)
{
    (void)e;
    t_comment(comment_str);
    t_macro_begin(macro_name, NULL, 0);
    t_call_sym_plt(rt_sym, 0);
    t_macro_end();
    t_pad_to_blob_size();
}

void emit_sm_lbl_rt(emitter_t *e,
                     const char *comment_str,
                     const char *macro_name,
                     const char *rt_sym,
                     const char *name_lbl,
                     uint64_t name_ptr)
{
    (void)e;
    t_comment(comment_str);
    static const char *const params[] = { "lbl" };
    t_macro_begin(macro_name, params, 1);
    t_lea_rdi_strtab_sym(name_lbl, name_ptr);
    t_call_sym_plt(rt_sym, 0);
    t_macro_end();
    t_pad_to_blob_size();
}

void emit_sm_arith_op(emitter_t *e, int op_enum, const char *macro_name)
{
    (void)e;
    t_comment(macro_name ? macro_name : "SM_ARITH");
    t_macro_begin(macro_name ? macro_name : "ARITH", NULL, 0);
    t_mov_rdi_imm64((uint64_t)(unsigned)op_enum);
    t_call_sym_plt("rt_arith", 0);
    t_macro_end();
    t_pad_to_blob_size();
}
