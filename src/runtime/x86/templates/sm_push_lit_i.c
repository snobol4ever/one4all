/*
 * templates/sm_push_lit_i.c — SM_PUSH_LIT_I per-opcode template.
 *
 * SM_PUSH_LIT_I pushes a 64-bit integer literal onto the value stack.
 * Runtime call: rt_push_int(int64_t val).
 *
 * Inline x86 form (both mode-4 text and eventually mode-3 native):
 *   movabs rdi, <val>       ; load immediate into first arg register
 *   call   rt_push_int@PLT  ; push INTVAL(val) onto SM value stack
 *
 * Unlike SM_HALT, this opcode has NO mode-3 vs mode-4 divergence:
 * both modes call rt_push_int at runtime.  Mode-3 currently routes
 * through the Standard blob (h_push_lit_i C handler via call-rax
 * thunk) rather than inline native; the template describes the
 * inline form that mode-4 already uses and that mode-3 will adopt
 * when ME-4+ lands (see sm_codegen.c top comment).
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-f (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * SUB-RUNG -f SCOPE:
 *   - Define the template below.
 *   - Wire mode-4 (sm_codegen_x64_emit.c) through it, replacing the
 *     sm_emit_int64() call in emit_sm_push_lit_i().
 *   - Mode-3 (sm_codegen.c) keeps the Standard blob for now; the
 *     template's binary backend is stubbed for future ME-4+ use.
 *   - Verify mode-4's per-call-site .s output is unchanged.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-f / GOAL-MODE4-EMIT
 */

#include "../emitter.h"

/*
 * emit_sm_push_lit_i — SM_PUSH_LIT_I template.
 *
 * Sprinkle model:
 *   - comment:           text backends render; binary backend ignores.
 *   - emit_mov_rdi_imm64: movabs rdi, val  (binary: 10 bytes; text: line).
 *   - emit_call_sym_plt:  call rt_push_int@PLT (binary: 5 bytes; text: line).
 *
 * Backend productions:
 *   - emitter_binary (future ME-4+):
 *       48 bf <8-byte val>   ; movabs rdi, val
 *       e8 <rel32>           ; call rt_push_int@PLT
 *   - emitter_text (mode-4, TEXT_MODE_INVOCATION):
 *       # SM_PUSH_LIT_I — push integer literal
 *                           movabs          rdi, <val>
 *                           call            rt_push_int@PLT
 *   - emitter_macro_def (TEXT_MODE_DEFINITION — for sm_macros.s regen):
 *       .macro PUSH_INT val
 *                           movabs          rdi, \val
 *                           call            rt_push_int@PLT
 *       .endm
 */
void emit_sm_push_lit_i(emitter_t *e, int64_t val)
{
    if (!e) return;

    EMIT_OPT(e, comment, e, "SM_PUSH_LIT_I — push integer literal");
    EMIT_OPT(e, macro_begin, e, "PUSH_INT", "val", 1);

    emit_mov_rdi_imm64(e, (uint64_t)val);
    emit_call_sym_plt(e, "rt_push_int", 0);

    EMIT_OPT(e, macro_end, e);
    EMIT_OPT(e, pad_to_blob_size, e);
}
