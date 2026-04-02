/* _XBAL     BAL         balanced parens — STUB; M-DYN-BAL pending */
#include "bb_box.h"
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
typedef struct { int δ; int start; }  bal_t;

spec_t bb_bal(void *zeta, int entry)
{
    (void)zeta; (void)entry;
    fprintf(stderr,"bb_bal: unimplemented — ω\n");
    return spec_empty;
}

bal_t *bb_bal_new(void)
{ return calloc(1,sizeof(bal_t)); }
