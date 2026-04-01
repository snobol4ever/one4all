/*
 * bb_arbno.c — ARBNO (zero-or-more) Byrd Box (M-DYN-2)
 *
 * ARBNO(body): match body zero or more times.
 * Possessive zero-advance guard: if body matched but cursor didn't move,
 * stop immediately (prevents infinite loop).
 *
 * Pattern:  ARBNO('Bird' | 'Blue' | LEN(1))
 * SNOBOL4:  ARBNO(P)
 *
 * Three-column layout — canonical form from test_sno_1.c §ARBNO:
 *
 *     LABEL:              ACTION                          GOTO
 *     ─────────────────────────────────────────────────────────
 *     ARBNO_α:            ζ = &stack[ARBNO_i=0];
 *                         ζ->ARBNO = spec(Σ+Δ,0);         → alt_α    (body α)
 *     ARBNO_β:            ζ = &stack[++ARBNO_i];
 *                         ζ->ARBNO = ARBNO;              → alt_α    (body α again)
 *     alt_γ:              ARBNO = spec_cat(ζ->ARBNO, alt);    → ARBNO_γ  (accumulated so far)
 *     alt_ω:              if (ARBNO_i <= 0)              → ARBNO_ω
 *                         ARBNO_i--; ζ = &stack[ARBNO_i];→ alt_β    (backtrack into body)
 *
                      *     ARBNO_γ:                          return ARBNO;
                      *     ARBNO_ω:                          return spec_empty;
 *
 * The ARBNO stack records one entry per successful body match.
 * On β, we walk back through the stack retrying each body iteration.
 *
 * Zero-advance guard (SNOBOL4 spec §3): if body matched zero chars,
 * ARBNO succeeds immediately at current position (prevents infinite loop).
 */

#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "bb_box.h"
#include <stdlib.h>

#define ARBNO_STACK_MAX 64

typedef struct {
    spec_t   ARBNO;    /* accumulated match up to this level */
    int     saved_Δ;  /* cursor before this iteration's body match */
} arbno_frame_t;

typedef struct {
    bb_box_fn    body_fn;                    /* body box function */
    void        *body_ζ;                     /* body box state */
    int          ARBNO_i;                    /* current stack depth */
    arbno_frame_t stack[ARBNO_STACK_MAX];    /* frame stack */
} arbno_t;

/* ── bb_arbno ────────────────────────────────────────────────────────────── */
spec_t bb_arbno(arbno_t **ζζ, int entry)
{
    arbno_t *ζ = *ζζ;

    if (entry == α)                                 goto ARBNO_α;
    if (entry == β)                                 goto ARBNO_β;

    /*------------------------------------------------------------------------*/
    spec_t             ARBNO;
    spec_t             body_r;
    arbno_frame_t    *frame;

    ARBNO_α:      ζ->ARBNO_i          = 0;
                  frame               = &ζ->stack[0];
                  frame->ARBNO        = spec(Σ+Δ, 0);
                  frame->saved_Δ      = Δ;
                  /* Greedily accumulate all repetitions on α.
                   * Each successful body iteration loops back here. */
    ARBNO_try:    body_r = ζ->body_fn(&ζ->body_ζ, α);
                      if (spec_is_empty(body_r))              goto body_ω;
                      else                                    goto body_γ;

    ARBNO_β:      /* β: backtrack — undo last successful iteration */
                  if (ζ->ARBNO_i <= 0)                        goto ARBNO_ω;
                  ζ->ARBNO_i--;
                  frame               = &ζ->stack[ζ->ARBNO_i];
                  Δ                   = frame->saved_Δ;
                  goto ARBNO_γ;

    body_γ:       frame  = &ζ->stack[ζ->ARBNO_i];
                  /* zero-advance guard: if body matched 0 chars, stop accumulating */
                      if (Δ == frame->saved_Δ)                goto ARBNO_γ_now;
                  ARBNO  = spec_cat(frame->ARBNO, body_r);
                  /* advance to next iteration */
                  if (ζ->ARBNO_i + 1 < ARBNO_STACK_MAX) {
                      ζ->ARBNO_i++;
                      frame = &ζ->stack[ζ->ARBNO_i];
                      frame->ARBNO   = ARBNO;
                      frame->saved_Δ = Δ;
                  }
                  goto ARBNO_try;   /* greedily try another iteration */

    ARBNO_γ_now:      ARBNO  = ζ->stack[ζ->ARBNO_i].ARBNO;    goto ARBNO_γ;

    body_ω:       /* body failed — return best accumulated match so far */
                  ARBNO  = ζ->stack[ζ->ARBNO_i].ARBNO;        goto ARBNO_γ;

    /*------------------------------------------------------------------------*/
    ARBNO_γ:                                                  return ARBNO;
    ARBNO_ω:                                                  return spec_empty;
}

/* ── bb_arbno_new ────────────────────────────────────────────────────────── */
arbno_t *bb_arbno_new(bb_box_fn body_fn)
{
    arbno_t *ζ = calloc(1, sizeof(arbno_t));
    ζ->body_fn = body_fn;
    ζ->body_ζ  = NULL;
                                                              return ζ;
}
