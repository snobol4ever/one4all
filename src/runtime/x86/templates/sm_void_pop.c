/*
 * templates/sm_void_pop.c — SM_VOID_POP per-opcode template.
 *
 * SM_VOID_POP pops and discards the top-of-stack value.
 * Runtime call: rt_pop_void().
 *
 * Inline x86 form:
 *   call rt_pop_void@PLT
 *
 * No mode-3-vs-mode-4 divergence in semantics.  Mode-3 currently uses
 * the Standard blob (h_pop C handler via call-rax thunk); this template
 * describes the inline form mode-4 uses.
 *
 * Three-way pattern: body calls t_* helpers that read bb_emit_mode.
 * No `emitter_t *e` is read; parameter preserved for caller compat.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6 / Claude Opus 4.7
 * Sprint:  EM-MODE4-IS-MODE3-DUMP / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* unused emitter_t * param; compatibility */
#include "../bb_emit.h"

void emit_sm_void_pop(emitter_t *e)
{
    (void)e;

    t_comment("SM_VOID_POP — pop and discard TOS");
    t_macro_begin("VOID_POP", NULL, 0);

    t_call_sym_plt("rt_pop_void", 0);

    t_macro_end();
    t_pad_to_blob_size();
}
