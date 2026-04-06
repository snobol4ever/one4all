/*
 * sil_arrays.c — Arrays, Tables, and Defined Data (v311.sil §14 4644–5267)
 *
 * Faithful C translation of Phil Budne's CSNOBOL4 v311.sil §14.
 * SORT/RSORT are stubbed (complex shell-sort + scratch ptr infra).
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M13
 */

#include <string.h>

#include "sil_types.h"
#include "sil_data.h"
#include "sil_arrays.h"
#include "sil_argval.h"
#include "sil_arena.h"
#include "sil_strings.h"
#include "sil_symtab.h"

/* External stubs — use signatures from headers where already declared */
extern SIL_result INVOKE_fn(void);
extern SIL_result INTVAL_fn(void);
/* VARVUP_fn, GENVUP_fn, FINDEX_fn declared in sil_argval.h / sil_arena.h / sil_symtab.h */
extern void       PSTACK_fn(DESCR_t *pos);
extern SIL_result MULT_fn(DESCR_t *out, DESCR_t a, DESCR_t b);
extern void       VPXPTR_fn2(void);

#define GETDC_B(dst, base_d, off_i) \
    memcpy(&(dst), (char*)A2P(D_A(base_d)) + (off_i), sizeof(DESCR_t))
#define PUTDC_B(base_d, off_i, src) \
    memcpy((char*)A2P(D_A(base_d)) + (off_i), &(src),  sizeof(DESCR_t))
#define GETD_B(dst, base_d, off_d) \
    memcpy(&(dst), (char*)A2P(D_A(base_d)) + D_A(off_d), sizeof(DESCR_t))
#define PUTD_B(base_d, off_d, src) \
    memcpy((char*)A2P(D_A(base_d)) + D_A(off_d), &(src), sizeof(DESCR_t))

static inline int deql(DESCR_t a, DESCR_t b)
{
    return D_A(a) == D_A(b) && D_V(a) == D_V(b);
}

/* Operand stack */
static DESCR_t ar_stk[32];
static int ar_top = 0;
static inline void    ar_push(DESCR_t d) { ar_stk[ar_top++] = d; }
static inline DESCR_t ar_pop(void)        { return ar_stk[--ar_top]; }

/* ── ARRAY(P,V) ──────────────────────────────────────────────────────── */
SIL_result ARRAY_fn(void)
{
    /* Get prototype string */
    if (VARVAL_fn() == FAIL) return FAIL;
    ar_push(XPTR);
    /* Get initial value (TPTR) */
    if (ARGVAL_fn() == FAIL) { ar_top--; return FAIL; }
    MOVD(TPTR, XPTR);
    XPTR = ar_pop();

    LOCSP_fn(&XSP, &XPTR);
    ar_push(XPTR);  /* save prototype descriptor for later */

    /* Parse dimension specs from prototype string */
    /* We use a simplified C loop rather than STREAM calls */
    /* Each dimension: [lo:]hi separated by commas */
    const char *p    = (const char *)A2P(XSP.a) + XSP.o;
    int32_t     plen = XSP.l;
    int32_t     ndim = 0;
    int32_t     total_elems = 1;

    /* Dimension info stack: pairs (lo, count) stored in ar_stk */
    int dim_base = ar_top;

    while (plen > 0) {
        /* Read first integer */
        int32_t lo = 1, hi = 0;
        int32_t v = 0; int neg = 0;
        if (*p == '-') { neg = 1; p++; plen--; }
        while (plen > 0 && *p >= '0' && *p <= '9') {
            v = v*10 + (*p - '0'); p++; plen--;
        }
        if (neg) v = -v;

        if (plen > 0 && *p == ':') {
            /* lo:hi */
            lo = v; p++; plen--;
            v = 0; neg = 0;
            if (plen > 0 && *p == '-') { neg = 1; p++; plen--; }
            while (plen > 0 && *p >= '0' && *p <= '9') {
                v = v*10 + (*p - '0'); p++; plen--;
            }
            if (neg) v = -v;
            hi = v;
        } else {
            /* single number: 1..v */
            if (v <= 0) { ar_top = dim_base - 1; return FAIL; }
            hi = v;
        }
        int32_t count = hi - lo + 1;
        if (count <= 0) { ar_top = dim_base - 1; return FAIL; }

        /* Push (lo, count) as two DESCRs */
        DESCR_t lo_d; SETAC(lo_d, lo); SETVC(lo_d, I);
        DESCR_t ct_d; SETAC(ct_d, count); SETVC(ct_d, I);
        ar_push(lo_d); ar_push(ct_d);
        ndim++;
        /* multiply into total */
        if ((int64_t)total_elems * count > 0x7FFFFFFF) {
            ar_top = dim_base - 1; return FAIL;
        }
        total_elems *= count;

        /* skip comma */
        if (plen > 0 && *p == ',') { p++; plen--; }
    }
    if (ndim == 0) { ar_top = dim_base - 1; return FAIL; }

    /* Allocate: 2 (heading) + ndim (index pairs) + total_elems (cells) */
    int32_t blk_elems = 2 + ndim + total_elems;
    int32_t blk_bytes = blk_elems * DESCR;
    SETAC(ZCL, blk_bytes); SETVC(ZCL, A);
    int32_t blk = BLOCK_fn(blk_bytes, A);
    if (!blk) { ar_top = dim_base - 1; return FAIL; }
    SETAC(ZPTR, blk); SETVC(ZPTR, A);

    /* Insert dimensionality at offset 2*DESCR */
    DESCR_t ndim_d; SETAC(ndim_d, ndim);
    PUTDC_B(ZPTR, 2*DESCR, ndim_d);

    /* Insert prototype descriptor at offset 1*DESCR */
    DESCR_t proto_d = ar_pop();   /* was saved before dim loop started */
    PUTDC_B(ZPTR, 1*DESCR, proto_d);

    /* Write dimension pairs: at offsets 3*DESCR .. (2+ndim)*DESCR */
    /* They are on ar_stk in reverse order (last dim first) */
    for (int i = ndim - 1; i >= 0; i--) {
        DESCR_t ct_d2 = ar_pop();
        DESCR_t lo_d2 = ar_pop();
        DESCR_t pair; SETAC(pair, D_A(lo_d2)); D_V(pair) = D_A(ct_d2);
        PUTDC_B(ZPTR, (3 + i) * DESCR, pair);
    }

    /* Fill element cells with initial value TPTR */
    for (int i = 0; i < total_elems; i++) {
        PUTDC_B(ZPTR, (2 + ndim + i) * DESCR, TPTR);
    }

    MOVD(XPTR, ZPTR); return OK;
}

/* ── ASSOC / ASSOCE — TABLE(N,M) ────────────────────────────────────── */
SIL_result ASSOCE_fn(DESCR_t size, DESCR_t ext)
{
    /* Allocate table extent block of size bytes, type T */
    int32_t blk = BLOCK_fn(D_A(size), T);
    if (!blk) return FAIL;
    SETAC(ZPTR, blk); SETVC(ZPTR, T);

    /* PUTD ZPTR,XPTR,ONECL — last slot = 1 (terminator) */
    DESCR_t sz2; MOVD(sz2, size); DECRA(sz2, DESCR);
    PUTD_B(ZPTR, sz2, ONECL);

    /* PUTD ZPTR,prev_slot,ext — second-to-last = extension size */
    DESCR_t sz3; MOVD(sz3, size); DECRA(sz3, 2*DESCR);
    PUTD_B(ZPTR, sz3, ext);

    /* Fill remaining slots with NULVCL */
    DESCR_t off; SETAC(off, DESCR);
    while (D_A(off) < D_A(sz3)) {
        PUTD_B(ZPTR, off, NULVCL);
        INCRA(off, 2*DESCR);
    }
    return OK;
}

SIL_result ASSOC_fn(void)
{
    /* Get N (table size) */
    if (INTVAL_fn() == FAIL) return FAIL;
    ar_push(XPTR);
    /* Get M (secondary allocation) */
    if (INTVAL_fn() == FAIL) { ar_top--; return FAIL; }
    MOVD(WPTR, XPTR);
    XPTR = ar_pop();

    /* Compute primary size: (N+1)*2*DESCR */
    int32_t n = D_A(XPTR) > 0 ? D_A(XPTR) : EXTSIZ;
    int32_t m = D_A(WPTR) > 0 ? D_A(WPTR) : EXTSIZ;
    DESCR_t xsz; SETAC(xsz, (n + 1) * 2 * DESCR); SETVC(xsz, T);
    DESCR_t wsz; SETAC(wsz, (m + 1) * 2 * DESCR);

    if (ASSOCE_fn(xsz, wsz) == FAIL) return FAIL;
    MOVD(XPTR, ZPTR); return OK;
}

/* ── ITEM — array/table reference ────────────────────────────────────── */
SIL_result ITEM_fn(void)
{
    SETAV(XCL, INCL);
    DECRA(XCL, 1);   /* skip referenced object */
    ar_push(XCL);

    if (ARGVAL_fn() == FAIL) { ar_top--; return FAIL; }
    MOVD(YCL, XPTR);
    XCL = ar_pop();

    if (VEQLC(YCL, A)) {
        /* Array reference */
        DESCR_t WCL_d; MOVD(WCL_d, XCL);
        /* Get dimension count */
        DESCR_t ndim_d; GETDC_B(ndim_d, YCL, 2*DESCR);
        int32_t ndim = D_A(ndim_d);

        /* Collect indices (pushing zeros for omitted args) */
        int32_t nargs = D_A(XCL);
        for (int i = 0; i < nargs; i++) {
            ar_push(XCL); ar_push(WCL_d); ar_push(YCL);
            if (INTVAL_fn() == FAIL) {
                ar_top -= 3 * (i + 1); return FAIL;
            }
            YCL = ar_pop(); WCL_d = ar_pop(); XCL = ar_pop();
            ar_push(XPTR);
            DECRA(XCL, 1);
        }
        /* Push zeros for omitted */
        while (D_A(XCL) < ndim) {
            ar_push(ZEROCL); INCRA(XCL, 1);
        }

        /* Compute linear offset */
        /* Dim pairs at offsets (2+i)*DESCR, pair: A=lo, V=count */
        int32_t offset = 0;
        for (int i = 0; i < ndim; i++) {
            DESCR_t pair; GETDC_B(pair, YCL, (3 + i) * DESCR);
            int32_t lo = D_A(pair), count = D_V(pair);
            DESCR_t idx = ar_pop();
            int32_t k = D_A(idx) - lo;
            if (k < 0 || k >= count) return FAIL;
            /* compute: offset = offset * count + k (but in reverse pop order) */
            /* Actually we popped in reverse; rebuild correctly */
            /* We'll multiply in a second pass: */
            ar_push(idx);  /* put back for now */
            (void)lo; (void)count; (void)k; (void)offset;
        }
        /* Simple 1D fast path; ND: compute properly */
        {
            int32_t linear = 0;
            for (int i = ndim - 1; i >= 0; i--) {
                DESCR_t pair; GETDC_B(pair, YCL, (3 + i) * DESCR);
                int32_t lo = D_A(pair), count = D_V(pair);
                DESCR_t idx = ar_pop();
                int32_t k = D_A(idx) - lo;
                if (k < 0 || k >= count) return FAIL;
                if (i == ndim - 1) {
                    linear = k;
                } else {
                    DESCR_t next_pair; GETDC_B(next_pair, YCL, (3 + i + 1) * DESCR);
                    linear = k * D_V(next_pair) + linear;
                }
            }
            /* Element at offset (2+ndim+linear)*DESCR */
            int32_t elem_off = (2 + ndim + linear) * DESCR;
            SETAC(XPTR, D_A(YCL) + elem_off);
            SETVC(XPTR, N);  /* NAME — interior pointer */
            return OK;
        }
    }

    if (VEQLC(YCL, T)) {
        /* Table reference */
        if (D_A(XCL) != 1) return FAIL;  /* ARGNER */
        ar_push(YCL);
        if (ARGVAL_fn() == FAIL) { ar_top--; return FAIL; }
        MOVD(YPTR, XPTR);
        MOVD(XPTR, ar_pop());  /* XPTR = table base */

        /* Walk extents looking for YPTR */
        while (1) {
            int32_t assoc = locapv_fn(D_A(XPTR), &YPTR);
            if (assoc) {
                /* Found: return interior NAME pointer */
                SETAC(XPTR, assoc); SETVC(XPTR, N);
                return OK;
            }
            /* Not found — check if more extents */
            int32_t sz = x_bksize(D_A(XPTR));
            DESCR_t last; GETDC_B(last, XPTR, sz - DESCR);
            if (AEQLC(last, 1)) {
                /* Last extent: check frozen */
                if (TESTF(YCL, FRZN)) { MOVD(XPTR, NULVCL); return OK; }
                /* Find empty slot and fill */
                int32_t slot = locapv_fn(D_A(XPTR), &ZEROCL);
                if (!slot) {
                    /* Expand: allocate new extent */
                    DESCR_t wsz; GETDC_B(wsz, XPTR, sz - 2*DESCR);
                    if (ASSOCE_fn(wsz, wsz) == FAIL) return FAIL;
                    /* Link old → new */
                    DESCR_t new_ext; MOVD(new_ext, ZPTR);
                    PUTDC_B(XPTR, sz - DESCR, new_ext);
                    slot = locapv_fn(D_A(ZPTR), &ZEROCL);
                    if (!slot) return FAIL;
                }
                /* Store subscript in value slot */
                PUTDC_B(XPTR, slot + DESCR, YPTR);
                SETAC(XPTR, D_A(XPTR) + slot); SETVC(XPTR, N);
                return OK;
            }
            /* Move to next extent */
            MOVD(XPTR, last);
        }
    }

    return FAIL;  /* NONARY */
}

/* ── PROTO(A) — PROTOTYPE ────────────────────────────────────────────── */
SIL_result PROTO_fn(void)
{
    if (ARGVAL_fn() == FAIL) return FAIL;
    if (!VEQLC(XPTR, A)) return FAIL;
    GETDC_B(ZPTR, XPTR, DESCR);
    MOVD(XPTR, ZPTR); return OK;
}

/* ── FREEZE(T) ───────────────────────────────────────────────────────── */
SIL_result FREEZE_fn(void)
{
    if (ARGVAL_fn() == FAIL) return FAIL;
    if (!VEQLC(XPTR, T)) return FAIL;
    D_F(XPTR) |= FRZN;
    /* Update in arena */
    { char *p = (char*)A2P(D_A(XPTR));
      *p = (*p) | (uint8_t)FRZN; }
    MOVD(XPTR, NULVCL); return OK;
}

/* ── THAW(T) ─────────────────────────────────────────────────────────── */
SIL_result THAW_fn(void)
{
    if (ARGVAL_fn() == FAIL) return FAIL;
    if (!VEQLC(XPTR, T)) return FAIL;
    { char *p = (char*)A2P(D_A(XPTR));
      *p = (*p) & ~(uint8_t)FRZN; }
    MOVD(XPTR, NULVCL); return OK;
}

/* ── DATDEF — DATA(P) ────────────────────────────────────────────────── */
/* Complex: requires STREAM/VARATB, AUGATL, FINDEX, PSTACK.
 * Stubbed until those infrastructure pieces are in place. */
SIL_result DATDEF_fn(void) { return FAIL; }

/* ── DEFDAT — create defined data object ─────────────────────────────── */
SIL_result DEFDAT_fn(void)
{
    SETAV(XCL, INCL);
    DESCR_t WCL_d; MOVD(WCL_d, XCL);
    DESCR_t YCL_d; MOVD(YCL_d, INCL);
    DESCR_t YPTR_d;
    PSTACK_fn(&YPTR_d);  /* post stack position */

    /* Read argument values from object code */
    while (D_A(XCL) > 0) {
        INCRA(OCICL, DESCR);
        GETD_B(XPTR, OCBSCL, OCICL);
        if (TESTF(XPTR, FNC)) {
            ar_push(XCL); ar_push(WCL_d); ar_push(YCL_d); ar_push(YPTR_d);
            if (INVOKE_fn() == FAIL) {
                ar_top -= 4; return FAIL;
            }
            YPTR_d = ar_pop(); YCL_d = ar_pop(); WCL_d = ar_pop(); XCL = ar_pop();
        } else {
            GETDC_B(XPTR, XPTR, DESCR);
        }
        ar_push(XPTR);
        DECRA(XCL, 1);
    }

    /* Get expected arg count from procedure descriptor */
    DESCR_t proc_d; GETDC_B(proc_d, YCL_d, 0);
    SETAV(XCL, proc_d);
    /* Pad with nulls if fewer args given */
    while (ACOMP(WCL_d, XCL) < 0) {
        ar_push(NULVCL); INCRA(WCL_d, 1);
    }

    /* Get definition block */
    DESCR_t def_d; GETDC_B(def_d, YCL_d, DESCR);
    DESCR_t size_d; MOVD(size_d, XCL);
    SETAC(size_d, D_A(XCL) * DESCR);
    MOVV(size_d, def_d);

    int32_t blk = BLOCK_fn(D_A(size_d), D_V(size_d));
    if (!blk) { ar_top -= D_A(XCL); return FAIL; }
    SETAC(ZPTR, blk);

    /* Copy stacked values into block */
    for (int32_t i = D_A(XCL) - 1; i >= 0; i--) {
        DESCR_t v = ar_pop();
        PUTDC_B(ZPTR, (int32_t)(i + 1) * DESCR, v);
    }

    MOVD(XPTR, ZPTR); return OK;
}

/* ── FIELD — field accessor ──────────────────────────────────────────── */
SIL_result FIELD_fn(void)
{
    ar_push(INCL);
    if (ARGVAL_fn() == FAIL) { ar_top--; return FAIL; }
    if (deql(XPTR, NULVCL)) { ar_top--; return FAIL; }
    MOVD(YCL, ar_pop());

    /* Handle integer index */
    if (VEQLC(XPTR, I)) {
        int32_t off = GNVARI_fn(D_A(XPTR));
        if (!off) return FAIL;
        SETAC(XPTR, off); SETVC(XPTR, S);
    } else if (VEQLC(XPTR, S)) {
        if (AEQLC(CASECL, 0)) VPXPTR_fn2();
    }

    /* Look up data type offset in definition block */
    MOVV(DT1CL, XPTR);
    DESCR_t def_d; GETDC_B(def_d, YCL, DESCR);
    int32_t assoc = locapt_fn(D_A(def_d), &DT1CL);
    if (!assoc) return FAIL;
    DESCR_t off_d; GETDC_B(off_d, DT1CL, 2*DESCR);
    SUM(XPTR, XPTR, off_d);
    SETVC(XPTR, N);
    return OK;
}

/* ── RSORT / SORT — stubs ────────────────────────────────────────────── */
/* Shell-sort requires A4PTR..A7PTR, LPTR, NANCHK, RCOMP, INTRL etc.
 * Stubbed until M19+ infrastructure is in place.                        */
SIL_result RSORT_fn(void) { return FAIL; }
SIL_result SORT_fn(void)  { return FAIL; }
