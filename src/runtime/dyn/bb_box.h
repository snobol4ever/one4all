/*
 * bb_box.h — Dynamic Byrd Box Runtime Types (M-DYN-4)
 *
 * THE CANONICAL FORM — one C function per box, three-column layout:
 *
 *     LABEL:              ACTION                          GOTO
 *     ─────────────────────────────────────────────────────────
 *     BIRD_alpha:         if (Σ[Δ+0] != 'B')             goto BIRD_omega;
 *                         BIRD = spec(Σ+Δ, 4); Δ += 4;   goto BIRD_gamma;
 *     BIRD_beta:          Δ -= 4;                         goto BIRD_omega;
 *     BIRD_gamma:         return BIRD;
 *     BIRD_omega:         return spec_empty;
 *
 * Reference implementations: .github/test_sno_*.c, .github/test_icon.c
 *
 * Every box is:
 *   spec_t BoxName(boxname_t **zetazeta, int entry);
 *
 * entry == alpha (0): fresh entry (go to alpha port)
 * entry == beta  (1): re-entry for backtracking (go to beta port)
 * return spec_is_empty(): omega fired (failure)
 * return non-empty:       gamma fired (success, value = matched substring)
 *
 * Global match state (shared, like SNOBOL4's implicit subject):
 *   Σ  — subject string pointer
 *   Δ  — cursor (current match position)
 *   Ω  — subject length
 */

#ifndef BB_BOX_H
#define BB_BOX_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ── spec_t — the universal value type ─────────────────────────────────── */
/*
 * spec_t represents a substring of the subject (or the empty/fail sentinel).
 * sigma = pointer into subject string, delta = length.
 * empty (sigma == NULL) signals failure (omega port fired).
 */
typedef struct { const char *sigma; int delta; } spec_t;

/* The failure sentinel */
static const spec_t spec_empty = { (const char *)0, 0 };

/* Construct a spec_t from pointer and length */
static inline spec_t spec(const char *s, int d) { return (spec_t){ s, d }; }

/* Concatenate two substrings (they must be contiguous in the subject) */
static inline spec_t spec_cat(spec_t x, spec_t y) { return (spec_t){ x.sigma, x.delta + y.delta }; }

/* Test for failure sentinel */
static inline bool spec_is_empty(spec_t x) { return x.sigma == (const char *)0; }

/* ── entry ports ────────────────────────────────────────────────────────── */
#define alpha 0   /* fresh entry */
#define beta  1   /* backtrack re-entry */

/* ── global match state ─────────────────────────────────────────────────── */
/*
 * Set by the statement executor (Phase 1: build subject) and shared
 * across all boxes during a single match.  Cursor Δ is mutated by each
 * box as it matches forward; restored on backtrack.
 */
extern const char *Σ;   /* subject string */
extern int         Δ;   /* cursor */
extern int         Ω;   /* subject length */

/* ── state allocator (from test_sno_3.c §enter) ─────────────────────────── */
/*
 * Each box that needs per-invocation state (cursor saves, loop vars)
 * gets a typed struct zeta allocated once and reused across calls.
 * On alpha: zeroed.  On beta: left intact (holds saved state).
 */
static inline void *bb_enter(void **zetazeta, size_t size) {
    void *zeta = *zetazeta;
    if (size) {
        if (zeta) memset(zeta, 0, size);
        else      zeta = *zetazeta = calloc(1, size);
    }
    return zeta;
}
#define BB_ENTER(ref, T)  ((T *)bb_enter((void **)(ref), sizeof(T)))

/* ── box function signature ──────────────────────────────────────────────── */
/*
 * Every box is called as:
 *   spec_t result = BoxName(&zeta, alpha);  // fresh
 *   spec_t result = BoxName(&zeta, beta);   // backtrack
 *
 * Caller checks spec_is_empty(result) to know if gamma or omega fired.
 */
typedef spec_t (*bb_box_fn)(void **zetazeta, int entry);

#endif /* BB_BOX_H */
