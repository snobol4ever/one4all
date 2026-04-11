/*
 * extern.c — External functions (v311.sil §13 lines 4471–4643)
 *
 * Faithful C translation of Phil Budne's CSNOBOL4 v311.sil §13.
 *
 * LOAD/UNLOAD require STREAM/VARATB (prototype parsing) — stubbed.
 * LNKFNC: argument coercion + LINK call (platform stub).
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M17
 */

#include <string.h>

#include "types.h"
#include "data.h"
#include "extern.h"
#include "argval.h"
#include "symtab.h"
#include "arena.h"
#include "strings.h"
#include "symtab.h"

/* Platform stubs */
extern RESULT_t XCALL_LOAD(DESCR_t *entry_out, SPEC_t *fn_sp, SPEC_t *lib_sp);
extern void       XCALL_UNLOAD(SPEC_t *fn_sp);
extern RESULT_t XCALL_LINK(DESCR_t *result, DESCR_t *arg_base,
                              int32_t nargs, DESCR_t entry);
extern RESULT_t XCALL_RELSTRING(DESCR_t str);

#define GETDC_B(dst, base_d, off_i) \
    memcpy(&(dst), (char*)A2P(D_A(base_d)) + (off_i), sizeof(DESCR_t))
#define PUTDC_B(base_d, off_i, src) \
    memcpy((char*)A2P(D_A(base_d)) + (off_i), &(src),  sizeof(DESCR_t))
#define GETD_B(dst, base_d, off_d) \
    memcpy(&(dst), (char*)A2P(D_A(base_d)) + D_A(off_d), sizeof(DESCR_t))

static inline int deql(DESCR_t a, DESCR_t b)
{
    return D_A(a) == D_A(b) && D_V(a) == D_V(b);
}

/*====================================================================================================================*/
static DESCR_t ext_stk[32];
static int ext_top = 0;
static inline void    ext_push(DESCR_t d) { ext_stk[ext_top++] = d; }
static inline DESCR_t ext_pop(void)        { return ext_stk[--ext_top]; }

/* ── LOAD(P,L) ───────────────────────────────────────────────────────── */
/* v311.sil §13 lines 4475–4548  ·  snobol4.c LOAD()/LOAD2() lines 5666–5792 */
extern RESULT_t STREAM_fn(SPEC_t *res, SPEC_t *src, DESCR_t *tbl, int *stype_out);
extern DESCR_t  VARATB;
extern DESCR_t  PROTER;   /* prototype error label — treated as FAIL here */

static int spec_eq_rparen(SPEC_t *sp)
{
    /* LEXCMP TSP,RPRNSP — single char ')' comparison */
    extern SPEC_t RPRNSP;
    if (sp->l != RPRNSP.l) return 0;
    const char *a = (const char *)A2P(sp->a)  + sp->o;
    const char *b = (const char *)A2P(RPRNSP.a) + RPRNSP.o;
    return memcmp(a, b, (size_t)sp->l) == 0;
}

RESULT_t LOAD_fn(void)
{
    /* RCALL XPTR,VARVAL,,FAIL — get prototype string */
    if (VARVAL_fn() == FAIL) return FAIL;
    ext_push(XPTR);                             /* PUSH XPTR */
    /* RCALL WPTR,VARVAL,,FAIL — get library name */
    if (VARVAL_fn() == FAIL) { ext_pop(); return FAIL; }
    XPTR = ext_pop();                           /* POP XPTR */

    /* LOAD2: */
    SPEC_t VSP, XSP, YSP, ZSP, TSP;
    LOCSP_fn(&VSP, &WPTR);                      /* LOCSP VSP,WPTR */
    LOCSP_fn(&XSP, &XPTR);                      /* LOCSP XSP,XPTR */

    int stype = 0;
    /* STREAM YSP,XSP,VARATB,PROTER,PROTER — get function name */
    if (STREAM_fn(&YSP, &XSP, &VARATB, &stype) != OK) return FAIL;
    if (stype == 0 /* ST_ERROR */ || stype == 2 /* ST_EOS */) return FAIL;

    /* AEQLC STYPE,LPTYP,PROTER — verify left parenthesis */
    if (stype != LPTYP) return FAIL;

    /* RCALL XPTR,GENVUP,YSPPTR — generate var for function name */
    { SPEC_t tmp = YSP;
      int32_t r = GENVUP_fn(&tmp); if (!r) return FAIL;
      XPTR.a.i = r; XPTR.f = 0; XPTR.v = S; }
    /* RCALL ZCL,FINDEX,XPTR — find function slot */
    DESCR_t ZCL; ZCL.a.i = FINDEX_fn(&XPTR); ZCL.f = 0; ZCL.v = 0;
    DESCR_t YCL = ZEROCL;                       /* MOVD YCL,ZEROCL — arg count = 0 */

    /* LOAD4 loop: parse argument types */
L_LOAD4:
    XSP.l--; XSP.o++;                           /* FSHRTN XSP,1 */
    stype = 0;
    if (STREAM_fn(&ZSP, &XSP, &VARATB, &stype) != OK) goto L_LOAD1;
    if (stype == 2 /* ST_EOS */) return FAIL;   /* PROTER */

    /* SELBRA STYPE,(PROTER,,LOAD6) — stype==1→PROTER, stype==3→LOAD6, else fall */
    if (stype == 1) return FAIL;
    if (stype == 3) goto L_LOAD6;

    /* Mid-arg type: GENVUP + LOCAPV */
    { SPEC_t tmp = ZSP;
      int32_t r = GENVUP_fn(&tmp); if (!r) return FAIL;
      XPTR.a.i = r; XPTR.f = 0; XPTR.v = S; }
    { int32_t pair = locapv_fn(D_A(DTATL), &XPTR);
      if (!pair) goto L_LOAD9;
      memcpy(&XPTR, A2P(pair + DESCR), sizeof(DESCR_t)); }
    ext_push(XPTR);                             /* PUSH XPTR */
L_LOAD10:
    YCL.a.i++;                                  /* INCRA YCL,1 */
    goto L_LOAD4;

L_LOAD6:
    YCL.a.i++;                                  /* INCRA YCL,1 */
    { SPEC_t tmp = ZSP;
      int32_t r = GENVAR_fn(&tmp); if (!r) return FAIL;
      XPTR.a.i = r; XPTR.f = 0; XPTR.v = S; }
    { int32_t pair = locapv_fn(D_A(DTATL), &XPTR);
      if (!pair) goto L_LOAD11;
      memcpy(&XPTR, A2P(pair + DESCR), sizeof(DESCR_t)); }
    ext_push(XPTR);                             /* PUSH XPTR */

L_LOAD13:
    XSP.l--; XSP.o++;                           /* FSHRTN XSP,1 — delete ')' */
    { SPEC_t tmp = XSP;
      int32_t r = GENVAR_fn(&tmp); if (!r) return FAIL;
      XPTR.a.i = r; XPTR.f = 0; XPTR.v = S; }
    { int32_t pair = locapv_fn(D_A(DTATL), &XPTR);
      if (!pair) goto L_LOAD7;
      memcpy(&XPTR, A2P(pair + DESCR), sizeof(DESCR_t)); }
    ext_push(XPTR);                             /* PUSH XPTR */

L_LOAD8:
    /* SETVA LODCL,YCL — store arg count in LODCL.v */
    LODCL.v = YCL.a.i;
    YCL.a.i++;                                  /* INCRA YCL,1 */
    DESCR_t XCL;
    XCL.a.i = YCL.a.i * DESCR;                 /* MULTC XCL,YCL,DESCR */
    XCL.f = 0; XCL.v = 0;
    XCL.a.i += DESCR;                           /* INCRA XCL,DESCR */
    XCL.v = B;                                  /* SETVC XCL,B */
    /* RCALL ZPTR,BLOCK,XCL — allocate definition block */
    { int32_t ba = BLOCK_fn(XCL.a.i, B);
      if (!ba) return FAIL;
      ZPTR.a.i = ba; ZPTR.f = 0; ZPTR.v = B; }
    /* SUM XPTR,ZPTR,XCL — pointer to end of block */
    XPTR.a.i = ZPTR.a.i + XCL.a.i; XPTR.f = 0; XPTR.v = 0;

L_LOAD12:
    XPTR.a.i -= DESCR;                          /* DECRA XPTR,DESCR */
    { DESCR_t yptr = ext_pop();                 /* POP YPTR */
      memcpy(A2P(XPTR.a.i + DESCR), &yptr, sizeof(DESCR_t)); } /* PUTDC XPTR,DESCR,YPTR */
    YCL.a.i--;                                  /* DECRA YCL,1 */
    if (YCL.a.i > 0) goto L_LOAD12;            /* ACOMPC YCL,0,LOAD12 */

    /* LOAD YPTR,YSP,VSP,FAIL — call platform to load the symbol */
    if (XCALL_LOAD(&YPTR, &YSP, &VSP) != OK) return FAIL;
    memcpy(A2P(XPTR.a.i),        &YPTR, sizeof(DESCR_t)); /* PUTDC XPTR,0,YPTR */
    memcpy(A2P(ZCL.a.i),         &LODCL, sizeof(DESCR_t)); /* PUTDC ZCL,0,LODCL */
    memcpy(A2P(ZCL.a.i + DESCR), &ZPTR,  sizeof(DESCR_t)); /* PUTDC ZCL,DESCR,ZPTR */
    MOVD(XPTR, NULVCL); return OK;              /* BRANCH RETNUL */

L_LOAD7:
    ext_push(ZEROCL); goto L_LOAD8;            /* unspecified return type */
L_LOAD9:
    ext_push(ZEROCL); goto L_LOAD10;           /* unspecified arg type (mid) */
L_LOAD11:
    ext_push(ZEROCL); goto L_LOAD13;           /* unspecified arg type (last) */

L_LOAD1:
    /* ST_ERROR from STREAM — check if we hit ')' (single-arg no-type case) */
    ext_push(ZEROCL);
    TSP = XSP; TSP.l = 1;                      /* SETSP TSP,XSP / SETLC TSP,1 */
    YCL.a.i++;
    if (spec_eq_rparen(&TSP)) goto L_LOAD13;
    goto L_LOAD4;
}

/* ── UNLOAD(F) ───────────────────────────────────────────────────────── */
RESULT_t UNLOAD_fn(void)
{
    if (VARVUP_fn() == FAIL) return FAIL;
    int32_t zcl_off = FINDEX_fn(&XPTR); /* FINDEX — get function descriptor */
    if (!zcl_off) return FAIL;
    SETAC(ZCL, zcl_off);
    PUTDC_B(ZCL, 0, UNDFCL); /* Reset to UNDFCL */
    LOCSP_fn(&XSP, &XPTR); /* Platform unload */
    XCALL_UNLOAD(&XSP);
    MOVD(XPTR, NULVCL); return OK;
}

/*====================================================================================================================*/
/* ── LNKFNC — invoke external function ──────────────────────────────── */
/*
 * Coerces each argument to the declared type, then calls XCALL_LINK.
 * Type coercions:
 *   STRING→INTEGER (LNKVI), INTEGER→STRING (LNKIV)
 *   REAL→INTEGER  (LNKRI), INTEGER→REAL   (LNKIR)
 *   STRING→REAL   (LNKVR), REAL→STRING    (LNKRV)
 */
RESULT_t LNKFNC_fn(void)
{
    SETAV(XCL, INCL); /* actual arg count       */
    MOVD(YCL, INCL); /* save function desc     */
    DESCR_t WCL_d; GETDC_B(WCL_d, YCL, 0); SETAV(WCL_d, WCL_d); /* formal count */
    DESCR_t WPTR_d; MOVD(WPTR_d, ZEROCL); /* passed-arg counter */
    DESCR_t ZCL_d; GETDC_B(ZCL_d, YCL, DESCR); /* definition block */
    DESCR_t YPTR_d; /* stack position — we use ext_top as proxy */
    int stack_base = ext_top;
    DESCR_t TCL_d; SETAC(TCL_d, 2*DESCR); /* offset into def block */
    int32_t nactual = D_A(XCL);
    for (int32_t i = 0; i < nactual; i++) { /* Evaluate and coerce each actual argument */
        ext_push(XCL); ext_push(ZCL_d); ext_push(TCL_d);
        ext_push(YPTR_d); ext_push(WCL_d); ext_push(YCL); ext_push(WPTR_d);
        if (ARGVAL_fn() == FAIL) { ext_top = stack_base; return FAIL; }
        WPTR_d = ext_pop(); YCL = ext_pop(); WCL_d = ext_pop();
        YPTR_d = ext_pop(); TCL_d = ext_pop(); ZCL_d = ext_pop(); XCL = ext_pop();
        DECRA(WCL_d, 1);
        if (D_A(WCL_d) >= 0) { /* Coerce if within formal range and type specified */
            DESCR_t ZPTR_d; GETD_B(ZPTR_d, ZCL_d, TCL_d);
            if (!AEQLC(ZPTR_d, 0) && !VEQLC(ZPTR_d, D_V(XPTR))) {
                SETAV(DTCL, XPTR); MOVV(DTCL, ZPTR_d);
                if (deql(DTCL, VIDTP)) { /* STRING→INTEGER */
                    LOCSP_fn(&XSP, &XPTR);
                    SPCINT_fn(&XPTR, &XSP);
                }
                else if (deql(DTCL, IVDTP)) { /* INTEGER→STRING */
                    int32_t off = GNVARI_fn(D_A(XPTR));
                    if (off) { SETAC(XPTR, off); SETVC(XPTR, S); }
                }
                else if (deql(DTCL, RIDTP)) { /* REAL→INTEGER */
                    D_A(XPTR) = (int32_t)D_R(XPTR); SETVC(XPTR, I); /* RLINT stub — truncate real to int */
                }
                else if (deql(DTCL, IRDTP)) { /* INTEGER→REAL */
                    D_R(XPTR) = (float)D_A(XPTR); SETVC(XPTR, R);
                }
                else if (deql(DTCL, RVDTP)) { /* REAL→STRING */
                    SPEC_t rsp; REALST_fn(&rsp, &XPTR);
                    int32_t off = GENVAR_fn(&rsp);
                    if (off) { SETAC(XPTR, off); SETVC(XPTR, S); }
                }
                else if (deql(DTCL, VRDTP)) { /* STRING→REAL */
                    LOCSP_fn(&XSP, &XPTR);
                    if (SPCINT_fn(&XPTR, &XSP) == FAIL) {
                        if (SPREAL_fn(&XPTR, &XSP) == FAIL) return FAIL;
                    }
                }
            }
        }
        ext_push(XPTR);
        INCRA(TCL_d, DESCR);
        INCRA(WPTR_d, 1);
    }
    while (D_A(WCL_d) > 0) { /* Pad with nulls for omitted formals */
        ext_push(NULVCL);
        INCRA(WPTR_d, 1);
        DECRA(WCL_d, 1);
    }
    int32_t sz = x_bksize(D_A(ZCL_d)); /* Get definition block end: entry point and target type */
    DESCR_t xptr2; GETDC_B(xptr2, ZCL_d, sz - DESCR); /* target type   */
    DESCR_t zcl2; GETDC_B(zcl2, ZCL_d, DESCR); /* entry address — oracle: D(ZCL)=D(D_A(ZCL)+DESCR) → slot 1 */
    int32_t nargs = ext_top - stack_base; /* Pointer to argument list on our ext_stk */
    DESCR_t *arg_base = &ext_stk[stack_base];
    if (XCALL_LINK(&ZPTR, arg_base, nargs, zcl2) == FAIL) { /* LINK: call external function */
        ext_top = stack_base; return FAIL;
    }
    ext_top = stack_base;
    if (D_V(ZPTR) == M) { /* Handle return value */
        LOCSP_fn(&ZSP, &ZPTR); /* malloc'd linked string [PLB130] */
        int32_t off = GENVAR_fn(&ZSP);
        DESCR_t old_zptr = ZPTR;
        if (off) { SETAC(ZPTR, off); SETVC(ZPTR, S); }
        XCALL_RELSTRING(old_zptr);
        MOVD(XPTR, ZPTR); return OK;
    }
    if (D_V(ZPTR) == L) {
        LOCSP_fn(&ZSP, &ZPTR); /* linked string */
        int32_t off = GENVAR_fn(&ZSP);
        if (off) { SETAC(ZPTR, off); SETVC(ZPTR, S); }
        MOVD(XPTR, ZPTR); return OK;
    }
    LOCSP_fn(&ZSP, &ZPTR); /* Generate variable from ZSP */
    int32_t off = GENVAR_fn(&ZSP);
    if (!off) return FAIL;
    SETAC(ZPTR, off); SETVC(ZPTR, S);
    MOVD(XPTR, ZPTR); return OK;
}
