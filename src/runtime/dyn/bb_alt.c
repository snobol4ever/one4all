/* _XOR      ALT         alternation: try each child in order on α; β retries same child */
#include "bb_box.h"
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#define BB_ALT_MAX 16
typedef struct { bb_box_fn fn; void *ζ; } bb_altchild_t;
typedef struct { int n; bb_altchild_t ch[BB_ALT_MAX]; int alt_i; int saved_Δ; spec_t result; } alt_t;

spec_t bb_alt(alt_t **ζζ, int entry)
{
    alt_t *ζ = *ζζ;
    spec_t child_r;
    if (entry==α)                                                               goto ALT_α;
    if (entry==β)                                                               goto ALT_β;
    ALT_α:          ζ->saved_Δ=Δ; ζ->alt_i=1;                                   
                    child_r=ζ->ch[0].fn(&ζ->ch[0].ζ,α);                         
                    if (spec_is_empty(child_r))                                 goto child_α_ω;
                                                                                goto child_α_γ;
    ALT_β:          child_r=ζ->ch[ζ->alt_i-1].fn(&ζ->ch[ζ->alt_i-1].ζ,β);       
                    if (spec_is_empty(child_r))                                 goto ALT_ω;
                                                                                goto child_β_γ;
    child_α_γ:      ζ->result=child_r;                                          goto ALT_γ;
    child_α_ω:      ζ->alt_i++;                                                 
                    if (ζ->alt_i > ζ->n)                                        goto ALT_ω;
                    Δ=ζ->saved_Δ;                                               
                    child_r=ζ->ch[ζ->alt_i-1].fn(&ζ->ch[ζ->alt_i-1].ζ,α);       
                    if (spec_is_empty(child_r))                                 goto child_α_ω;
                                                                                goto child_α_γ;
    child_β_γ:      ζ->result=child_r;                                          goto ALT_γ;
    ALT_γ:                                                                      return ζ->result;
    ALT_ω:                                                                      return spec_empty;
}

alt_t *bb_alt_new(int n, bb_box_fn *fns)
{ alt_t *ζ=calloc(1,sizeof(alt_t)); ζ->n=n; for(int i=0;i<n&&i<BB_ALT_MAX;i++) ζ->ch[i].fn=fns[i]; return ζ; }
