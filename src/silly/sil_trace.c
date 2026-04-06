/*
 * sil_trace.c — Tracing procedures (v311.sil §16 lines 5466–5827)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M16
 */

#include <string.h>

#include "sil_types.h"
#include "sil_data.h"
#include "sil_trace.h"
#include "sil_argval.h"
#include "sil_arena.h"
#include "sil_strings.h"
#include "sil_symtab.h"
#include "sil_asgn.h"   /* IND_fn */

extern Sil_result INVOKE_fn(void);
extern Sil_result KEYT_fn(void);       /* KEYWRD internal lookup    */
extern Sil_result ARGINT_fn(DESCR_t fn, DESCR_t n); /* ARG internal */
extern Sil_result DTREP_fn3(DESCR_t *out, DESCR_t obj);
extern void       XCALL_MSTIME(DESCR_t *out);
extern void       XCALL_SBREAL(DESCR_t *out, DESCR_t a, DESCR_t b);
extern void       STPRNT_fn(int32_t key, DESCR_t blk, SPEC_t *sp);
extern void       XCALL_OUTPUT(int32_t unit, const char *msg);
extern void       SHORTN_fn(SPEC_t *sp, int32_t n);

#define GETDC_B(dst, base_d, off_i) \
    memcpy(&(dst), (char*)A2P(D_A(base_d)) + (off_i), sizeof(DESCR_t))
#define PUTDC_B(base_d, off_i, src) \
    memcpy((char*)A2P(D_A(base_d)) + (off_i), &(src),  sizeof(DESCR_t))
#define ATTRIB  (2 * DESCR)

static inline int deql(DESCR_t a, DESCR_t b)
{
    return D_A(a) == D_A(b) && D_V(a) == D_V(b);
}

static inline void SETLC_sp(SPEC_t *sp, int32_t n) { sp->l = n; }
static inline void SETSP_fn(SPEC_t *dst, SPEC_t *src) { *dst = *src; }

static DESCR_t tr_stk[32];
static int tr_top = 0;
static inline void    tr_push(DESCR_t d) { tr_stk[tr_top++] = d; }
static inline DESCR_t tr_pop(void)        { return tr_stk[--tr_top]; }

/* Forward declarations */
static Sil_result fentr_common(void);
static Sil_result valtr_common(void);
static Sil_result valtr4(void);

/* ── TRACEP — shared subentry for TRACE/STOPTR ───────────────────────── */
static Sil_result tracep(DESCR_t xptr, DESCR_t yptr, DESCR_t wptr, DESCR_t zptr)
{
    /* GETDC TPTR,YPTR,DESCR — get default function */
    GETDC_B(TPTR, yptr, DESCR);

    /* Use supplied trace function if non-null */
    if (!deql(zptr, NULVCL)) {
        int32_t off = FINDEX_fn(&zptr);
        if (off) SETAC(TPTR, off);
    }

    /* Allocate 5*DESCR code block, copy TRCBLK skeleton */
    int32_t blk = BLOCK_fn(5*DESCR, C);
    if (!blk) return FAIL;
    SETAC(XCL, blk);
    memcpy(A2P(blk), TRCBLK, 6 * sizeof(DESCR_t));

    /* SETVC TPTR,2 — 2 arguments */
    SETVC(TPTR, 2);
    PUTDC_B(XCL, 1*DESCR, TPTR);    /* function descriptor */
    PUTDC_B(XCL, 3*DESCR, xptr);    /* name to trace       */
    PUTDC_B(XCL, 5*DESCR, wptr);    /* tag                 */

    /* GETDC TPTR,YPTR,0 — attribute list for this trace type */
    GETDC_B(TPTR, yptr, 0);
    if (!AEQLC(TPTR, 0)) {
        int32_t found = locapt_fn(D_A(TPTR), &xptr);
        if (found) {
            SETAC(TPTR, found);
            PUTDC_B(TPTR, 2*DESCR, XCL);
            MOVD(XPTR, NULVCL); return OK;
        }
        /* AUGATL */
        AUGATL_fn(D_A(TPTR), xptr, XCL);
        MOVD(XPTR, NULVCL); return OK;
    }
    /* TRAC4: allocate new pair list */
    int32_t pb = BLOCK_fn(2*DESCR, B);
    if (!pb) return FAIL;
    SETAC(TPTR, pb);
    PUTDC_B(TPTR, DESCR, xptr);
    PUTDC_B(TPTR, 2*DESCR, XCL);
    PUTDC_B(yptr, 0, TPTR);
    MOVD(XPTR, NULVCL); return OK;
}

/* ── TRACE(V,R,T,F) ──────────────────────────────────────────────────── */
Sil_result TRACE_fn(void)
{
    if (IND_fn() == FAIL) return FAIL;
    tr_push(XPTR);
    if (VARVUP_fn() == FAIL) { tr_top--; return FAIL; }
    tr_push(XPTR);
    if (ARGVAL_fn() == FAIL) { tr_top -= 2; return FAIL; }
    tr_push(XPTR);
    if (VARVUP_fn() == FAIL) { tr_top -= 3; return FAIL; }
    MOVD(ZPTR, XPTR);
    MOVD(WPTR, tr_pop()); MOVD(YPTR, tr_pop()); XPTR = tr_pop();

    /* Default type → VALUE */
    if (deql(YPTR, NULVCL)) MOVD(YPTR, VALTRS);

    /* Look up trace type in TRATL */
    int32_t assoc = locapv_fn(D_A(TRATL), &YPTR);
    if (assoc) {
        SETAC(YPTR, assoc);
        GETDC_B(YPTR, YPTR, DESCR);
        return tracep(XPTR, YPTR, WPTR, ZPTR);
    }
    /* FUNCTION type */
    if (deql(YPTR, FUNTCL)) {
        MOVD(YPTR, TFNCLP);
        if (tracep(XPTR, YPTR, WPTR, ZPTR) == FAIL) return FAIL;
        MOVD(YPTR, TFNRLP);
        return tracep(XPTR, YPTR, WPTR, ZPTR);
    }
    return FAIL;  /* unknown type */
}

/* ── STOPTR(V,R) ─────────────────────────────────────────────────────── */
Sil_result STOPTR_fn(void)
{
    if (IND_fn() == FAIL) return FAIL;
    tr_push(XPTR);
    if (VARVUP_fn() == FAIL) { tr_top--; return FAIL; }
    MOVD(YPTR, XPTR); XPTR = tr_pop();

    if (deql(YPTR, NULVCL)) MOVD(YPTR, VALTRS);

    int32_t assoc = locapv_fn(D_A(TRATL), &YPTR);
    DESCR_t yptr2;
    if (assoc) {
        SETAC(yptr2, assoc);
        GETDC_B(yptr2, yptr2, DESCR);
    } else if (deql(YPTR, FUNTCL)) {
        /* FUNCTION: stop both CALL and RETURN */
        for (int i = 0; i < 2; i++) {
            DESCR_t lp = (i == 0) ? TFNCLP : TFNRLP;
            GETDC_B(yptr2, lp, 0);
            int32_t found = locapt_fn(D_A(yptr2), &XPTR);
            if (found) {
                SETAC(TPTR, found);
                PUTDC_B(TPTR, DESCR, ZEROCL);
                PUTDC_B(TPTR, 2*DESCR, ZEROCL);
            }
        }
        MOVD(XPTR, NULVCL); return OK;
    } else {
        return FAIL;
    }
    GETDC_B(yptr2, yptr2, 0);
    int32_t found = locapt_fn(D_A(yptr2), &XPTR);
    if (found) {
        SETAC(TPTR, found);
        PUTDC_B(TPTR, DESCR, ZEROCL);
        PUTDC_B(TPTR, 2*DESCR, ZEROCL);
    }
    MOVD(XPTR, NULVCL); return OK;
}

/* ── Shared trace message builder ────────────────────────────────────── */
/* Appends "FILE:LINE Trace at STNO: " prefix to PROTSP */
static void trace_prefix(void)
{
    SETLC_sp(&PROTSP, 0);
    LOCSP_fn(&XSP, &FILENM);
    APDSP_fn(&PROTSP, &XSP);
    APDSP_fn(&PROTSP, &COLSP);
    INTSPC_fn(&XSP, &LNNOCL);
    APDSP_fn(&PROTSP, &XSP);
    APDSP_fn(&PROTSP, &TRSTSP);
    INTSPC_fn(&XSP, &STNOCL);
    APDSP_fn(&PROTSP, &XSP);
    APDSP_fn(&PROTSP, &COLSP);
    APDSP_fn(&PROTSP, &SPCSP);
}

/* Append elapsed time suffix and print */
static void trace_print(SPEC_t *buf)
{
    XCALL_MSTIME(&YPTR);
    XCALL_SBREAL(&YPTR, YPTR, ETMCL);
    SPEC_t tsp; REALST_fn(&tsp, &YPTR);
    APDSP_fn(buf, &ETIMSP);
    APDSP_fn(buf, &tsp);
    STPRNT_fn(D_A(IOKEY), OUTBLK, buf);
}

/* ── FENTR — function call trace ─────────────────────────────────────── */
Sil_result FENTR_fn(void)
{
    if (VARVAL_fn() == FAIL) return FAIL;
    MOVD(WPTR, XPTR);
    return fentr_common();
}

Sil_result FENTR2_fn(DESCR_t name)
{
    MOVD(WPTR, name);
    return fentr_common();
}

static Sil_result fentr_common(void)
{
    trace_prefix();
    LOCSP_fn(&XSP, &WPTR);
    D_A(TCL) = XSP.l;
    if (D_A(TCL) > BUFLEN) { XCALL_OUTPUT(D_A(OUTPUT), "***PRINT REQUEST TOO LONG***\n"); return OK; }
    APDSP_fn(&PROTSP, &XSP);
    APDSP_fn(&PROTSP, &LPRNSP);

    SETAC(WCL, 0);
    while (1) {
        INCRA(WCL, 1);
        /* Get argument WCL of WPTR */
        Sil_result rc = ARGINT_fn(WPTR, WCL);
        if (rc == FAIL) break;
        GETDC_B(ZPTR, XPTR, DESCR);
        SPEC_t vsp;
        switch (D_V(ZPTR)) {
        case S: LOCSP_fn(&vsp, &ZPTR); break;
        case I: INTSPC_fn(&vsp, &ZPTR); break;
        default:
            if (DTREP_fn3(&XPTR, ZPTR) == FAIL) break;
            memcpy(&vsp, A2P(D_A(XPTR)), sizeof(SPEC_t)); break;
        }
        D_A(SCL) = vsp.l;
        SUM(TCL, TCL, SCL);
        if (D_A(TCL) > BUFLEN) { XCALL_OUTPUT(D_A(OUTPUT), "***PRINT REQUEST TOO LONG***\n"); return OK; }
        APDSP_fn(&PROTSP, &vsp);
        APDSP_fn(&PROTSP, &CMASP);
    }
    if (D_A(WCL) > 1) SHORTN_fn(&PROTSP, 1);  /* remove last comma */
    APDSP_fn(&PROTSP, &RPRNSP);
    trace_print(&PROTSP);
    return OK;
}

/* ── KEYTR — keyword trace ───────────────────────────────────────────── */
Sil_result KEYTR_fn(void)
{
    SETAC(FNVLCL, 1);
    if (VARVAL_fn() == FAIL) return FAIL;
    MOVD(WPTR, XPTR);
    LOCSP_fn(&XSP, &WPTR);
    if (KEYT_fn() == FAIL) return FAIL;
    MOVD(YCL, XPTR);

    trace_prefix();
    APDSP_fn(&PROTSP, &AMPSP);
    APDSP_fn(&PROTSP, &XSP);
    APDSP_fn(&PROTSP, &BLSP);
    INTSPC_fn(&YSP, &YCL);
    APDSP_fn(&PROTSP, &EQLSP);
    APDSP_fn(&PROTSP, &YSP);
    trace_print(&PROTSP);
    return OK;
}

/* ── LABTR — label trace ─────────────────────────────────────────────── */
Sil_result LABTR_fn(void)
{
    SETAC(FNVLCL, 0);
    if (VARVAL_fn() == FAIL) return FAIL;
    MOVD(YPTR, XPTR);
    LOCSP_fn(&YSP, &YPTR);
    SETSP_fn(&XSP, &XFERSP);
    trace_prefix();
    APDSP_fn(&PROTSP, &XSP);
    APDSP_fn(&PROTSP, &YSP);
    trace_print(&PROTSP);
    return OK;
}

/* ── TRPHND — trace handler ──────────────────────────────────────────── */
Sil_result TRPHND_fn(DESCR_t atptr)
{
    MOVD(ATPTR, atptr);
    DECRA(TRAPCL, 1);

    /* Save system state */
    DESCR_t sv_filenm = FILENM, sv_lnnocl = LNNOCL, sv_lstncl = LSTNCL;
    DESCR_t sv_stnocl = STNOCL, sv_frtncl = FRTNCL;
    DESCR_t sv_ocbscl = OCBSCL, sv_ocicl  = OCICL;
    DESCR_t sv_trapcl = TRAPCL, sv_tracl  = TRACL;
    DESCR_t sv_lsflnm = LSFLNM, sv_lslncl = LSLNCL;

    /* Set up new code base from trace block */
    GETDC_B(OCBSCL, ATPTR, 2*DESCR);
    SETAC(OCICL, DESCR);
    memcpy(&XPTR, (char*)A2P(D_A(OCBSCL)) + D_A(OCICL), sizeof(DESCR_t));
    SETAC(TRAPCL, 0);
    SETAC(TRACL,  0);

    INVOKE_fn();  /* execute trace function (ignore return value) */

    /* Restore */
    MOVD(LSLNCL, sv_lslncl); MOVD(LSFLNM, sv_lsflnm);
    MOVD(TRACL,  sv_tracl);  MOVD(TRAPCL, sv_trapcl);
    MOVD(OCICL,  sv_ocicl);  MOVD(OCBSCL, sv_ocbscl);
    MOVD(FRTNCL, sv_frtncl); MOVD(STNOCL, sv_stnocl);
    MOVD(LSTNCL, sv_lstncl); MOVD(LNNOCL, sv_lnnocl);
    MOVD(FILENM, sv_filenm);

    return OK;
}

/* ── VALTR — value trace ─────────────────────────────────────────────── */
Sil_result VALTR_fn(void)
{
    SETAC(FNVLCL, 1);
    return valtr_common();
}

Sil_result FNEXTR_fn(void)
{
    SETAC(FNVLCL, 0);
    return valtr_common();
}

Sil_result FNEXT2_fn(DESCR_t name)
{
    SETAC(FNVLCL, 0);
    MOVD(XPTR, name);
    return valtr4();
}

static Sil_result valtr_common(void)
{
    if (IND_fn() == FAIL) return FAIL;
    tr_push(XPTR);
    if (VARVAL_fn() == FAIL) { tr_top--; return FAIL; }
    MOVD(ZPTR, XPTR); XPTR = tr_pop();
    return valtr4();
}

static Sil_result valtr4(void)
{
    SETLC_sp(&TRACSP, 0);
    LOCSP_fn(&XSP, &FILENM);
    APDSP_fn(&TRACSP, &XSP);
    APDSP_fn(&TRACSP, &COLSP);
    INTSPC_fn(&XSP, &LNNOCL);
    APDSP_fn(&TRACSP, &XSP);
    APDSP_fn(&TRACSP, &TRSTSP);
    INTSPC_fn(&XSP, &STNOCL);
    APDSP_fn(&TRACSP, &XSP);
    APDSP_fn(&TRACSP, &COLSP);
    APDSP_fn(&TRACSP, &SPCSP);

    if (!AEQLC(FNVLCL, 0)) {
        /* value trace: append variable name */
        if (VEQLC(XPTR, S)) {
            LOCSP_fn(&XSP, &ZPTR);
        } else {
            LOCSP_fn(&XSP, &XPTR);
        }
        D_A(TCL) = XSP.l;
        if (D_A(TCL) > BUFLEN) goto vxovr;
        APDSP_fn(&TRACSP, &XSP);
        APDSP_fn(&TRACSP, &BLEQSP);
        GETDC_B(YPTR, XPTR, DESCR);
    } else {
        /* return trace: append level + return type + function name */
        APDSP_fn(&TRACSP, &TRLVSP);
        MOVD(XCL, LVLCL); DECRA(XCL, 1);
        INTSPC_fn(&XSP, &XCL);
        APDSP_fn(&TRACSP, &XSP);
        APDSP_fn(&TRACSP, &BLSP);
        LOCSP_fn(&XSP, &RETPCL);
        APDSP_fn(&TRACSP, &XSP);
        APDSP_fn(&TRACSP, &OFSP);
        if (deql(RETPCL, FRETCL)) {
            LOCSP_fn(&XSP, &ZPTR);
        } else {
            LOCSP_fn(&XSP, &XPTR);
        }
        D_A(TCL) = XSP.l;
        if (D_A(TCL) > BUFLEN) goto vxovr;
        APDSP_fn(&TRACSP, &XSP);
        trace_print(&TRACSP);
        return OK;
    }

    /* Append value */
    switch (D_V(YPTR)) {
    case S: {
        LOCSP_fn(&XSP, &YPTR);
        D_A(SCL) = XSP.l;
        SUM(TCL, TCL, SCL);
        if (D_A(TCL) > BUFLEN) goto vxovr;
        APDSP_fn(&TRACSP, &QTSP);
        APDSP_fn(&TRACSP, &XSP);
        APDSP_fn(&TRACSP, &QTSP);
        break;
    }
    case I:
        INTSPC_fn(&XSP, &YPTR);
        APDSP_fn(&TRACSP, &XSP);
        break;
    default:
        if (DTREP_fn3(&XPTR, YPTR) == FAIL) break;
        memcpy(&XSP, A2P(D_A(XPTR)), sizeof(SPEC_t));
        APDSP_fn(&TRACSP, &XSP);
        break;
    }
    trace_print(&TRACSP);
    return OK;

vxovr:
    XCALL_OUTPUT(D_A(OUTPUT), "***PRINT REQUEST TOO LONG***\n");
    return OK;
}

/* ── SETEXIT(LBL) ────────────────────────────────────────────────────── */
Sil_result SETEXIT_fn(void)
{
    if (VARVUP_fn() == FAIL) return FAIL;
    if (!AEQLC(XPTR, 0)) {
        GETDC_B(YPTR, XPTR, ATTRIB);
        if (AEQLC(YPTR, 0)) return FAIL;
    }
    MOVD(YPTR, XITPTR);
    MOVD(XITPTR, XPTR);
    MOVD(XPTR, YPTR);
    return OK;
}

/* ── XITHND — SETEXIT handler ────────────────────────────────────────── */
Sil_result XITHND_fn(void)
{
    if (AEQLC(XITPTR, 0)) return FAIL;
    MOVD(XFILEN, FILENM);
    MOVD(XLNNOC, LNNOCL);
    MOVD(XSTNOC, STNOCL);
    MOVD(XLSFLN, LSFLNM);
    MOVD(XLSLNC, LSLNCL);
    MOVD(XERRTY, ERRTYP);
    MOVD(XOCBSC, OCBSCL);
    MOVD(XFRTNC, FRTNCL);
    MOVD(XOCICL, OCICL);

    GETDC_B(OCBSCL, XITPTR, ATTRIB);
    if (AEQLC(OCBSCL, 0)) return FAIL;
    SETAC(FRTNCL, 0);
    SETAC(XITPTR, 0);
    MOVD(LSTNCL, STNOCL);
    MOVA(LSLNCL, LNNOCL);
    MOVA(LSFLNM, FILENM);
    MOVD(XPTR, NULVCL);
    return OK;
}

/* end of sil_trace.c */
