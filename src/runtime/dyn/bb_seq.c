/* _XCAT     SEQ         concatenation: left then right; β retries right then left */
#include "bb_box.h"
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wmisleading-indentation"
typedef struct { bb_box_fn fn; void *state; } bb_child_t;
typedef struct { bb_child_t left; bb_child_t right; spec_t matched; } seq_t;

spec_t bb_seq(void *zeta, int entry)
{
    seq_t *ζ = zeta;
    spec_t SEQ; spec_t lr; spec_t rr;
    if (entry==α)                                                               goto SEQ_α;
    if (entry==β)                                                               goto SEQ_β;
    SEQ_α:          ζ->matched=spec(Σ+Δ,0);                                     
                    lr=ζ->left.fn(ζ->left.state,α);                             
                    if (spec_is_empty(lr))                                      goto left_ω;
                                                                                goto left_γ;
    SEQ_β:          rr=ζ->right.fn(ζ->right.state,β);                           
                    if (spec_is_empty(rr))                                      goto right_ω;
                                                                                goto right_γ;
    left_γ:         ζ->matched=spec_cat(ζ->matched,lr);                         
                    rr=ζ->right.fn(ζ->right.state,α);                           
                    if (spec_is_empty(rr))                                      goto right_ω;
                                                                                goto right_γ;
    left_ω:                                                                     goto SEQ_ω;
    right_γ:        SEQ=spec_cat(ζ->matched,rr);                                goto SEQ_γ;
    right_ω:        lr=ζ->left.fn(ζ->left.state,β);                             
                    if (spec_is_empty(lr))                                      goto left_ω;
                                                                                goto left_γ;
    SEQ_γ:                                                                      return SEQ;
    SEQ_ω:                                                                      return spec_empty;
}

seq_t *bb_seq_new(bb_box_fn lf, void *ls, bb_box_fn rf, void *rs)
{ seq_t *ζ=calloc(1,sizeof(seq_t)); ζ->left.fn=lf; ζ->left.state=ls; ζ->right.fn=rf; ζ->right.state=rs; return ζ; }
