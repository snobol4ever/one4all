/*
 * bb_seq.c — SEQ (sequence) Byrd Box (M-DYN-2)
 *
 * SEQ(left, right): match left, then right at the new cursor position.
 * On backtrack: retry right first, then if right exhausted retry left.
 *
 * Pattern:  POS(0) 'Bird' RPOS(0)
 * SNOBOL4:  implicit concatenation
 *
 * Three-column layout — canonical form from test_sno_*.c §seq:
 *
 *     LABEL:              ACTION                          GOTO
 *     ─────────────────────────────────────────────────────────
 *     seq_α:              seq = str(Σ+Δ, 0);             → left_α
 *     seq_β:                                             → right_β
 *     left_γ:             seq = cat(seq, left);          → right_α
 *     left_ω:                                            → seq_ω
 *     right_γ:            seq = cat(seq, right);         → seq_γ
 *     right_ω:                                           → left_β
 *     seq_γ:              return seq;
 *     seq_ω:              return empty;
 *
 * State ζ: the accumulated seq value, child states.
 */

#include "bb_box.h"
#include <stdlib.h>

/* forward from bb_alt.c */
typedef struct bb_child bb_child_t;
typedef struct { bb_box_fn fn; void *ζ; } bb_child2_t;

typedef struct {
    bb_child2_t left;
    bb_child2_t right;
    str_t       seq;
} seq_t;

/* ── bb_seq ──────────────────────────────────────────────────────────────── */
str_t bb_seq(seq_t **ζζ, int entry)
{
    seq_t *ζ = *ζζ;

    if (entry == α)                                     goto SEQ_α;
    if (entry == β)                                     goto SEQ_β;

    /*------------------------------------------------------------------------*/
    str_t         SEQ;
    str_t         left_r;
    str_t         right_r;

    SEQ_α:        ζ->seq = str(Σ+Δ, 0);
                  left_r = ζ->left.fn(&ζ->left.ζ, α);
                  if (is_empty(left_r))                 goto left_ω;
                  else                                  goto left_γ;

    SEQ_β:        right_r = ζ->right.fn(&ζ->right.ζ, β);
                  if (is_empty(right_r))                goto right_ω;
                  else                                  goto right_γ;

    left_γ:       ζ->seq = cat(ζ->seq, left_r);
                  right_r = ζ->right.fn(&ζ->right.ζ, α);
                  if (is_empty(right_r))                goto right_ω;
                  else                                  goto right_γ;

    left_ω:                                             goto SEQ_ω;

    right_γ:      SEQ = cat(ζ->seq, right_r);          goto SEQ_γ;

    right_ω:      left_r = ζ->left.fn(&ζ->left.ζ, β);
                  if (is_empty(left_r))                 goto left_ω;
                  else                                  goto left_γ;

    /*------------------------------------------------------------------------*/
    SEQ_γ:        return SEQ;
    SEQ_ω:        return empty;
}

/* ── bb_seq_new ──────────────────────────────────────────────────────────── */
seq_t *bb_seq_new(bb_box_fn left_fn, bb_box_fn right_fn)
{
    seq_t *ζ = calloc(1, sizeof(seq_t));
    ζ->left.fn  = left_fn;
    ζ->left.ζ   = NULL;
    ζ->right.fn = right_fn;
    ζ->right.ζ  = NULL;
    return ζ;
}
