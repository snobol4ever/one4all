/* _XNME/_XFNME  CAPTURE     wrap child; $ writes on every γ; . buffers for Phase-5 commit */
#include "bb_box.h"
#include <stdlib.h>
#include <string.h>

extern void (*NV_SET_fn)(const char*, DESCR_t);
extern void *GC_MALLOC(size_t);
typedef struct {
    bb_box_fn fn; void *ζ; const char *varname;
    int immediate; spec_t pending; int has_pending;
} capture_t;

spec_t bb_capture(capture_t **ζζ, int entry)
{
    capture_t *ζ = *ζζ;
    spec_t child_r;
    if (entry==α)                                                               goto CAP_α;
    if (entry==β)                                                               goto CAP_β;
    CAP_α:          child_r=ζ->fn(&ζ->ζ,α);                                     
                    if (spec_is_empty(child_r))                                 goto CAP_ω;
                                                                                goto CAP_γ_core;
    CAP_β:          child_r=ζ->fn(&ζ->ζ,β);                                     
                    if (spec_is_empty(child_r))                                 goto CAP_ω;
                                                                                goto CAP_γ_core;
    CAP_γ_core:     if (ζ->varname && *ζ->varname && ζ->immediate) {            
                        char *s=GC_MALLOC(child_r.δ+1);                         
                        memcpy(s,child_r.σ,(size_t)child_r.δ); s[child_r.δ]=0;  
                        DESCR_t v={.v=DT_S,.slen=(uint32_t)child_r.δ,.s=s};     
                        NV_SET_fn(ζ->varname,v);                                
                    } else if (ζ->varname && *ζ->varname) {                     
                        ζ->pending=child_r; ζ->has_pending=1; }                 
                                                                                return child_r;
    CAP_ω:          ζ->has_pending=0;                                           return spec_empty;
}

capture_t *bb_capture_new(bb_box_fn fn, void *fζ, const char *var, int imm)
{ capture_t *ζ=calloc(1,sizeof(capture_t)); ζ->fn=fn; ζ->ζ=fζ; ζ->varname=var; ζ->immediate=imm; return ζ; }
