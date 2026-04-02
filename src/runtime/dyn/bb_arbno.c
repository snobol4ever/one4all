/* _XARBN    ARBNO       zero-or-more greedy; zero-advance guard; β unwinds stack */
#include "bb_box.h"
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#define ARBNO_STACK_MAX 64
typedef struct { spec_t ARBNO; int saved_Δ; } arbno_frame_t;
typedef struct { bb_box_fn body_fn; void *body_ζ; int ARBNO_i; arbno_frame_t stack[ARBNO_STACK_MAX]; } arbno_t;

spec_t bb_arbno(void *zeta, int entry)
{
    arbno_t *ζ = zeta;
    spec_t ARBNO; spec_t body_r; arbno_frame_t *frame;
    if (entry==α)                                                               goto ARBNO_α;
    if (entry==β)                                                               goto ARBNO_β;
    ARBNO_α:        ζ->ARBNO_i=0; frame=&ζ->stack[0];                           
                    frame->ARBNO=spec(Σ+Δ,0); frame->saved_Δ=Δ;                 
    ARBNO_try:      body_r=ζ->body_fn(ζ->body_ζ,α);                             
                    if (spec_is_empty(body_r))                                  goto body_ω;
                                                                                goto body_γ;
    ARBNO_β:        if (ζ->ARBNO_i<=0)                                          goto ARBNO_ω;
                    ζ->ARBNO_i--; frame=&ζ->stack[ζ->ARBNO_i]; Δ=frame->saved_Δ;goto ARBNO_γ;
    body_γ:         frame=&ζ->stack[ζ->ARBNO_i];                                
                    if (Δ==frame->saved_Δ)                                      goto ARBNO_γ_now;
                    ARBNO=spec_cat(frame->ARBNO,body_r);                        
                    if (ζ->ARBNO_i+1<ARBNO_STACK_MAX) {                         
                        ζ->ARBNO_i++; frame=&ζ->stack[ζ->ARBNO_i];              
                        frame->ARBNO=ARBNO; frame->saved_Δ=Δ; }                 goto ARBNO_try;
    body_ω:         ARBNO=ζ->stack[ζ->ARBNO_i].ARBNO;                           goto ARBNO_γ;
    ARBNO_γ_now:    ARBNO=ζ->stack[ζ->ARBNO_i].ARBNO;                           goto ARBNO_γ;
    ARBNO_γ:                                                                    return ARBNO;
    ARBNO_ω:                                                                    return spec_empty;
}

arbno_t *bb_arbno_new(bb_box_fn body_fn)
{ arbno_t *ζ=calloc(1,sizeof(arbno_t)); ζ->body_fn=body_fn; return ζ; }
