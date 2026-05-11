/*
 * templates/sm_halt.c — SM_HALT per-opcode template.
 *
 * SM_HALT terminates an SM program.  Mode-3 (binary) bumps the program
 * counter and returns from sm_jit_run; mode-4 (text/macro_def) emits the
 * same instruction sequence — the divergence to `call rt_halt_tos@PLT`
 * that earlier sub-rungs treated as a sanctioned exception is handled
 * (when needed) at the link stage, not inside the template.
 *
 * Three-way pattern: the template body calls free-standing helpers that
 * read bb_emit_mode internally.  No `emitter_t *e` is read inside the
 * body; the parameter is preserved on the signature for backward compat
 * with existing call sites (sm_codegen.c: emit_halt_blob_via_template;
 * demo_template_productions.c; test_template_byte_identity.c) and will
 * be removed once those callers drop it too.
 *
 * Byte sequence (BINARY):  41 ff 45 14 c3   (5 bytes)
 *   41 ff 45 14   inc dword [r13 + 20]   ; st->pc++
 *   c3            ret                    ; unwind to sm_jit_run
 *
 * Text rendering (TEXT, MACRO_DEF):
 *     inc       dword ptr [r13 + 20]
 *     ret
 *
 * Authors: Lon Jones Cherryholmes · Claude Opus 4.7
 * Sprint:  EM-MODE4-IS-MODE3-DUMP / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* still required for the unused emitter_t * param */
#include "../bb_emit.h"

void emit_sm_halt(emitter_t *e)
{
    (void)e;   /* parameter unused — three-way helpers read bb_emit_mode */

    t_comment("SM_HALT — exit sm_jit_run via ret");
    t_inc_mem_r13_disp8(20);     /* inc dword [r13+20] — st->pc++ */
    t_ret();                     /* ret — return to sm_jit_run frame */
    t_pad_to_blob_size();        /* no-op today; hook for fixed-size dispatch */
}
