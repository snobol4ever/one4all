/*
 * bb_lit.c — LIT (literal string) Byrd Box (M-DYN-2)
 *
 * Matches a fixed literal string against the subject at the current cursor.
 * If the subject at Δ starts with the literal, gamma fires and Δ advances.
 * On backtrack (beta), Δ is restored and omega fires.
 *
 * Pattern:  LIT('Bird')
 * SNOBOL4:  'Bird'
 *
 * Three-column layout — one function, all four ports:
 *
 *     LABEL:              ACTION                          GOTO
 *     ─────────────────────────────────────────────────────────
 *     LIT_alpha:              length check                   → omega if too short
 *                         byte-by-byte match             → omega on mismatch
 *                         LIT = spec(Σ+Δ, len); Δ += len; → LIT_gamma
 *     LIT_beta:              Δ -= len;                      → LIT_omega
 *     LIT_gamma:              return LIT;
 *     LIT_omega:              return spec_empty;
 *
 * State zeta: saved cursor advance (= lit_len, for beta restore).
 * Since lit_len is known at box-build time, zeta can hold it directly;
 * alternatively, no zeta needed if we just recompute from the literal length.
 * We use no zeta — the literal length IS the advance, always.
 */

#include "bb_box.h"
#include <string.h>

/* ── lit box state ───────────────────────────────────────────────────────── */
typedef struct {
    const char *lit;     /* literal bytes */
    int         len;     /* literal length */
} lit_t;

/* ── bb_lit ──────────────────────────────────────────────────────────────── */
/*
 * spec_t bb_lit(lit_t **zetazeta, int entry)
 *
 * zetazeta must point to a lit_t pre-filled with lit/len before first alpha call.
 * (Unlike named patterns, LIT has no dynamic state beyond the literal itself.)
 */
spec_t bb_lit(lit_t **zetazeta, int entry)
{
    lit_t *zeta = *zetazeta;

    if (entry == alpha)                                     goto LIT_alpha;
    if (entry == beta)                                     goto LIT_beta;

    /*------------------------------------------------------------------------*/
    spec_t         LIT;

    LIT_alpha:        if (Δ + zeta->len > Ω)                  goto LIT_omega;
                  if (memcmp(Σ + Δ, zeta->lit, (size_t)zeta->len) != 0)
                                                        goto LIT_omega;
                  LIT = spec(Σ+Δ, zeta->len); Δ += zeta->len; goto LIT_gamma;

    LIT_beta:        Δ -= zeta->len;                         goto LIT_omega;

    /*------------------------------------------------------------------------*/
    LIT_gamma:        return LIT;
    LIT_omega:        return spec_empty;
}

/* ── bb_lit_new ──────────────────────────────────────────────────────────── */
/*
 * Construct a lit_t and return a pointer.  Caller owns it.
 * Used by the graph assembler (bb_build.c).
 */
lit_t *bb_lit_new(const char *lit, int len)
{
    lit_t *zeta = calloc(1, sizeof(lit_t));
    zeta->lit   = lit;
    zeta->len   = len;
    return zeta;
}
