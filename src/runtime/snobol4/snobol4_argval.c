#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gc.h>
#include "snobol4.h"
#include "sil_macros.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t VARVAL_d_fn(DESCR_t d)
{
    if (d.v == DT_FAIL) return FAILDESCR;
    if (d.v == DT_N) {
        if (d.slen == 0 && d.s && *d.s) d = NV_GET_fn(d.s);
        else if (d.slen == 1 && d.ptr)  d = *(DESCR_t *)d.ptr;
        else return NULVCL;
    }
    if (d.v == DT_K && d.s) d = NV_GET_fn(d.s);
    if (d.v == DT_FAIL) return FAILDESCR;
    if (d.v == DT_S || d.v == DT_SNUL) return d;
    if (d.v == DT_I) {
        char buf[64];
        snprintf(buf, sizeof buf, "%lld", (long long)d.i);
        return STRVAL(GC_strdup(buf));
    }
    if (d.v == DT_R) {
        char buf[64];
        snprintf(buf, sizeof buf, "%g", d.r);
        return STRVAL(GC_strdup(buf));
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t INTVAL_fn(DESCR_t d)
{
    if (d.v == DT_FAIL) return FAILDESCR;
    if (d.v == DT_N) {
        if (d.slen == 0 && d.s && *d.s) d = NV_GET_fn(d.s);
        else if (d.slen == 1 && d.ptr)  d = *(DESCR_t *)d.ptr;
        else return FAILDESCR;
    }
    if (d.v == DT_K && d.s) d = NV_GET_fn(d.s);
    if (d.v == DT_FAIL) return FAILDESCR;
    if (d.v == DT_I)   return d;
    if (d.v == DT_R)   return INTVAL((int64_t)d.r);
    if (d.v == DT_S || d.v == DT_SNUL) {
        const char *s = d.s ? d.s : "";
        if (!*s) return FAILDESCR;
        char *end;
        long long n = strtoll(s, &end, 10);
        while (*end == ' ' || *end == '\t') end++;
        if (*end != '\0') return FAILDESCR;
        return INTVAL((int64_t)n);
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t PATVAL_fn(DESCR_t d)
{
    if (d.v == DT_FAIL) return FAILDESCR;
    if (d.v == DT_N) {
        if (d.slen == 0 && d.s && *d.s) d = NV_GET_fn(d.s);
        else if (d.slen == 1 && d.ptr)  d = *(DESCR_t *)d.ptr;
        else return FAILDESCR;
    }
    if (d.v == DT_K && d.s) d = NV_GET_fn(d.s);
    if (d.v == DT_FAIL) return FAILDESCR;
    if (d.v == DT_P) return d;
    if (d.v == DT_E) {
        extern DESCR_t EVAL_fn(DESCR_t);
        DESCR_t val = EVAL_fn(d);
        if (IS_FAIL_fn(val)) return FAILDESCR;
        if (val.v == DT_P) return val;
        d = val;
    }
    DESCR_t s = VARVAL_d_fn(d);
    if (s.v == DT_FAIL) return FAILDESCR;
    {
        extern DESCR_t pat_lit(const char *s);
        const char *sp = (s.v == DT_SNUL || !s.s) ? "" : s.s;
        return pat_lit(sp);
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t VARVUP_fn(DESCR_t d)
{
    d = VARVAL_d_fn(d);
    if (d.v == DT_FAIL) return FAILDESCR;
    return d;
}
