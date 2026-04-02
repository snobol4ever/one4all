/* _XBRKX    BREAKX      like BRK but fails on zero advance */
#include "bb_box.h"
#include <stdlib.h>
#include <string.h>

typedef struct { const char *chars; int δ; }  brkx_t;

spec_t bb_breakx(brkx_t **ζζ, int entry)
{
    brkx_t *ζ = *ζζ;
    spec_t BREAKX;
    if (entry==α)                                                               goto BREAKX_α;
    if (entry==β)                                                               goto BREAKX_β;
    BREAKX_α:       ζ->δ=0;                                                     
                    while (Δ+ζ->δ<Ω && !strchr(ζ->chars,Σ[Δ+ζ->δ])) ζ->δ++;     
                    if (ζ->δ==0 || Δ+ζ->δ>=Ω)                                   goto BREAKX_ω;
                    BREAKX = spec(Σ+Δ,ζ->δ); Δ += ζ->δ;                         goto BREAKX_γ;
    BREAKX_β:       Δ -= ζ->δ;                                                  goto BREAKX_ω;
    BREAKX_γ:                                                                   return BREAKX;
    BREAKX_ω:                                                                   return spec_empty;
}

brkx_t *bb_breakx_new(const char *chars)
{ brkx_t *ζ=calloc(1,sizeof(brkx_t)); ζ->chars=chars; return ζ; }
