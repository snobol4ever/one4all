/*
 * sil_errors.h — Error handlers and termination (v311.sil §22+§23)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M20
 */

#ifndef SIL_ERRORS_H
#define SIL_ERRORS_H

#include "sil_types.h"

/* Fatal termination paths */
void FTLEND_fn(void);   /* fatal end (always terminates)                 */
void FTLTST_fn(void);   /* non-fatal: test &ERRLIMIT, maybe continue     */
void FTLERR_fn(void);   /* check &FATALLIMIT, then FTLEND or FTLTST      */
void END_fn(void);      /* normal program end                            */
void SYSCUT_fn(void);   /* signal/interrupt cut                          */

/* Error entry points — each sets ERRTYP then calls FTLTST/FTLERR/FTLEND */
void AERROR_fn(void);   /* arithmetic error             ERRTYP=2        */
void ALOC2_fn(void);    /* storage exhausted            ERRTYP=20       */
void ARGNER_fn(void);   /* wrong argument count         ERRTYP=25       */
void COMP1_fn(void);    /* missing END                  ERRTYP=32       */
void COMP3_fn(void);    /* program error                ERRTYP=17       */
void COMP5_fn(void);    /* reading error                ERRTYP=11       */
void COMP6_fn(void);    /* writing error                ERRTYP=33       */
void COMP7_fn(void);    /* erroneous END                ERRTYP=27       */
void COMP9_fn(void);    /* compilation error limit      ERRTYP=26       */
void EROR_fn(void);     /* erroneous statement (inserts stno)           */
void EXEX_fn(void);     /* &STLIMIT exceeded            ERRTYP=22       */
void INTR1_fn(void);    /* illegal data type            ERRTYP=1        */
void INTR4_fn(void);    /* erroneous goto               ERRTYP=24       */
void INTR5_fn(void);    /* failure in goto              ERRTYP=19       */
void INTR8_fn(void);    /* exceeded &MAXLNGTH           ERRTYP=15       */
void INTR10_fn(void);   /* program error (= COMP3)      ERRTYP=17       */
void INTR13_fn(void);   /* program error (= COMP3)      ERRTYP=17       */
void INTR27_fn(void);   /* excessive data types         ERRTYP=13       */
void INTR30_fn(void);   /* illegal argument             ERRTYP=10       */
void INTR31_fn(void);   /* pattern stack overflow       ERRTYP=16       */
void LENERR_fn(void);   /* negative number              ERRTYP=14       */
void MAIN1_fn(void);    /* return from level zero       ERRTYP=18       */
void NEMO_fn(void);     /* variable not present         ERRTYP=8        */
void NONAME_fn(void);   /* null string                  ERRTYP=4        */
void NONARY_fn(void);   /* bad array/table ref          ERRTYP=3        */
void PROTER_fn(void);   /* prototype error              ERRTYP=30       */
void SIZERR_fn(void);   /* size limit exceeded          ERRTYP=23       */
void UNKNKW_fn(void);   /* unknown keyword              ERRTYP=7        */
void UNDFFE_fn(void);   /* undefined function           ERRTYP=29       */

#endif /* SIL_ERRORS_H */
