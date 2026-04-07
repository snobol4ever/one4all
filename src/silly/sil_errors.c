/*
 * sil_errors.c — Error handlers and termination (v311.sil §22+§23)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M20
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sil_types.h"
#include "sil_data.h"
#include "sil_errors.h"
#include "sil_arena.h"
#include "sil_strings.h"
#include "sil_trace.h"   /* XITHND_fn */

extern void XCALL_io_flushall(void);
extern void XCALL_OUTPUT_fmt(DESCR_t unit, const char *fmt, ...);
extern void STPRNT_fn(int32_t key, DESCR_t blk, SPEC_t *sp);
extern DESCR_t ERRBLK;
extern DESCR_t PUNCH;   /* stderr output block                           */
extern DESCR_t OUTBLK;
extern DESCR_t GCTTTL;

#define GETD_B(dst, base_d, off_d) \
    memcpy(&(dst), (char*)A2P(D_A(base_d))+D_A(off_d), sizeof(DESCR_t))

/* ── FTLEND — fatal termination ─────────────────────────────────────── */
void FTLEND_fn(void)
{
    XCALL_io_flushall();
    {                                                                   /* Print fatal error message from MSGNO table */
        int32_t ety = D_A(ERRTYP);
        if (ety >= 0 && MSGNO[ety]) {
            fprintf(stderr, "%s\n", MSGNO[ety]);
        }
    }
    exit(D_A(RETCOD) ? D_A(RETCOD) : 1);  /* ISTACK: reset system stack (no-op in C — stack is native)  Statistics omitted for brevity in this stub */
}

/* ── FTLTST — non-fatal test ─────────────────────────────────────────── */
/*
 * v311.sil FTLTST line 10458:
 *   SETAC FATLCL,0   — mark non-fatal
 *   [fall to FTERST]
 * FTERST:
 *   ACOMPC ERRLCL,0,,FTLEND,FTLEND — check &ERRLIMIT
 *   DECRA ERRLCL,1
 *   SETAC ERRTYP,0; SETAC ERRTXT,0
 *   [try XITHND, if no handler → FTLEND]
 */
void FTLTST_fn(void)
{
    SETAC(FATLCL, 0);
    if (ACOMPC(ERRLCL, 0) <= 0) { FTLEND_fn(); return; }                                                   /* FTERST: */
    DECRA(ERRLCL, 1);
    SETAC(ERRTYP, 0); SETAC(ERRTXT, 0);
    MOVD(FRTNCL, OCICL); DECRA(FRTNCL, DESCR);
    MOVD(LSTNCL, STNOCL);
    MOVA(LSLNCL, LNNOCL); MOVA(LSFLNM, FILENM);
    if (XITHND_fn() == FAIL) { FTLEND_fn(); }                                                  /* Try SETEXIT handler */
}

/* ── FTLERR — check &FATALLIMIT ─────────────────────────────────────── */
void FTLERR_fn(void)
{
    if (ACOMPC(FTLLCL, 0) <= 0) { FTLEND_fn(); return; }
    FTLTST_fn();
}

/* ── END — normal program end ────────────────────────────────────────── */
void END_fn(void)
{
    if (ACOMPC(ERRLCL, 0) > 0) {                                                      /* Check &ERRLIMIT for re-entry */
        DECRA(ERRLCL, 1);
        SETAC(ERRTYP, 0); SETAC(ERRTXT, 0);
        MOVD(FRTNCL, OCICL); DECRA(FRTNCL, DESCR);
        MOVD(LSTNCL, STNOCL);
        MOVA(LSLNCL, LNNOCL); MOVA(LSFLNM, FILENM);
        if (XITHND_fn() != FAIL) return;
    }
    XCALL_io_flushall();
    if (!AEQLC(BANRCL, 0)) {
        XCALL_OUTPUT_fmt(PUNCH, "Normal termination at level %d\n",               /* Print normal termination message */
                         (int)D_A(LVLCL));
        XCALL_OUTPUT_fmt(PUNCH, "%s:%d: Last statement executed was %d\n",
                         (char*)A2P(D_A(FILENM)),
                         (int)D_A(LNNOCL), (int)D_A(STNOCL));
    }
    exit(0);
}

/* ── SYSCUT — signal/interrupt cut ──────────────────────────────────── */
void SYSCUT_fn(void)
{
    XCALL_io_flushall();
    XCALL_OUTPUT_fmt(PUNCH, "%s:%d: Caught signal %d in statement %d at level %d\n",
                     (char*)A2P(D_A(FILENM)),
                     (int)D_A(LNNOCL), (int)D_A(SIGNCL),
                     (int)D_A(STNOCL), (int)D_A(LVLCL));
    if (!AEQLC(CUTNO, 0)) { SETAC(CUTNO, 1); SETAC(RETCOD, 1); FTLEND_fn(); }
    exit(1);
}

/* ── Error entry points ──────────────────────────────────────────────── */
#define ERR_FTLTST(n) do { SETAC(ERRTYP,(n)); FTLTST_fn(); } while(0)
#define ERR_FTLERR(n) do { SETAC(ERRTYP,(n)); FTLERR_fn(); } while(0)
#define ERR_FTLEND(n) do { SETAC(ERRTYP,(n)); FTLEND_fn(); } while(0)

void AERROR_fn(void) { ERR_FTLTST(2);  }
void ALOC2_fn(void)  { ERR_FTLEND(20); }
void ARGNER_fn(void) { ERR_FTLERR(25); }
void COMP1_fn(void)  { ERR_FTLTST(32); }
void COMP3_fn(void)  { ERR_FTLEND(17); }
void COMP5_fn(void)  { ERR_FTLTST(11); }
void COMP6_fn(void)  { ERR_FTLTST(33); }
void COMP7_fn(void)  { ERR_FTLEND(27); }
void COMP9_fn(void)  { SETAC(ERRTYP,26); DECRA(ESAICL,DESCR); FTLEND_fn(); }
void EROR_fn(void)   {
    SETAC(ERRTYP, 28);
    INCRA(OCICL, DESCR); GETD_B(STNOCL, OCBSCL, OCICL);
    INCRA(OCICL, DESCR); GETD_B(LNNOCL, OCBSCL, OCICL);
    INCRA(OCICL, DESCR); GETD_B(FILENM, OCBSCL, OCICL);
    FTLEND_fn();
}
void EXEX_fn(void)   { ERR_FTLEND(22); }
void INTR1_fn(void)  { ERR_FTLTST(1);  }
void INTR4_fn(void)  { ERR_FTLERR(24); }
void INTR5_fn(void)  { ERR_FTLERR(19); }
void INTR8_fn(void)  { ERR_FTLTST(15); }
void INTR10_fn(void) { ERR_FTLEND(17); }
void INTR13_fn(void) { ERR_FTLEND(17); }
void INTR27_fn(void) { ERR_FTLTST(13); }
void INTR30_fn(void) { ERR_FTLTST(10); }
void INTR31_fn(void) { SETAC(ERRTYP,16); SETAC(SCERCL,3); FTLTST_fn(); }
void LENERR_fn(void) { ERR_FTLTST(14); }
void MAIN1_fn(void)  { ERR_FTLEND(18); }
void NEMO_fn(void)   { ERR_FTLTST(8);  }
void NONAME_fn(void) { ERR_FTLTST(4);  }
void NONARY_fn(void) { ERR_FTLTST(3);  }
void PROTER_fn(void) { ERR_FTLTST(30); }
void SIZERR_fn(void) { ERR_FTLTST(23); }
void UNKNKW_fn(void) { ERR_FTLTST(7);  }
void UNDFFE_fn(void) { ERR_FTLTST(29); }
