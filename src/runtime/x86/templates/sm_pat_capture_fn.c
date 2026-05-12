/*
 * templates/sm_pat_capture_fn.c — capture-fn SM_PAT_* opcode templates.
 *
 * Covers the three-argument capture/usercall pattern opcodes:
 *
 *   SM_PAT_CAPTURE_FN      PAT_CAPTURE_FN      rt_pat_capture_fn
 *     a[0].s=fname, a[1].i=is_imm, a[2].s=namelist
 *     pops child pattern, pushes . *func() / $ *func() wrapper
 *
 *   SM_PAT_CAPTURE_FN_ARGS PAT_CAPTURE_FN_ARGS rt_pat_capture_fn_args
 *     a[0].s=fname, a[1].i=is_imm, a[2].i=nargs
 *     pops child pattern, pushes *func(args) wrapper
 *
 * Pattern: MACRO_DEF source of truth only, same as sm_call_fn.c and
 * sm_pat_capture.c.  TEXT and BINARY handled by legacy paths in
 * sm_codegen_x64_emit.c (sm_emit_capture_fn / sm_emit_capture_fn_args).
 *
 * Macro body shapes:
 *   PAT_CAPTURE_FN  is_imm, fname_lbl, namelist_lbl:
 *       lea  rdi, [rip + \fname_lbl]     ; fname pointer
 *       mov  esi, \is_imm               ; is_imm flag
 *       lea  rdx, [rip + \namelist_lbl] ; namelist pointer (may be null sym)
 *       call rt_pat_capture_fn@PLT
 *
 *   PAT_CAPTURE_FN_ARGS  is_imm, nargs, fname_lbl:
 *       lea  rdi, [rip + \fname_lbl]     ; fname pointer
 *       mov  esi, \is_imm               ; is_imm flag
 *       mov  edx, \nargs                ; nargs count
 *       call rt_pat_capture_fn_args@PLT
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-r (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-r / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* unused emitter_t * param; caller compat */
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

    /* MACRO_DEF: lea rdi, [rip + \fname_lbl] */
    t_lea_rdi_strtab_sym(fname_lbl, fname_ptr);
    /* MACRO_DEF: mov esi, \is_imm */
    t_mov_esi_imm32(is_imm);
    /* MACRO_DEF: lea rdx, [rip + \namelist_lbl] */
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

    /* MACRO_DEF: lea rdi, [rip + \fname_lbl] */
    t_lea_rdi_strtab_sym(fname_lbl, fname_ptr);
    /* MACRO_DEF: mov esi, \is_imm */
    t_mov_esi_imm32(is_imm);
    /* MACRO_DEF: mov edx, \nargs */
    t_mov_edx_imm32(nargs);
    t_call_sym_plt("rt_pat_capture_fn_args", 0);

    t_macro_end();
    t_pad_to_blob_size();
}
