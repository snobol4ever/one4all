#include "../emitter.h"
#include "../bb_emit.h"

static void emit_sm_pat_lbl_int_rt(emitter_t *e,
                                    const char *comment_str,
                                    const char *macro_name,
                                    const char *rt_sym,
                                    const char *name_lbl,
                                    uint64_t name_ptr,
                                    int n)
{
    (void)e;
    t_comment(comment_str);

    static const char *const params[] = { "lbl", "n" };
    t_macro_begin(macro_name, params, 2);

    t_lea_rdi_strtab_sym(name_lbl, name_ptr);

    t_mov_esi_imm32(n);
    t_call_sym_plt(rt_sym, 0);

    t_macro_end();
    t_pad_to_blob_size();
}

void emit_sm_pat_capture(emitter_t *e,
                          const char *name_lbl, uint64_t name_ptr, int kind)
{
    emit_sm_pat_lbl_int_rt(e,
                            "SM_PAT_CAPTURE — pop child, push capture(varname, kind)",
                            "PAT_CAPTURE", "rt_pat_capture",
                            name_lbl, name_ptr, kind);
}

void emit_sm_pat_usercall_args(emitter_t *e,
                                const char *name_lbl, uint64_t name_ptr, int nargs)
{
    emit_sm_pat_lbl_int_rt(e,
                            "SM_PAT_USERCALL_ARGS — push *func(args) user-call pattern",
                            "PAT_USERCALL_ARGS", "rt_pat_usercall_args",
                            name_lbl, name_ptr, nargs);
}
