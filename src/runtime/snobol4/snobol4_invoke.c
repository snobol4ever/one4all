#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gc.h>
#include "snobol4.h"
#include "sil_macros.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t INVOKE_fn(const char *name, DESCR_t *args, int nargs)
{
    if (!name) return FAILDESCR;
    return APPLY_fn(name, args, nargs);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t ARGVAL_fn(DESCR_t d)
{
    if (d.v == DT_FAIL) return FAILDESCR;
    if (d.v == DT_N) {
        if (d.slen == 0 && d.s && *d.s)
            return NV_GET_fn(d.s);
        if (d.slen == 1 && d.ptr)
            return *(DESCR_t *)d.ptr;
        return NULVCL;
    }
    if (d.v == DT_K && d.s)
        return NV_GET_fn(d.s);
    return d;
}
