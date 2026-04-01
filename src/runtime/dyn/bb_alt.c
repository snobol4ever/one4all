/*
 * bb_alt.c — ALT (alternation) Byrd Box (M-DYN-2)
 *
 * ALT(left, right): try left; if it fails, restore cursor and try right.
 * On backtrack (β): retry whichever child last succeeded.
 *
 * Pattern:  'Bird' | 'Blue' | LEN(1)
 * SNOBOL4:  P1 | P2 | P3
 *
 * Three-column layout — canonical form from test_sno_1.c §alt:
 *
 *     LABEL:              ACTION                          GOTO
 *     ─────────────────────────────────────────────────────────
 *     alt_α:              ζ->alt_i = 1; (save cursor)    → left_α
 *     alt_β:              dispatch on ζ->alt_i           → left_β / right_β / ...
 *
 *     left_γ:             ζ->alt = left_result;          → alt_γ
 *     left_ω:             ζ->alt_i++; restore cursor;    → right_α
 *     right_γ:            ζ->alt = right_result;         → alt_γ
 *     right_ω:                                           → alt_ω
 *
 *     alt_γ:              return ζ->alt;
 *     alt_ω:              return empty;
 *
 * This supports exactly N alternatives (N=2 shown, generalises to N).
 * State ζ holds: saved cursor, alt_i (which branch is live), result.
 *
 * For the dynamic model: alt_t holds pointers to N child boxes.
 * Each child is called as child->fn(&child->ζ, α) or β.
 */

#include "bb_box.h"
#include <stdlib.h>

/* ── child box reference ─────────────────────────────────────────────────── */
typedef struct bb_child {
    bb_box_fn  fn;     /* box function */
    void      *ζ;      /* box state (allocated by box on α entry) */
} bb_child_t;

/* ── alt box state ───────────────────────────────────────────────────────── */
#define BB_ALT_MAX 16   /* max alternatives in one ALT node */

typedef struct {
    int        n;                   /* number of alternatives */
    bb_child_t children[BB_ALT_MAX];/* child boxes */
    int        alt_i;               /* which child is currently live (1-based) */
    int        saved_Δ;             /* cursor saved at α entry */
    str_t      result;              /* result from whichever child succeeded */
} alt_t;

/* ── bb_alt ──────────────────────────────────────────────────────────────── */
str_t bb_alt(alt_t **ζζ, int entry)
{
    alt_t *ζ = *ζζ;

    if (entry == α)                                     goto ALT_α;
    if (entry == β)                                     goto ALT_β;

    /*------------------------------------------------------------------------*/
    str_t         ALT;
    str_t         child_result;

    ALT_α:        ζ->saved_Δ = Δ;
                  ζ->alt_i   = 1;
                  child_result = ζ->children[0].fn(&ζ->children[0].ζ, α);
                  if (is_empty(child_result))           goto child_ω;
                  else                                  goto child_γ;

    ALT_β:        /* retry: backtrack into the child that last succeeded */
                  child_result = ζ->children[ζ->alt_i-1].fn(
                                     &ζ->children[ζ->alt_i-1].ζ, β);
                  if (is_empty(child_result))           goto child_ω;
                  else                                  goto child_γ;

    child_γ:      ζ->result = child_result;             goto ALT_γ;

    child_ω:      /* this child exhausted — try next, or fail */
                  ζ->alt_i++;
                  if (ζ->alt_i > ζ->n)                 goto ALT_ω;
                  Δ = ζ->saved_Δ;   /* restore cursor before trying next */
                  child_result = ζ->children[ζ->alt_i-1].fn(
                                     &ζ->children[ζ->alt_i-1].ζ, α);
                  if (is_empty(child_result))           goto child_ω;
                  else                                  goto child_γ;

    /*------------------------------------------------------------------------*/
    ALT_γ:        return ζ->result;
    ALT_ω:        return empty;
}

/* ── bb_alt_new ──────────────────────────────────────────────────────────── */
alt_t *bb_alt_new(int n, bb_box_fn *fns)
{
    alt_t *ζ = calloc(1, sizeof(alt_t));
    ζ->n = n;
    for (int i = 0; i < n && i < BB_ALT_MAX; i++) {
        ζ->children[i].fn = fns[i];
        ζ->children[i].ζ  = NULL;
    }
    return ζ;
}
