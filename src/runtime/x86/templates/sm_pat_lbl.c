#include "../emitter.h"
#include "../bb_emit.h"

static void emit_sm_pat_lbl_rt(emitter_t *e,
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

void emit_sm_pat_lit(emitter_t *e, const char *name_lbl, uint64_t name_ptr)
{
    emit_sm_pat_lbl_rt(e,
                       "SM_PAT_LIT — push literal-string match pattern",
                       "PAT_LIT", "rt_pat_lit",
                       name_lbl, name_ptr);
}

void emit_sm_pat_refname(emitter_t *e, const char *name_lbl, uint64_t name_ptr)
{
    emit_sm_pat_lbl_rt(e,
                       "SM_PAT_REFNAME — push *varname pattern (deref by name)",
                       "PAT_REFNAME", "rt_pat_refname",
                       name_lbl, name_ptr);
}

void emit_sm_pat_usercall(emitter_t *e, const char *name_lbl, uint64_t name_ptr)
{
    emit_sm_pat_lbl_rt(e,
                       "SM_PAT_USERCALL — push *func() user-call pattern",
                       "PAT_USERCALL", "rt_pat_usercall",
                       name_lbl, name_ptr);
}
