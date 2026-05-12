/*
 * templates/sm_pat_lbl.c — string-arg SM_PAT_* opcode templates.
 *
 * Covers SM_PAT_* opcodes that take one string argument (SM_TPL_LBLOPT
 * in g_sm_templates[]).  The argument is a strtab label in TEXT/MACRO_DEF
 * mode and a baked in-process pointer in BINARY mode.
 *
 * Opcodes covered:
 *   SM_PAT_LIT      PAT_LIT      rt_pat_lit      literal string match
 *   SM_PAT_REFNAME  PAT_REFNAME  rt_pat_refname  *varname deref by name
 *   SM_PAT_USERCALL PAT_USERCALL rt_pat_usercall *func() user call
 *
 * Pattern: identical to sm_call_fn.c (MACRO_DEF source of truth only).
 * TEXT dispatch uses the legacy sm_emit_lblopt path in
 * sm_codegen_x64_emit.c.  BINARY uses standard_blob in sm_codegen.c.
 *
 * Macro bodies:
 *   .macro PAT_LIT lbl
 *       lea  rdi, [rip + \lbl]   ; pointer to literal string
 *       call rt_pat_lit@PLT
 *   .endm
 *   (PAT_REFNAME and PAT_USERCALL: same shape, different rt_sym)
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-r (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-r / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* unused emitter_t * param; caller compat */
#include "../bb_emit.h"

/* emit_sm_pat_lbl_rt — shared MACRO_DEF body for string-arg PAT_* templates.
 *   comment_str:  inline annotation
 *   macro_name:   GAS macro name, e.g. "PAT_LIT"
 *   rt_sym:       PLT symbol, e.g. "rt_pat_lit"
 *   name_lbl:     strtab label (used in TEXT/MACRO_DEF; NULL in MACRO_DEF body)
 *   name_ptr:     in-process pointer (BINARY only; 0 in MACRO_DEF body)
 */
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

    /* In MACRO_DEF mode: emits  lea rdi, [rip + \lbl] */
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
