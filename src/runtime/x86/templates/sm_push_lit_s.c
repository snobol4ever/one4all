/*
 * templates/sm_push_lit_s.c — SM_PUSH_LIT_S template.
 *
 * SM_PUSH_LIT_S pushes a string literal onto the value stack.
 *   a[0].s = string value (interned in strtab)
 *   a[1].i = string length
 *
 * Runtime call: rt_push_str(const char *s, int len)
 *   rdi = RIP-relative pointer to strtab label (TEXT/MACRO_DEF)
 *         or baked in-process pointer (BINARY)
 *   esi = length (int32)
 *
 * Macro body (SM_TPL_LBL_INT32):
 *   .macro PUSH_STR lbl, n
 *       lea  rdi, [rip + \lbl]
 *       mov  esi, \n
 *       call rt_push_str@PLT
 *   .endm
 *
 * Pattern: MACRO_DEF source of truth only (same discipline as
 * sm_call_fn.c).  TEXT dispatch uses sm_emit_lbl_int32 in
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

void emit_sm_push_lit_s(emitter_t *e,
                         const char *str_lbl, uint64_t str_ptr, int len)
{
    (void)e;
    t_comment("SM_PUSH_LIT_S — push string literal via rt_push_str(s, len)");

    static const char *const params[] = { "lbl", "n" };
    t_macro_begin("PUSH_STR", params, 2);

    t_lea_rdi_strtab_sym(str_lbl, str_ptr);
    t_mov_esi_imm32(len);
    t_call_sym_plt("rt_push_str", 0);

    t_macro_end();
    t_pad_to_blob_size();
}
