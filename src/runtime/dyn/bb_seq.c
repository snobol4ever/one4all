/* _XCAT     SEQ         concatenation: left then right; β retries right then left */
#include "bb_box.h"
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wmisleading-indentation"
typedef struct { bb_box_fn fn; void *ζ; } bb_child_t;
typedef struct { bb_child_t left; bb_child_t right; spec_t seq; } seq_t;

spec_t bb_seq(void *zeta, int entry)
{
    seq_t *ζ = zeta;
    spec_t SEQ; spec_t left_r; spec_t right_r;
    if (entry==α)                                                               goto SEQ_α;
    if (entry==β)                                                               goto SEQ_β;
    SEQ_α:          ζ->seq=spec(Σ+Δ,0);                                         
                    left_r=ζ->left.fn(ζ->left.ζ,α);                             
                    if (spec_is_empty(left_r))                                  goto left_ω;
                                                                                goto left_γ;
    SEQ_β:          right_r=ζ->right.fn(ζ->right.ζ,β);                          
                    if (spec_is_empty(right_r))                                 goto right_ω;
                                                                                goto right_γ;
    left_γ:         ζ->seq=spec_cat(ζ->seq,left_r);                             
                    right_r=ζ->right.fn(ζ->right.ζ,α);                          
                    if (spec_is_empty(right_r))                                 goto right_ω;
                                                                                goto right_γ;
    left_ω:                                                                     goto SEQ_ω;
    right_γ:        SEQ=spec_cat(ζ->seq,right_r);                               goto SEQ_γ;
    right_ω:        left_r=ζ->left.fn(ζ->left.ζ,β);                             
                    if (spec_is_empty(left_r))                                  goto left_ω;
                                                                                goto left_γ;
    SEQ_γ:                                                                      return SEQ;
    SEQ_ω:                                                                      return spec_empty;
}

seq_t *bb_seq_new(bb_box_fn lf, void *lz, bb_box_fn rf, void *rz)
{ seq_t *ζ=calloc(1,sizeof(seq_t)); ζ->left.fn=lf; ζ->left.ζ=lz; ζ->right.fn=rf; ζ->right.ζ=rz; return ζ; }
