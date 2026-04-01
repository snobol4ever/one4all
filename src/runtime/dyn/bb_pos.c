/*
 * bb_pos.c — POS and RPOS Byrd Boxes (M-DYN-2)
 *
 * POS(n):  succeeds if cursor Δ == n (no advance, no backtrack)
 * RPOS(n): succeeds if cursor Δ == Ω - n
 *
 * Three-column layout:
 *
 *     LABEL:              ACTION                          GOTO
 *     ─────────────────────────────────────────────────────────
 *     POS_alpha:              if (Δ != n)                    → POS_omega
 *                         POS = spec(Σ+Δ, 0);             → POS_gamma
 *     POS_beta:                                             → POS_omega   (no backtrack)
 *     POS_gamma:              return POS;
 *     POS_omega:              return spec_empty;
 */

#include "bb_box.h"
#include <stdlib.h>

/* ── POS ─────────────────────────────────────────────────────────────────── */
typedef struct { int n; } pos_t;

spec_t bb_pos(pos_t **zetazeta, int entry)
{
    pos_t *zeta = *zetazeta;

    if (entry == alpha)                                     goto POS_alpha;
    if (entry == beta)                                     goto POS_beta;

    /*------------------------------------------------------------------------*/
    spec_t         POS;

    POS_alpha:        if (Δ != zeta->n)                        goto POS_omega;
                  POS = spec(Σ+Δ, 0);                    goto POS_gamma;

    POS_beta:                                              goto POS_omega;

    /*------------------------------------------------------------------------*/
    POS_gamma:        return POS;
    POS_omega:        return spec_empty;
}

pos_t *bb_pos_new(int n)
{
    pos_t *zeta = calloc(1, sizeof(pos_t));
    zeta->n = n;
    return zeta;
}

/* ── RPOS ────────────────────────────────────────────────────────────────── */
typedef struct { int n; } rpos_t;

spec_t bb_rpos(rpos_t **zetazeta, int entry)
{
    rpos_t *zeta = *zetazeta;

    if (entry == alpha)                                     goto RPOS_alpha;
    if (entry == beta)                                     goto RPOS_beta;

    /*------------------------------------------------------------------------*/
    spec_t         RPOS;

    RPOS_alpha:       if (Δ != Ω - zeta->n)                   goto RPOS_omega;
                  RPOS = spec(Σ+Δ, 0);                   goto RPOS_gamma;

    RPOS_beta:                                             goto RPOS_omega;

    /*------------------------------------------------------------------------*/
    RPOS_gamma:       return RPOS;
    RPOS_omega:       return spec_empty;
}

rpos_t *bb_rpos_new(int n)
{
    rpos_t *zeta = calloc(1, sizeof(rpos_t));
    zeta->n = n;
    return zeta;
}
