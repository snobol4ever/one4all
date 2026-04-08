/*
 * nmd.c — Naming list commit procedure (v311.sil §17 lines 6055–6091)
 *
 * Faithful C translation of Phil Budne's CSNOBOL4 v311.sil §17 NMD section.
 *
 * SIL source:
 *   NMD    MOVD TCL,NHEDCL
 *   NMD1   ACOMP TCL,NAMICL,INTR13,RTN2  — end of list → return OK
 *          SUM TPTR,NBSPTR,TCL            — compute entry address
 *          GETSPC TSP,TPTR,DESCR          — get captured specifier
 *          GETDC TVAL,TPTR,DESCR+SPEC     — get target variable
 *          GETLG XCL,TSP                  — get length
 *          ACOMP XCL,MLENCL,INTR8         — check &MAXLNGTH
 *          VEQLC TVAL,E,,NAMEXN           — EXPRESSION target?
 *   NMD5   VEQLC TVAL,K,,NMDIC           — KEYWORD target?
 *          RCALL VVAL,GENVAR,(TSPPTR)     — intern substring
 *   NMD4   PUTDC TVAL,DESCR,VVAL         — assign to target
 *          [OUTPUT / TRACE hooks]
 *   NMD2   INCRA TCL,DESCR+SPEC          — advance to next entry
 *          BRANCH NMD1
 *   NMDIC  SPCINT VVAL,TSP,INTR1,NMD4   — keyword: coerce to int
 *   NAMEXN RCALL TVAL,EXPEVL,TVAL,...   — expression: evaluate then NMD5
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M9
 */

#include <string.h>

#include "types.h"
#include "data.h"
#include "nmd.h"
#include "arena.h"
#include "strings.h"
#include "symtab.h"
#include "errors.h" /* INTR1_fn, INTR8_fn, INTR13_fn */

/* External stubs resolved at link time */
extern RESULT_t EXPEVL_fn(void);
extern RESULT_t TRPHND_fn(DESCR_t atptr);
extern void       PUTOUT_fn(DESCR_t yptr, DESCR_t val);

/* Arena block read helpers (mirror scan.c) */
#define GETDC_B(dst, base_d, off_i) \
    memcpy(&(dst), (char*)A2P(D_A(base_d)) + (off_i), sizeof(DESCR_t))

#define PUTDC_B(base_d, off_i, src) \
    memcpy((char*)A2P(D_A(base_d)) + (off_i), &(src),  sizeof(DESCR_t))

/* GETSPC: read a SPEC_t from arena block at base+off */
static inline void getspc(SPEC_t *sp, DESCR_t base, int32_t off)
{
    memcpy(sp, (char*)A2P(D_A(base)) + off, sizeof(SPEC_t));
}

/*====================================================================================================================*/
/* ── NMD_fn ──────────────────────────────────────────────────────────── */
RESULT_t NMD_fn(void)
{
    MOVD(TCL, NHEDCL); /* MOVD TCL,NHEDCL — start from saved head */
    for (;;) {
        if (ACOMP(TCL, NAMICL) == 0) return OK;          /* NMD1: == NAMICL → RTN2 */
        if (ACOMP(TCL, NAMICL) > 0)  { INTR13_fn(); return FAIL; } /* > NAMICL → INTR13 */
        SUM(TPTR, NBSPTR, TCL); /* SUM TPTR,NBSPTR,TCL — entry address */
        getspc(&TSP, TPTR, DESCR); /* GETSPC TSP,TPTR,DESCR — captured substring */
        GETDC_B(TVAL, TPTR, DESCR + (int32_t)sizeof(SPEC_t)); /* GETDC TVAL,TPTR,DESCR+SPEC — target variable */
        D_A(XCL) = TSP.l; /* GETLG XCL,TSP — get length */
        if (ACOMP(XCL, MLENCL) > 0) { INTR8_fn(); return FAIL; } /* INTR8 — string too long */
        if (VEQLC(TVAL, E)) { /* VEQLC TVAL,E,,NAMEXN — EXPRESSION target? */
            /* NAMEXN: PUSH(TVAL); EXPEVL(TVAL); case1→FAIL, case2→NMD5, case3→NEMO */
            MOVD(XPTR, TVAL);
            RESULT_t rc = EXPEVL_fn();
            if (rc == FAIL) return FAIL;
            if (rc == NEMO) return NEMO;
            MOVD(TVAL, XPTR); /* case2: evaluated result → fall into NMD5 */
        }
nmd5:
        if (VEQLC(TVAL, K)) { /* VEQLC TVAL,K,,NMDIC — KEYWORD target? */
            /* NMDIC: SPCINT VVAL,TSP,INTR1,NMD4 */
            if (SPCINT_fn(&VVAL, &TSP) == FAIL) { INTR1_fn(); return FAIL; }
            goto nmd4;
        }
        { /* Normal string target: RCALL VVAL,GENVAR,(TSPPTR) */
            int32_t off = GENVAR_fn(&TSP);
            if (!off) return FAIL;
            SETAC(VVAL, off);
            SETVC(VVAL, S);
        }
nmd4:
        PUTDC_B(TVAL, DESCR, VVAL); /* PUTDC TVAL,DESCR,VVAL — assign value to target variable */
        if (!AEQLC(OUTSW, 0)) { /* AEQLC OUTSW,0,,NMD3 — check &OUTPUT */
            int32_t assoc = locapv_fn(D_A(OUTATL), &TVAL);
            if (assoc) {
                DESCR_t zptr; SETAC(zptr, assoc);
                GETDC_B(zptr, zptr, DESCR); /* NMD-5 fix: read block from ATL entry */
                PUTOUT_fn(zptr, VVAL);
            }
        }
        if (ACOMPC(TRAPCL, 0) > 0) { /* NMD3: trace if TRAPCL > 0 */
            int32_t assoc = locapt_fn(D_A(TVALL), &TVAL);
            if (assoc) {
                DESCR_t save_TCL = TCL, save_NAMICL = NAMICL,
                        save_NHEDCL = NHEDCL;
                MOVD(NHEDCL, NAMICL);
                SETAC(ATPTR, assoc);
                TRPHND_fn(ATPTR);
                MOVD(TCL, save_TCL);
                MOVD(NAMICL, save_NAMICL);
                MOVD(NHEDCL, save_NHEDCL);
            }
        }
        INCRA(TCL, DESCR + (int32_t)sizeof(SPEC_t)); /* NMD2: advance to next entry */
        (void) && nmd5; /* suppress unused-label warning */
    }
}
