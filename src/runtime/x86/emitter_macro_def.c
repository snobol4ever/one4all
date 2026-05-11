/*
 * emitter_macro_def.c — macro-definition backend for the emitter_t vtable
 *
 * Third concrete backend, alongside emitter_text and emitter_binary.
 * Walked over the same per-opcode templates that mode-3 and mode-4 walk,
 * but produces the *body* of each opcode's macro rather than an
 * invocation or live bytes.  Output is consumed by sm_macros.s
 * regeneration so the macro library is generated from the same source of
 * truth as the live emitters — preventing the macro file from drifting
 * out of step with what mode-4's invocations expect.
 *
 * Per the design doc (MIGRATION-MODE4-IS-MODE3-DUMP.md §"Why three
 * backends not two" and §"macro_def is SM-only — the one asymmetry"):
 *   - macro_def is SM-only — BB templates have no .macro layer and
 *     never call this surface.
 *   - macro_def is structurally a text emitter in DEFINITION mode:
 *     the same emission routines, the same vtable, just with the
 *     text_mode field set to TEXT_MODE_DEFINITION so macro_begin /
 *     macro_param_ref / macro_end emit the .macro/.endm shape instead
 *     of an invocation-and-suppress pattern.
 *
 * Sub-rung -b creates this file as a thin wrapper.  No template walks
 * through it yet; sub-rung -v (sm_macros.s regen) is the first caller.
 *
 * Authors: Lon Jones Cherryholmes · Claude Opus 4.7
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-b / GOAL-MODE4-EMIT
 */

#include "emitter.h"
#include <stdio.h>

/* The macro_def backend reuses the text backend in DEFINITION mode.
 * Callers see "emitter_macro_def_*" as the public API; the wiring is
 * minimal here on purpose — DEFINITION-vs-INVOCATION branching lives
 * inside emitter_text.c's macro_begin / macro_param_ref / macro_end. */

emitter_t *emitter_macro_def_new(FILE *out)
{
    return emitter_text_new_mode(out, TEXT_MODE_DEFINITION);
}

/* No separate _free needed — emitter_free() handles the underlying text
 * emitter's ctx + struct.  The wrapper is intentionally trivial. */
