/* _XOR      ALT         alternation: try each child on α; β retries same child only */
#include "bb_box.h"
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#define BB_ALT_MAX 16
typedef struct { bb_box_fn fn; void *state; } bb_altchild_t;
typedef struct { int n; bb_altchild_t children[BB_ALT_MAX]; int current; int position; spec_t result; } alt_t;

spec_t bb_alt(void *zeta, int entry)
{
    alt_t *ζ = zeta;
    spec_t cr;
    if (entry==α)                                                               goto ALT_α;
    if (entry==β)                                                               goto ALT_β;
    ALT_α:          ζ->position=Δ; ζ->current=1;                                
                    cr=ζ->children[0].fn(ζ->children[0].state,α);               
                    if (spec_is_empty(cr))                                      goto child_α_ω;
                                                                                goto child_α_γ;
    ALT_β:          cr=ζ->children[ζ->current-1].fn(ζ->children[ζ->current-1].state,β);
                    if (spec_is_empty(cr))                                      goto ALT_ω;
                                                                                goto child_β_γ;
    child_α_γ:      ζ->result=cr;                                               goto ALT_γ;
    child_α_ω:      ζ->current++;                                               
                    if (ζ->current > ζ->n)                                      goto ALT_ω;
                    Δ=ζ->position;                                              
                    cr=ζ->children[ζ->current-1].fn(ζ->children[ζ->current-1].state,α);
                    if (spec_is_empty(cr))                                      goto child_α_ω;
                                                                                goto child_α_γ;
    child_β_γ:      ζ->result=cr;                                               goto ALT_γ;
    ALT_γ:                                                                      return ζ->result;
    ALT_ω:                                                                      return spec_empty;
}

alt_t *bb_alt_new(int n, bb_box_fn *fns)
{ alt_t *ζ=calloc(1,sizeof(alt_t)); ζ->n=n; for(int i=0;i<n&&i<BB_ALT_MAX;i++) ζ->children[i].fn=fns[i]; return ζ; }
