#include "../emitter.h"
#include "../bb_emit.h"

static void emit_sm_lbl_rt(emitter_t *e,
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

void emit_sm_push_var(emitter_t *e, const char *name_lbl, uint64_t name_ptr)
{
    emit_sm_lbl_rt(e,
                   "SM_PUSH_VAR — push value of named variable via rt_nv_get",
                   "PUSH_VAR", "rt_nv_get",
                   name_lbl, name_ptr);
}

void emit_sm_store_var(emitter_t *e, const char *name_lbl, uint64_t name_ptr)
{
    emit_sm_lbl_rt(e,
                   "SM_STORE_VAR — store TOS into named variable via rt_nv_set",
                   "STORE_VAR", "rt_nv_set",
                   name_lbl, name_ptr);
}
