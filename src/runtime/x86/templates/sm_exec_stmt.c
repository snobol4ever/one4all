/*
 * templates/sm_exec_stmt.c — expression and statement-execution templates.
 *
 * Covers three opcodes that manage expression evaluation and pattern
 * statement execution:
 *
 *   SM_PUSH_EXPRESSION  PUSH_EXPRESSION  rt_push_expression_descr
 *     a[0] = entry point (chunk/fn pointer), a[1].i = arity
 *     Pushes an expression descriptor onto the value stack.
 *     Macro body (SM_TPL_PUSH_EXPRESSION):
 *       .macro PUSH_EXPRESSION entry, arity
 *           movabs rdi, \entry
 *           mov    esi, \arity
 *           call   rt_push_expression_descr@PLT
 *       .endm
 *
 *   SM_CALL_EXPRESSION  CALL_EXPRESSION  (no runtime symbol — direct call)
 *     a[0] = target chunk label
 *     Calls a pre-built expression chunk directly.
 *     Macro body (SM_TPL_CALL_EXPRESSION):
 *       .macro CALL_EXPRESSION tgt
 *           call \tgt
 *       .endm
 *
 *   SM_EXEC_STMT  EXEC_STMT_VARIANT  rt_match_variant
 *     a[0].s = subject name (strtab label), a[1].i = has_repl
 *     Executes a pattern statement against TOS subject/pattern/repl.
 *     Macro body (SM_TPL_EXEC_VAR):
 *       .macro EXEC_STMT_VARIANT has_repl, subj_lbl
 *           lea  rdi, [rip + \subj_lbl]   ; subject name ptr
 *           mov  esi, \has_repl           ; has replacement flag
 *           call rt_match_variant@PLT
 *       .endm
 *
 * Pattern: MACRO_DEF source of truth only.  TEXT dispatch uses
 * sm_emit_push_expression / sm_emit_call_expression / sm_emit_exec_var
 * in sm_codegen_x64_emit.c.  BINARY uses standard_blob in sm_codegen.c.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-s (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-s / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* unused emitter_t * param; caller compat */
#include "../bb_emit.h"

void emit_sm_push_expression(emitter_t *e, uint64_t entry_ptr, int arity)
{
    (void)e;
    t_comment("SM_PUSH_EXPRESSION — push expression descriptor (entry, arity)");

    static const char *const params[] = { "entry", "arity" };
    t_macro_begin("PUSH_EXPRESSION", params, 2);

    /* MACRO_DEF: movabs rdi, \entry */
    t_movabs_rdi_entry(entry_ptr);
    /* MACRO_DEF: mov esi, \arity */
    t_mov_esi_imm32(arity);
    t_call_sym_plt("rt_push_expression_descr", 0);

    t_macro_end();
    t_pad_to_blob_size();
}

void emit_sm_call_expression(emitter_t *e, const char *tgt_sym)
{
    (void)e;
    t_comment("SM_CALL_EXPRESSION — call expression chunk directly");

    static const char *const params[] = { "tgt" };
    t_macro_begin("CALL_EXPRESSION", params, 1);

    /* MACRO_DEF: call \tgt */
    t_call_sym_param(tgt_sym);

    t_macro_end();
    t_pad_to_blob_size();
}

void emit_sm_exec_stmt(emitter_t *e,
                        const char *subj_lbl, uint64_t subj_ptr,
                        int has_repl)
{
    (void)e;
    t_comment("SM_EXEC_STMT — execute pattern statement via rt_match_variant");

    static const char *const params[] = { "has_repl", "subj_lbl" };
    t_macro_begin("EXEC_STMT_VARIANT", params, 2);

    /* MACRO_DEF: lea rdi, [rip + \subj_lbl] */
    t_lea_rdi_strtab_sym(subj_lbl, subj_ptr);
    /* MACRO_DEF: mov esi, \has_repl */
    t_mov_esi_imm32(has_repl);
    t_call_sym_plt("rt_match_variant", 0);

    t_macro_end();
    t_pad_to_blob_size();
}
