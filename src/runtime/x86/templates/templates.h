/*
 * templates/templates.h — declarations for per-opcode / per-box templates.
 *
 * One declaration per template function.  Templates live in
 * `templates/{sm,bb}_<name>.c` files.  Callers (sm_codegen.c for
 * mode-3 binary; sm_codegen_x64_emit.c for mode-4 text; future
 * regen_macros tool for sm_macros.s) include this header and invoke
 * the appropriate template with an emitter_t constructed for their
 * target backend.
 *
 * Sub-rung -c lands the first template: emit_sm_halt.  Sub-rungs -d
 * through -p add more, one per rung, alternating SM ↔ BB.
 *
 * Authors: Lon Jones Cherryholmes · Claude Opus 4.7
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-c / GOAL-MODE4-EMIT
 */

#ifndef RUNTIME_X86_TEMPLATES_TEMPLATES_H
#define RUNTIME_X86_TEMPLATES_TEMPLATES_H

#include "../emitter.h"

/* ── SM opcode templates ──────────────────────────────────────────────── */

/* SM_HALT — pc++ then ret.  Mode-3 in-process: returns out of
 * sm_jit_run's call frame.  See templates/sm_halt.c for the
 * KNOWN OPEN ARCHITECTURAL QUESTION about mode-4 semantics. */
void emit_sm_halt(emitter_t *e);

/* ── BB box templates ─────────────────────────────────────────────────── */

/* (None yet — sub-rung -d adds the first: emit_bb_xchr.) */

#endif /* RUNTIME_X86_TEMPLATES_TEMPLATES_H */
