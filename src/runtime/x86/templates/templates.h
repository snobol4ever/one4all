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

/* Dependency pull-ins for everything this header declares.  Putting
 * them at the top of the header (rather than scattered between
 * declarations) means callers get all the types they need by including
 * this one header. */
#include "../snobol4.h"          /* DESCR_t, SPEC_t (transitive deps of patnd) */
#include "../emitter.h"          /* emitter_t */
#include "../bb_emit.h"          /* bb_label_t */
#include "../snobol4_patnd.h"    /* PATND_t */

/* ── SM opcode templates ──────────────────────────────────────────────── */

/* SM_HALT — pc++ then ret.  Mode-3 in-process: returns out of
 * sm_jit_run's call frame.  See templates/sm_halt.c for the
 * KNOWN OPEN ARCHITECTURAL QUESTION about mode-4 semantics. */
void emit_sm_halt(emitter_t *e);

/* ── BB box templates ─────────────────────────────────────────────────── */

/* XCHR — literal-string-match box.  Sub-rung -d (2026-05-11). */
void emit_bb_xchr(emitter_t *e, PATND_t *p,
                  bb_label_t *lbl_succ, bb_label_t *lbl_fail,
                  bb_label_t *lbl_β);

#endif /* RUNTIME_X86_TEMPLATES_TEMPLATES_H */
