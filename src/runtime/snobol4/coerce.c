#include "coerce.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <gc/gc.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *real_str(double r, char *buf, int bufsz);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int64_t     to_int(DESCR_t v);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
double      to_real(DESCR_t v);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t descr_to_str_icn(DESCR_t d)
{
    if (IS_INT_fn(d)) {
        char *nbuf = GC_malloc(32);
        snprintf(nbuf, 32, "%lld", (long long)d.i);
        return STRVAL(nbuf);
    }
    if (IS_REAL_fn(d)) {
        char tmp[64];
        real_str(d.r, tmp, sizeof tmp);
        size_t len = strlen(tmp);
        char *nbuf = GC_malloc(len + 1);
        memcpy(nbuf, tmp, len + 1);
        return STRVAL(nbuf);
    }
    if (IS_STR_fn(d) || d.v == DT_SNUL) return d;
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t shared_arith(DESCR_t l, DESCR_t r, sm_opcode_t op)
{
    if (l.v == DT_I && r.v == DT_I) {
        switch (op) {
        case SM_ADD: return INTVAL(l.i + r.i);
        case SM_SUB: return INTVAL(l.i - r.i);
        case SM_MUL: return INTVAL(l.i * r.i);
        case SM_DIV:
            if (r.i == 0) { fprintf(stderr, "Error 2: division by zero\n"); return FAILDESCR; }
            return INTVAL(l.i / r.i);
        case SM_MOD:
            if (r.i == 0) { fprintf(stderr, "Error 2: division by zero\n"); return FAILDESCR; }
            return INTVAL(l.i % r.i);
        case SM_EXP:
            if (r.i >= 0) {
                extern int g_icn_jcon;
                extern int g_lang;
                int64_t base = l.i, exp = r.i, res = 1;
                while (exp-- > 0) res *= base;
                return (g_lang != 1 || g_icn_jcon) ? INTVAL(res) : REALVAL((double)res);
            }
            return REALVAL(pow((double)l.i, (double)r.i));
        default: break;
        }
    }
    double ld = to_real(l), rd = to_real(r);
    switch (op) {
    case SM_ADD: return REALVAL(ld + rd);
    case SM_SUB: return REALVAL(ld - rd);
    case SM_MUL: return REALVAL(ld * rd);
    case SM_DIV:
        if (rd == 0.0) { fprintf(stderr, "Error 2: division by zero\n"); return FAILDESCR; }
        return REALVAL(ld / rd);
    case SM_MOD:
        if (rd == 0.0) { fprintf(stderr, "Error 2: division by zero\n"); return FAILDESCR; }
        return REALVAL(fmod(ld, rd));
    case SM_EXP: return REALVAL(pow(ld, rd));
    default:     return FAILDESCR;
    }
}
