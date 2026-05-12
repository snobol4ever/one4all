/*
 * templates/sm_pat_capture.c — capture SM_PAT_* opcode templates.
 *
 * Covers SM_PAT_* opcodes that take a string label and an integer
 * (SM_TPL_LBLOPT_INT32 in g_sm_templates[]):
 *
 *   SM_PAT_CAPTURE       PAT_CAPTURE       rt_pat_capture
 *     a[0].s = varname, a[1].i = kind (1=imm/$, else=cond/.)
 *     pops child pattern from value stack, pushes capture wrapper
 *
 *   SM_PAT_USERCALL_ARGS PAT_USERCALL_ARGS rt_pat_usercall_args
 *     a[0].s = fname, a[1].i = nargs
 *     pops nargs patterns + child, pushes usercall wrapper
 *
 * Pattern: same as sm_call_fn.c — MACRO_DEF source of truth only.
 * TEXT dispatch uses sm_emit_lblopt_int32 in sm_codegen_x64_emit.c.
 * BINARY uses standard_blob in sm_codegen.c.
 *
 * Macro bodies:
 *   .macro PAT_CAPTURE lbl, n
 *       lea  rdi, [rip + \lbl]   ; varname pointer
 *       mov  esi, \n             ; kind (1=imm, 0=cond)
 *       call rt_pat_capture@PLT
 *   .endm
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-r (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-r / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* unused emitter_t * param; caller compat */
#include "../bb_emit.h"

/* emit_sm_pat_lbl_int_rt — shared MACRO_DEF body for lbl+int PAT_* templates. */
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

    /* MACRO_DEF: emits  lea rdi, [rip + \lbl] */
    t_lea_rdi_strtab_sym(name_lbl, name_ptr);
    /* MACRO_DEF: emits  mov esi, \n */
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
