/*
 * templates/sm_return.c — SM_RETURN and SM_RETURN_VARIANT family templates.
 *
 * Covers two shapes:
 *
 * 1. SM_RETURN (unconditional plain return)
 *    Macro: RETURN
 *    Body:  ret
 *
 * 2. SM_RETURN_VARIANT (conditional / typed returns)
 *    Opcodes: SM_RETURN_S, SM_RETURN_F,
 *             SM_FRETURN, SM_FRETURN_S, SM_FRETURN_F,
 *             SM_NRETURN, SM_NRETURN_S, SM_NRETURN_F
 *    Args:  kind ∈ {0=RETURN, 1=FRETURN, 2=NRETURN}
 *           cond ∈ {0=unconditional, 1=_S (success-only), 2=_F (fail-only)}
 *           pc   = instruction PC (for unique local label .Lretskip_N)
 *    Macro: RETURN_VARIANT kind, cond, pc
 *    Body:
 *      mov  edi, kind          ; first arg: return kind
 *      mov  esi, cond          ; second arg: condition
 *      call rt_do_return@PLT   ; returns 1=do-ret, 0=skip
 *      test eax, eax
 *      jz   .Lretskip_<pc>
 *      ret
 *    .Lretskip_<pc>:
 *
 * Mode discipline (same as sm_call_fn.c):
 *   BINARY:    SM_RETURN uses standard_blob (mode-3); template not wired.
 *   TEXT:      sm_codegen_x64_emit.c dispatch uses sm_emit_ret /
 *              sm_emit_ret_var (proven path).
 *   MACRO_DEF: emit_sm_return / emit_sm_return_variant called directly;
 *              these are the source of truth for the RETURN / RETURN_VARIANT
 *              macro bodies in sm_macros.s.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-q (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-q / GOAL-MODE4-EMIT
 */

#include "../emitter.h"
#include "../bb_emit.h"

/* emit_sm_return — MACRO_DEF source of truth for the RETURN macro.
 *
 * MACRO_DEF emits:
 *   .macro RETURN
 *       ret
 *   .endm
 *
 * TEXT/BINARY: not called from dispatch; dispatch uses sm_emit_ret. */
void emit_sm_return(emitter_t *e)
{
    (void)e;
    t_comment("SM_RETURN — native return");
    t_macro_begin("RETURN", NULL, 0);
    t_ret();
    t_macro_end();
    t_pad_to_blob_size();
}

/* emit_sm_return_variant — MACRO_DEF source of truth for RETURN_VARIANT.
 *
 * MACRO_DEF emits:
 *   .macro RETURN_VARIANT kind, cond, pc
 *       mov  edi, \kind
 *       mov  esi, \cond
 *       call rt_do_return@PLT
 *       test eax, eax
 *       jz   .Lretskip_\pc
 *       ret
 *   .Lretskip_\pc\():
 *   .endm
 *
 * kind/cond/pc are ignored in MACRO_DEF mode (param refs used instead).
 * TEXT/BINARY: not called from dispatch; dispatch uses sm_emit_ret_var. */
void emit_sm_return_variant(emitter_t *e, int kind, int cond, int pc)
{
    (void)e;

    t_comment("SM_RETURN_VARIANT — conditional/typed return via rt_do_return");

    static const char *const params[] = { "kind", "cond", "pc" };
    t_macro_begin("RETURN_VARIANT", params, 3);

    t_mov_edi_imm32(kind);          /* MACRO_DEF: mov edi, \kind */
    t_mov_esi_imm32(cond);          /* MACRO_DEF: mov esi, \cond */
    t_call_sym_plt("rt_do_return", 0);
    t_test_eax_eax();               /* test eax, eax */
    t_jz_retskip(pc);              /* jz .Lretskip_\pc  (MACRO_DEF: \pc ref) */
    t_ret();
    t_retskip_label(pc);           /* .Lretskip_\pc\():  (MACRO_DEF: \pc ref) */

    t_macro_end();
    t_pad_to_blob_size();
}
