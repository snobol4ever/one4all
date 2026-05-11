/*
 * templates/sm_void_pop.c — SM_VOID_POP per-opcode template.
 *
 * SM_VOID_POP pops and discards the top-of-stack value.
 * Runtime call: rt_pop_void().
 *
 * Inline x86 form:
 *   call rt_pop_void@PLT   ; discard TOS from SM value stack
 *
 * Unlike SM_HALT, this opcode has NO mode-3 vs mode-4 divergence:
 * both modes call rt_pop_void at runtime.  Mode-3 currently routes
 * through the Standard blob (h_pop C handler via call-rax thunk);
 * the template describes the inline form mode-4 uses.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-h (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * SUB-RUNG -h SCOPE:
 *   - Define the template below.
 *   - Wire mode-4 (sm_codegen_x64_emit.c) through it, replacing the
 *     sm_emit_nullary() call in emit_sm_pop().
 *   - Mode-3 (sm_codegen.c) keeps the Standard blob for now.
 *   - Verify mode-4's per-call-site .s output is unchanged.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-h / GOAL-MODE4-EMIT
 */

#include "../emitter.h"

/*
 * emit_sm_void_pop — SM_VOID_POP template.
 *
 * Sprinkle model:
 *   - comment:          text backends render; binary backend ignores.
 *   - macro_begin/end:  macro_def wraps body; text/binary: NO-OP.
 *   - emit_call_sym_plt: call rt_pop_void@PLT (binary: 5 bytes; text: line).
 *
 * Backend productions:
 *   - emitter_binary (future ME-4+):
 *       e8 <rel32>           ; call rt_pop_void@PLT
 *   - emitter_text (mode-4, TEXT_MODE_INVOCATION):
 *       # SM_VOID_POP — pop and discard TOS
 *                           call            rt_pop_void@PLT
 *   - emitter_macro_def (TEXT_MODE_DEFINITION — for sm_macros.s regen):
 *       .macro VOID_POP
 *                           call            rt_pop_void@PLT
 *       .endm
 */
void emit_sm_void_pop(emitter_t *e)
{
    if (!e) return;

    EMIT_OPT(e, comment, e, "SM_VOID_POP — pop and discard TOS");
    EMIT_OPT(e, macro_begin, e, "VOID_POP", NULL, 0);

    emit_call_sym_plt(e, "rt_pop_void", 0);

    EMIT_OPT(e, macro_end, e);
    EMIT_OPT(e, pad_to_blob_size, e);
}
