/*
 * templates/sm_label_stno.c — SM_LABEL and SM_STNO no-op marker templates.
 *
 * SM_LABEL — control-flow entry point marker.
 *   BINARY: no-op (the .LpcN label is placed by the label system; no bytes).
 *   TEXT:   one three-column line with "LABEL" in col 2; consumes .LpcN:.
 *           Macro body is empty (.macro LABEL\n.endm) → assembles to nothing.
 *
 * SM_STNO — source-statement boundary marker.
 *   BINARY: no-op.
 *   TEXT:   120-char #= banner showing stmt N / line L / source text,
 *           then one three-column line with "STNO" in col 2.
 *           Macro body is empty → assembles to nothing.
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-o (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-o / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* emitter_t * param — kept for caller compat */
#include "../bb_emit.h"

void emit_sm_label(emitter_t *e)
{
    (void)e;
    /* BINARY: no-op; label placement is the caller's responsibility.
     * TEXT/MACRO_DEF: one three-column line, macro name "LABEL" in col 2. */
    t_noop_macro("LABEL");
}

void emit_sm_stno(emitter_t *e, int stno, int lineno, const char *src_text)
{
    (void)e;
    /* BINARY: no-op for both banner and marker.
     * TEXT/MACRO_DEF: major banner + STNO macro name in col 2. */
    t_banner_stno(stno, lineno, src_text);
    t_noop_macro("STNO");
}
