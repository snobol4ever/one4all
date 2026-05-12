/*
 * templates/sm_var.c — SM_PUSH_VAR and SM_STORE_VAR templates.
 *
 * Both opcodes take one string argument (SM_TPL_LBL): a variable name
 * stored in the strtab.  The macro body loads the name's address into
 * rdi and calls the runtime function.
 *
 *   SM_PUSH_VAR   PUSH_VAR  rt_nv_get   — push value of named variable
 *   SM_STORE_VAR  STORE_VAR rt_nv_set   — store TOS into named variable
 *
 * Macro bodies:
 *   .macro PUSH_VAR lbl
 *       lea  rdi, [rip + \lbl]
 *       call rt_nv_get@PLT
 *   .endm
 *   (STORE_VAR: same shape, rt_nv_set)
 *
 * Pattern: MACRO_DEF source of truth only (same discipline as
 * sm_call_fn.c, sm_pat_lbl.c).  TEXT dispatch uses sm_emit_lbl in
 * sm_codegen_x64_emit.c.  BINARY uses standard_blob in sm_codegen.c.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-s (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-s / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* unused emitter_t * param; caller compat */
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
