/* _XDSAR/_XVAR  DVAR        *VAR/VAR — re-resolve live value on every α */
#include "bb_box.h"
#include <stdlib.h>
#include <string.h>

extern DESCR_t (*NV_GET_fn)(const char*);
extern bb_node_t bb_build(void*);
typedef struct { const char *name; bb_box_fn child_fn; void *child_ζ; size_t child_ζ_size; } dvar_t;

spec_t bb_deferred_var(dvar_t **ζζ, int entry)
{
    dvar_t *ζ = *ζζ;
    spec_t DVAR;
    if (entry==α)                                                               goto DVAR_α;
    if (entry==β)                                                               goto DVAR_β;
    DVAR_α:         { DESCR_t val=NV_GET_fn(ζ->name); int rebuilt=0;            
                      if (val.v==DT_P && val.p && val.p!=ζ->child_ζ) {          
                          bb_node_t c=bb_build(val.p);                          
                          ζ->child_fn=c.fn; ζ->child_ζ=c.ζ; ζ->child_ζ_size=c.ζ_size; rebuilt=1; }
                      else if (val.v==DT_S && val.s) {                          
                          _lit_t *lz=(_lit_t*)ζ->child_ζ;                       
                          if (!lz||lz->lit!=val.s) {                            
                              lz=calloc(1,sizeof(_lit_t)); lz->lit=val.s; lz->len=(int)strlen(val.s);
                              ζ->child_fn=(bb_box_fn)bb_lit; ζ->child_ζ=lz;     
                              ζ->child_ζ_size=sizeof(_lit_t); rebuilt=1; } }    
                      if (!rebuilt&&ζ->child_ζ&&ζ->child_ζ_size                 
                          &&ζ->child_fn!=(bb_box_fn)bb_lit)                     
                          memset(ζ->child_ζ,0,ζ->child_ζ_size); }               
                    if (!ζ->child_fn)                                           goto DVAR_ω;
                    DVAR=ζ->child_fn(&ζ->child_ζ,α);                            
                    if (spec_is_empty(DVAR))                                    goto DVAR_ω;
                                                                                goto DVAR_γ;
    DVAR_β:         if (!ζ->child_fn)                                           goto DVAR_ω;
                    DVAR=ζ->child_fn(&ζ->child_ζ,β);                            
                    if (spec_is_empty(DVAR))                                    goto DVAR_ω;
                                                                                goto DVAR_γ;
    DVAR_γ:                                                                     return DVAR;
    DVAR_ω:                                                                     return spec_empty;
}

dvar_t *bb_dvar_new(const char *name)
{ dvar_t *ζ=calloc(1,sizeof(dvar_t)); ζ->name=name; return ζ; }
