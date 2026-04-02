/* _XARBN    ARBNO       zero-or-more greedy; zero-advance guard; β unwinds stack */
#include "bb_box.h"
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#define ARBNO_STACK_MAX 64
typedef struct { spec_t matched; int start; } arbno_frame_t;
typedef struct { bb_box_fn fn; void *state; int depth; arbno_frame_t stack[ARBNO_STACK_MAX]; } arbno_t;

spec_t bb_arbno(void *zeta, int entry)
{
    arbno_t *ζ = zeta;
    spec_t ARBNO; spec_t br; arbno_frame_t *fr;
    if (entry==α)                                                               goto ARBNO_α;
    if (entry==β)                                                               goto ARBNO_β;
    ARBNO_α:        ζ->depth=0; fr=&ζ->stack[0];                                
                    fr->matched=spec(Σ+Δ,0); fr->start=Δ;                       
    ARBNO_try:      br=ζ->fn(ζ->state,α);                                       
                    if (spec_is_empty(br))                                      goto body_ω;
                                                                                goto body_γ;
    ARBNO_β:        if (ζ->depth<=0)                                            goto ARBNO_ω;
                    ζ->depth--; fr=&ζ->stack[ζ->depth]; Δ=fr->start;            goto ARBNO_γ;
    body_γ:         fr=&ζ->stack[ζ->depth];                                     
                    if (Δ==fr->start)                                           goto ARBNO_γ_now;
                    ARBNO=spec_cat(fr->matched,br);                             
                    if (ζ->depth+1<ARBNO_STACK_MAX) {                           
                        ζ->depth++; fr=&ζ->stack[ζ->depth];                     
                        fr->matched=ARBNO; fr->start=Δ; }                       goto ARBNO_try;
    body_ω:         ARBNO=ζ->stack[ζ->depth].matched;                           goto ARBNO_γ;
    ARBNO_γ_now:    ARBNO=ζ->stack[ζ->depth].matched;                           goto ARBNO_γ;
    ARBNO_γ:                                                                    return ARBNO;
    ARBNO_ω:                                                                    return spec_empty;
}

arbno_t *bb_arbno_new(bb_box_fn fn, void *state)
{ arbno_t *ζ=calloc(1,sizeof(arbno_t)); ζ->fn=fn; ζ->state=state; return ζ; }
