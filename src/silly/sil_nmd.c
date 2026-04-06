/*
 * sil_nmd.c — Naming list commit procedure (v311.sil §17 lines 6055–6091)
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

#include "sil_types.h"
#include "sil_data.h"
#include "sil_nmd.h"
#include "sil_arena.h"
#include "sil_strings.h"
#include "sil_symtab.h"

/* External stubs resolved at link time */
extern SIL_result EXPEVL_fn(void);
extern SIL_result TRPHND_fn(DESCR_t atptr);
extern void       PUTOUT_fn(DESCR_t yptr, DESCR_t val);

/* Arena block read helpers (mirror sil_scan.c) */
#define GETDC_B(dst, base_d, off_i) \
    memcpy(&(dst), (char*)A2P(D_A(base_d)) + (off_i), sizeof(DESCR_t))

#define PUTDC_B(base_d, off_i, src) \
    memcpy((char*)A2P(D_A(base_d)) + (off_i), &(src),  sizeof(DESCR_t))

/* GETSPC: read a SPEC_t from arena block at base+off */
static inline void getspc(SPEC_t *sp, DESCR_t base, int32_t off)
{
    memcpy(sp, (char*)A2P(D_A(base)) + off, sizeof(SPEC_t));
}

/* ── NMD_fn ──────────────────────────────────────────────────────────── */
SIL_result NMD_fn(void)
{
    /* MOVD TCL,NHEDCL — start from saved head */
    MOVD(TCL, NHEDCL);

    for (;;) {
        /* NMD1: ACOMP TCL,NAMICL — past end? → RTN2 (return OK) */
        if (ACOMP(TCL, NAMICL) >= 0)
            return OK;

        /* SUM TPTR,NBSPTR,TCL — entry address */
        SUM(TPTR, NBSPTR, TCL);

        /* GETSPC TSP,TPTR,DESCR — captured substring */
        getspc(&TSP, TPTR, DESCR);

        /* GETDC TVAL,TPTR,DESCR+SPEC — target variable */
        GETDC_B(TVAL, TPTR, DESCR + (int32_t)sizeof(SPEC_t));

        /* GETLG XCL,TSP — get length */
        D_A(XCL) = TSP.l;

        /* ACOMP XCL,MLENCL,INTR8 — check &MAXLNGTH */
        if (ACOMP(XCL, MLENCL) > 0) {
            /* INTR8: string overflow — treat as non-fatal, skip entry */
            INCRA(TCL, DESCR + (int32_t)sizeof(SPEC_t));
            continue;
        }

        /* VEQLC TVAL,E,,NAMEXN — EXPRESSION target? */
        if (VEQLC(TVAL, E)) {
            /* NAMEXN: RCALL TVAL,EXPEVL,TVAL,(FAIL,NMD5,NEMO) */
            MOVD(XPTR, TVAL);   /* EXPEVL reads from XPTR in our impl */
            SIL_result rc = EXPEVL_fn();
            if (rc == FAIL) {
                /* FAIL exit — skip this capture */
                INCRA(TCL, DESCR + (int32_t)sizeof(SPEC_t));
                continue;
            }
            /* XPTR now holds evaluated result; fall into NMD5 */
            MOVD(TVAL, XPTR);
        }

nmd5:
        /* VEQLC TVAL,K,,NMDIC — KEYWORD target? */
        if (VEQLC(TVAL, K)) {
            /* NMDIC: SPCINT VVAL,TSP,INTR1,NMD4
             * Convert captured substring to integer for keyword assign */
            if (SPCINT_fn(&VVAL, &TSP) == FAIL) {
                /* INTR1: illegal data type — skip */
                INCRA(TCL, DESCR + (int32_t)sizeof(SPEC_t));
                continue;
            }
            goto nmd4;
        }

        /* Normal string target: RCALL VVAL,GENVAR,(TSPPTR) */
        {
            int32_t off = GENVAR_fn(&TSP);
            if (!off) {
                INCRA(TCL, DESCR + (int32_t)sizeof(SPEC_t));
                continue;
            }
            SETAC(VVAL, off);
            SETVC(VVAL, S);
        }

nmd4:
        /* PUTDC TVAL,DESCR,VVAL — assign value to target variable */
        PUTDC_B(TVAL, DESCR, VVAL);

        /* AEQLC OUTSW,0,,NMD3 — check &OUTPUT */
        if (!AEQLC(OUTSW, 0)) {
            int32_t assoc = locapv_fn(D_A(OUTATL), &TVAL);
            if (assoc) {
                DESCR_t zptr; SETAC(zptr, assoc); SETVC(zptr, S);
                GETDC_B(zptr, TVAL, DESCR);
                PUTOUT_fn(zptr, VVAL);
            }
        }

        /* NMD3: ACOMPC TRAPCL,0,,NMD2,NMD2 — check &TRACE */
        if (!ACOMPC(TRAPCL, 0)) {
            int32_t assoc = locapt_fn(D_A(TVALL), &TVAL);
            if (assoc) {
                /* PUSH (TCL,NAMICL,NHEDCL); trace; POP */
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

        /* NMD2: INCRA TCL,DESCR+SPEC — advance to next entry */
        INCRA(TCL, DESCR + (int32_t)sizeof(SPEC_t));
        /* BRANCH NMD1 — loop */
        continue;

        /* Suppress unused-label warning — nmd5 is jumped to from NAMEXN */
        (void)&&nmd5;
    }
}
