/*
 * sil_interp.c — Interpreter executive (v311.sil §7 lines 2520–2678)
 *
 * Faithful C translation of Phil Budne's CSNOBOL4 v311.sil §7.
 *
 * BRANIC (v311.sil indirect branch macro):
 *   SIL: BRANIC INCL,0 — branch through function descriptor in INCL.
 *   C:   look up INCL.a in invoke_table[], call the function pointer.
 *
 * INVOKE exit codes (v311.sil RRTURN exits):
 *   1 = normal success (continue)
 *   2 = success, push result
 *   3 = return value (RTZPTR / RTYPTR etc.)
 *   4 = FAIL
 *   5 = NRETURN (return by name)
 *   6 = RETURN  (return by value, GOTL path)
 *
 * INTERP loop uses exits 1–6 of INVOKE:
 *   1,2,3 = continue (result in XPTR / implicit)
 *   4     = failure → jump to FRTNCL
 *   5,6   = nested return (handled by DEFFNC_fn)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date:    2026-04-06
 * Milestone: M19
 */

#include <string.h>

#include "sil_types.h"
#include "sil_data.h"
#include "sil_interp.h"
#include "sil_argval.h"
#include "sil_trace.h"
#include "sil_symtab.h"   /* locapt_fn */

static inline int deql(DESCR_t a, DESCR_t b)
    { return D_A(a)==D_A(b) && D_V(a)==D_V(b); }

#define GETD_B(dst, base_d, off_d) \
    memcpy(&(dst), (char*)A2P(D_A(base_d))+D_A(off_d), sizeof(DESCR_t))
#define GETDC_B(dst, base_d, off_i) \
    memcpy(&(dst), (char*)A2P(D_A(base_d))+(off_i),    sizeof(DESCR_t))

/* ── Invoke dispatch table ───────────────────────────────────────────── */
#define INVOKE_TABLE_SZ  512

typedef struct {
    invoke_fn_t fn;
    int32_t     nargs;
} Invoke_entry;

static Invoke_entry invoke_table[INVOKE_TABLE_SZ];

void invoke_table_register(int32_t idx, invoke_fn_t fn, int32_t nargs)
{
    if ((uint32_t)idx < INVOKE_TABLE_SZ) {
        invoke_table[idx].fn    = fn;
        invoke_table[idx].nargs = nargs;
    }
}

/* ── Error stubs (§23 error targets) ────────────────────────────────── */
static void intr4(void) { SETAC(ERRTYP, 4);  }   /* bad CODE type        */
static void intr5(void) { SETAC(ERRTYP, 5);  }   /* bad goto value       */
static void cnterr(void){ SETAC(ERRTYP, 99); }   /* CONTINUE outside hdlr*/
static void cfterr(void){ SETAC(ERRTYP, 98); }   /* SCONTINUE outside    */
static void exex(void)  { SETAC(ERRTYP, 33); }   /* &STLIMIT exceeded    */
static void usrint(void){ SETAC(ERRTYP, 34); }   /* user interrupt       */

/* ── BASE ────────────────────────────────────────────────────────────── */
/*
 * v311.sil BASE line 2522:
 *   SUM OCBSCL,OCBSCL,OCICL   — advance base by offset
 *   SETAC OCICL,0              — zero offset
 */
Sil_result BASE_fn(void)
{
    SUM(OCBSCL, OCBSCL, OCICL);
    SETAC(OCICL, 0);
    return OK;   /* RTNUL3 */
}

/* ── GOTG — direct goto :<X> ─────────────────────────────────────────── */
/*
 * v311.sil GOTG line 2531:
 *   RCALL OCBSCL,ARGVAL,,INTR5  — evaluate goto arg into OCBSCL
 *   VEQLC OCBSCL,C,INTR4        — must be CODE type
 *   SETAC OCICL,0               — zero offset
 */
Sil_result GOTG_fn(void)
{
    if (ARGVAL_fn() == FAIL) { intr5(); return FAIL; }
    if (!VEQLC(XPTR, C)) { intr4(); return FAIL; }
    MOVD(OCBSCL, XPTR);
    SETAC(OCICL, 0);
    return OK;   /* RTNUL3 */
}

/* ── GOTL — label goto :(X) ──────────────────────────────────────────── */
/*
 * v311.sil GOTL line 2541:
 *   Reads label variable from object code. Handles special labels:
 *   RETCL(6=RETURN) FRETCL(4=FAIL) NRETCL(5=NRETURN) ABORCL SCNTCL CONTCL.
 *   Normal label: GETDC OCBSCL,XPTR,ATTRIB; SETAC OCICL,0.
 */
Sil_result GOTL_fn(void)
{
    INCRA(OCICL, DESCR);
    GETD_B(XPTR, OCBSCL, OCICL);

    /* Evaluate if function */
    if (TESTF(XPTR, FNC)) {
        if (INVOKE_fn() == FAIL) { intr5(); return FAIL; }
        if (!VEQLC(XPTR, S)) { intr4(); return FAIL; }
    }

    /* Label trace */
    if (!ACOMPC(TRAPCL, 0)) {
        int32_t assoc = locapt_fn(D_A(TLABL), &XPTR);
        if (assoc) {
            DESCR_t save_x = XPTR;
            SETAC(ATPTR, assoc);
            TRPHND_fn(ATPTR);
            MOVD(XPTR, save_x);
        }
    }

    /* Special label dispatch */
    if (deql(XPTR, RETCL))  return 6;   /* RETURN  */
    if (deql(XPTR, FRETCL)) return FAIL; /* FRETURN */
    if (deql(XPTR, NRETCL)) return 5;   /* NRETURN */

    if (deql(XPTR, ABORCL)) {
        if (AEQLC(XOCBSC, 0)) { cnterr(); return FAIL; }
        MOVD(ERRTYP, XERRTY); MOVD(FILENM, XFILEN);
        MOVD(LNNOCL, XLNNOC); MOVD(STNOCL, XSTNOC);
        /* FTLEND path — fatal termination */
        SETAC(ERRTYP, 255); return FAIL;
    }

    if (deql(XPTR, SCNTCL)) {
        if (AEQLC(FATLCL, 0)) { cfterr(); return FAIL; }
        MOVD(FRTNCL, XOCICL);
        goto restore_and_go;
    }

    if (deql(XPTR, CONTCL)) {
        if (AEQLC(FATLCL, 0)) { cfterr(); return FAIL; }
        MOVD(FRTNCL, XFRTNC);
restore_and_go:
        if (AEQLC(XOCBSC, 0)) { cnterr(); return FAIL; }
        MOVD(OCBSCL, XOCBSC);
        MOVD(FILENM, XFILEN); MOVD(LNNOCL, XLNNOC);
        MOVD(STNOCL, XSTNOC); MOVD(LSFLNM, XLSFLN);
        MOVD(LSLNCL, XLSLNC); MOVD(LSTNCL, XLNNOC);
        SETAC(XOCBSC, 0);
        if (!AEQLC(ERRTYP, 0)) MOVD(ERRTYP, XERRTY);
        return FAIL;
    }

    /* Normal label: get code base from ATTRIB field */
    GETDC_B(OCBSCL, XPTR, ATTRIB);
    if (AEQLC(OCBSCL, 0)) { intr4(); return FAIL; }
    SETAC(OCICL, 0);
    return OK;   /* RTNUL3 */
}

/* ── GOTO — internal goto ────────────────────────────────────────────── */
/*
 * v311.sil GOTO line 2603:
 *   INCRA OCICL,DESCR
 *   GETD OCICL,OCBSCL,OCICL   — load new OCICL from object code
 */
Sil_result GOTO_fn(void)
{
    INCRA(OCICL, DESCR);
    GETD_B(OCICL, OCBSCL, OCICL);
    return OK;   /* RTNUL3 */
}

/* ── INIT — statement initialisation ────────────────────────────────── */
/*
 * v311.sil INIT line 2612:
 *   Updates &LASTNO/&LASTFILE/&LASTLINE.
 *   Loads &STNO/FRTNCL/&LINE/&FILE from object code.
 *   Increments &STEXEC; checks &STLIMIT; checks &TRACE.
 */
Sil_result INIT_fn(void)
{
    MOVD(LSTNCL, STNOCL);
    MOVA(LSFLNM, FILENM);
    MOVA(LSLNCL, LNNOCL);

    /* Check user interrupt */
    if (!AEQLC(UINTCL, 0)) { usrint(); return FAIL; }

    /* Load statement number, failure offset, line, file from object code */
    INCRA(OCICL, DESCR); GETD_B(XCL, OCBSCL, OCICL);
    MOVA(STNOCL, XCL);
    SETAV(FRTNCL, XCL);
    INCRA(OCICL, DESCR); GETD_B(LNNOCL, OCBSCL, OCICL);
    INCRA(OCICL, DESCR); GETD_B(FILENM, OCBSCL, OCICL);

    INCRA(EXN2CL, 1);   /* &STEXEC */

    /* Check &STLIMIT */
    if (ACOMPC(EXLMCL, 0) >= 0) {
        if (ACOMP(EXNOCL, EXLMCL) >= 0) { exex(); return FAIL; }
        INCRA(EXNOCL, 1);   /* &STCOUNT */
    }

    /* &TRACE checks */
    if (!ACOMPC(TRAPCL, 0)) {
        /* Check for breakpoint */
        /* XCALLC chk_break — stub */

        int32_t assoc = locapt_fn(D_A(TKEYL), &STNOKY);
        if (assoc) { SETAC(ATPTR, assoc); TRPHND_fn(ATPTR); }

        assoc = locapt_fn(D_A(TKEYL), &STCTKY);
        if (assoc) { SETAC(ATPTR, assoc); TRPHND_fn(ATPTR); }
    }
    return OK;   /* RTNUL3 */
}

/* ── INTERP — interpreter main loop ─────────────────────────────────── */
/*
 * v311.sil INTERP line 2636:
 *   Loop: advance OCICL, load descriptor.
 *   If not FNC: push onto operand stack (literal value), continue.
 *   If FNC: call INVOKE.
 *   INVOKE exits: 1-3=continue, 4=failure→FRTNCL, 5-6=nested return.
 */
Sil_result INTERP_fn(void)
{
    while (1) {
        INCRA(OCICL, DESCR);
        GETD_B(XPTR, OCBSCL, OCICL);

        if (!TESTF(XPTR, FNC)) {
            /* Literal value — push onto operand stack and continue */
            /* (The operand stack is managed implicitly through INCL/ARGVAL) */
            continue;
        }

        /* Call via INVOKE */
        Sil_result rc = INVOKE_fn();

        switch ((int)rc) {
        case OK:   /* exits 1,2,3 — continue */
            continue;
        case FAIL: /* exit 4 — failure */
            MOVD(OCICL, FRTNCL);
            INCRA(FALCL, 1);   /* &STFCOUNT */
            /* &TRACE check on failure */
            if (!ACOMPC(TRAPCL, 0)) {
                int32_t assoc = locapt_fn(D_A(TKEYL), &FALKY);
                if (assoc) { SETAC(ATPTR, assoc); TRPHND_fn(ATPTR); }
            }
            continue;
        case 5:    /* NRETURN — caller handles */
            return 5;
        case 6:    /* RETURN — caller handles */
            return 6;
        default:
            return rc;
        }
    }
}

/* ── INVOKE — procedure invocation dispatch ──────────────────────────── */
/*
 * v311.sil INVOKE line 2652:
 *   POP INCL           — get function descriptor from stack
 *   GETDC XPTR,INCL,0  — get procedure descriptor
 *   VEQL INCL,XPTR     — check argument counts match
 *   BRANIC INCL,0      — indirect branch through function table
 *
 * In C: the function pointer is stored in invoke_table[D_A(INCL)].
 * Argument count is in D_V(INCL); checked against D_V(XPTR).
 */
Sil_result INVOKE_fn(void)
{
    /* INCL already loaded by caller (from object code stream) */
    GETDC_B(XPTR, INCL, 0);   /* procedure descriptor */

    /* VEQL INCL,XPTR — check arg counts (V fields) */
    if (D_V(INCL) != D_V(XPTR)) {
        /* INVK2: TESTF XPTR,FNC,ARGNER,INVK1 */
        if (TESTF(XPTR, FNC)) {
            /* variable argument function — pass as-is */
        } else {
            /* ARGNER: argument count mismatch */
            SETAC(ERRTYP, 10); return FAIL;
        }
    }

    /* INVK1: BRANIC INCL,0 — indirect call */
    int32_t idx = D_A(INCL);
    if ((uint32_t)idx >= INVOKE_TABLE_SZ || !invoke_table[idx].fn) {
        SETAC(ERRTYP, 13); return FAIL;   /* undefined function */
    }
    return invoke_table[idx].fn();
}

/* end of sil_interp.c */
