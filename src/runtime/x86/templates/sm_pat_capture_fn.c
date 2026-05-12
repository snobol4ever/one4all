#include "../emitter.h"
#include "../bb_emit.h"

void emit_sm_pat_capture_fn(emitter_t *e,
                             const char *fname_lbl, uint64_t fname_ptr,
                             int is_imm,
                             const char *namelist_lbl, uint64_t namelist_ptr)
{
    (void)e;
    t_comment("SM_PAT_CAPTURE_FN — pop child, push . *func() / $ *func() capture");

    static const char *const params[] = { "is_imm", "fname_lbl", "namelist_lbl" };
    t_macro_begin("PAT_CAPTURE_FN", params, 3);

    t_lea_rdi_strtab_sym(fname_lbl, fname_ptr);

    t_mov_esi_imm32(is_imm);

    t_lea_rdx_strtab_sym(namelist_lbl, namelist_ptr);
    t_call_sym_plt("rt_pat_capture_fn", 0);

    t_macro_end();
    t_pad_to_blob_size();
}

void emit_sm_pat_capture_fn_args(emitter_t *e,
                                  const char *fname_lbl, uint64_t fname_ptr,
                                  int is_imm, int nargs)
{
    (void)e;
    t_comment("SM_PAT_CAPTURE_FN_ARGS — pop child, push *func(args) capture");

    static const char *const params[] = { "is_imm", "nargs", "fname_lbl" };
    t_macro_begin("PAT_CAPTURE_FN_ARGS", params, 3);

    t_lea_rdi_strtab_sym(fname_lbl, fname_ptr);

    t_mov_esi_imm32(is_imm);

    t_mov_edx_imm32(nargs);
    t_call_sym_plt("rt_pat_capture_fn_args", 0);

    t_macro_end();
    t_pad_to_blob_size();
}
