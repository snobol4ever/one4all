/*
 * sn4parse.c — Faithful C translation of SNOBOL4 SIL lexer/parser
 *
 * Follows v311.sil exactly:
 *   CMPILE → compile one statement
 *   FORBLK / FORWRD → inter-field scanning via IBLKTB / FRWDTB
 *   ELEMNT → element analysis via ELEMTB, VARTB, INTGTB, FLITB, UNOPTB
 *   EXPR / EXPR1 → expression with BIOPTB precedence climbing
 *
 * 256-byte chrs[] arrays lifted verbatim from snobol4-2.3.3/syn.c.
 * stream() lifted verbatim from snobol4-2.3.3/lib/stream.c.
 * SIL names used throughout. Tree nodes use SIL STYPE codes and names.
 *
 * Output: s-expression IR tree printed to stdout.
 *
 * Build:  gcc -O0 -g -Wall -o sn4parse sn4parse.c
 * Usage:  ./sn4parse file.sno   or   cat file.sno | ./sn4parse
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6  2026-04-04
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* =========================================================================
 * syntab / acts — from snobol4-2.3.3/include/syntab.h
 * ========================================================================= */

typedef enum { AC_CONTIN=0, AC_STOP, AC_STOPSH, AC_ERROR, AC_GOTO } action_t;
typedef struct syntab syntab_t;
typedef struct { int put; action_t act; syntab_t *go; } acts_t;
struct syntab { const char *name; unsigned char chrs[256]; acts_t *actions; };

/* STYPE — global set by stream(), mirrors SIL's STYPE descriptor */
static int STYPE;

/* =========================================================================
 * stream() — verbatim from snobol4-2.3.3/lib/stream.c
 * Operates on a (ptr, len) specifier pair, SIL SPEC style.
 * Returns ST_STOP, ST_EOS, or ST_ERROR.
 * Sets STYPE = ap->put of the triggering action.
 * ========================================================================= */

typedef enum { ST_STOP, ST_EOS, ST_ERROR } stream_ret_t;

typedef struct { const char *ptr; int len; } spec_t; /* SIL SPEC */

static stream_ret_t stream(spec_t *sp1, spec_t *sp2, syntab_t *tp) {
    unsigned char *cp = (unsigned char *)sp2->ptr;
    int len = sp2->len;
    stream_ret_t ret;
    int put = 0;

    for (; len > 0; cp++, len--) {
        unsigned aindex = tp->chrs[*cp];
        if (aindex == 0) continue;          /* AC_CONTIN fast path */
        acts_t *ap = tp->actions + (aindex - 1);
        if (ap->put) put = ap->put;
        switch (ap->act) {
        case AC_CONTIN: break;
        case AC_STOP:   cp++; len--;        /* accept char */
            /* FALLTHROUGH */
        case AC_STOPSH: ret = ST_STOP; goto done;
        case AC_ERROR:  STYPE = 0; return ST_ERROR;
        case AC_GOTO:   tp = ap->go; break;
        }
    }
    ret = ST_EOS;
done:
    STYPE = put;
    /* sp1 = prefix (what was consumed) */
    sp1->ptr = sp2->ptr;
    sp1->len = sp2->len - len;
    /* sp2 = remainder */
    if (ret != ST_EOS) sp2->ptr += sp1->len;
    sp2->len = len;
    return ret;
}

/* =========================================================================
 * SIL token type constants — from equ.h
 * ========================================================================= */

#define QLITYP  1   /* quoted literal */
#define ILITYP  2   /* integer literal */
#define VARTYP  3   /* variable */
#define NSTTYP  4   /* ( nested expression */
#define FNCTYP  5   /* identifier( function call */
#define FLITYP  6   /* float literal */
#define ARYTYP  7   /* identifier< array ref */
#define NBTYP   1   /* non-blank (FRWDTB) */
#define EQTYP   4   /* = */
#define CLNTYP  5   /* : */
#define EOSTYP  6   /* end of statement */
#define RPTYP   3   /* ) */
#define CMATYP  2   /* , */
#define RBTYP   7   /* ] or > */
#define SGOTYP  2   /* :S( */
#define FGOTYP  3   /* :F( */
#define UGOTYP  1   /* :( unconditional */
#define STOTYP  5   /* :S< direct */
#define FTOTYP  6   /* :F< direct */
#define UTOTYP  4   /* :< direct */
#define CMTTYP  2   /* comment card */
#define CTLTYP  3   /* control card */
#define CNTTYP  4   /* continuation card */
#define NEWTYP  1   /* new statement card */

/* Operator put-codes for BIOPTB / UNOPTB (our integers, map 1-1 to SIL fns) */
#define ADDFN   201  /* + */
#define SUBFN   202  /* - */
#define MPYFN   203  /* * */
#define DIVFN   204  /* / */
#define EXPFN   205  /* ** */
#define ORFN    206  /* | alternation */
#define NAMFN   207  /* . conditional assign */
#define DOLFN   208  /* $ immediate assign */
#define BIATFN  209  /* @ binary cursor */
#define BIPDFN  210  /* # */
#define BIPRFN  211  /* % */
#define BIAMFN  212  /* & */
#define BINGFN  213  /* ~ */
#define BIQSFN  214  /* ? (SNOBOL4) */
#define BISNFN  215  /* ? (SPITBOL scan) */
#define BIEQFN  216  /* = (SPITBOL assign) */

/* Unary put-codes for UNOPTB */
#define PLSFN   301  /* + unary plus */
#define MNSFN   302  /* - unary minus */
#define DOTFN   303  /* . name */
#define INDFN   304  /* $ indirect */
#define STRFN   305  /* * deferred */
#define SLHFN   306  /* / */
#define PRFN    307  /* % */
#define ATFN    308  /* @ cursor */
#define PDFN    309  /* # */
#define KEYFN   310  /* & keyword */
#define NEGFN   311  /* ~ not */
#define BARFN   312  /* | */
#define QUESFN  313  /* ? interrogate */
#define AROWFN  314  /* ^ */

/* =========================================================================
 * Syntax tables — chrs[] verbatim from snobol4-2.3.3/syn.c
 * actions[] use our integer put-codes above.
 * ========================================================================= */

/* ---- LBLTB: label field ---- */
static acts_t LBLTB_actions[] = {
    {0,       AC_CONTIN, NULL},      /* 1: ALPHANUMERIC → CONTIN (goto LBLXTB) */
    {0,       AC_STOPSH, NULL},      /* 2: BLANK/EOS    → STOPSH */
    {0,       AC_ERROR,  NULL},      /* 3: else         → ERROR */
};
/* Note: SIL LBLTB goes to LBLXTB for alphanumeric; we inline it:
   LBLXTB is CONTIN for everything except BLANK/EOS which STOPSH.
   We implement this by making chrs[] identical to LBLTB+LBLXTB merged:
   alphanumeric=CONTIN, blank/EOS=STOPSH, else=ERROR */
static acts_t LBLXTB_actions[] = {
    {0, AC_STOPSH, NULL},   /* 1: BLANK/EOS → STOPSH */
    {0, AC_CONTIN, NULL},   /* 2: else      → CONTIN */
};
syntab_t LBLXTB = { "LBLXTB", {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
}, LBLXTB_actions };

syntab_t LBLTB = { "LBLTB", {
     3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 2, 3, 3, 3, 3,
     3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3,
     3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
}, LBLTB_actions };

/* ---- CARDTB: card type ---- */
static acts_t CARDTB_actions[] = {
    {NEWTYP, AC_STOPSH, NULL},  /* 1: else → new statement */
    {CMTTYP, AC_STOPSH, NULL},  /* 2: * comment */
    {CTLTYP, AC_STOPSH, NULL},  /* 3: - control */
    {CNTTYP, AC_STOPSH, NULL},  /* 4: + continuation */
};
syntab_t CARDTB = { "CARDTB", {
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 4, 1, 3, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
}, CARDTB_actions };

/* ---- IBLKTB: inter-field blank scan → FRWDTB ---- */
/* ---- FRWDTB: forward scan to next token boundary ---- */
static acts_t FRWDTB_actions[] = {
    /* Verbatim from syn_init.h -- index matches chrs[] value (1-based) */
    {EQTYP,  AC_STOP,   NULL},   /* [0] chrs='='->1  */
    {RPTYP,  AC_STOP,   NULL},   /* [1] chrs=')'->2  */
    {RBTYP,  AC_STOP,   NULL},   /* [2] chrs='>'->3  */
    {CMATYP, AC_STOP,   NULL},   /* [3] chrs=','->4  */
    {CLNTYP, AC_STOP,   NULL},   /* [4] chrs=':'->5  */
    {EOSTYP, AC_STOP,   NULL},   /* [5] chrs=';'->6  */
    {NBTYP,  AC_STOPSH, NULL},   /* [6] chrs=other->7 STOPSH (don't consume) */
};
syntab_t FRWDTB = { "FRWDTB", {
     7, 7, 7, 7, 7, 7, 7, 7, 7, 0, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     0, 7, 7, 7, 7, 7, 7, 7, 7, 2, 7, 7, 4, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 5, 6, 7, 1, 3, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 3, 7, 1,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
}, FRWDTB_actions };

static acts_t IBLKTB_actions[] = {
    {0,      AC_GOTO,   &FRWDTB},  /* 1: non-blank → FRWDTB */
    {EOSTYP, AC_STOPSH, NULL},     /* 2: EOS */
    {0,      AC_ERROR,  NULL},     /* 3: else */
};
syntab_t IBLKTB = { "IBLKTB", {
     3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
}, IBLKTB_actions };

/* ---- ELEMTB: element type dispatch ---- */
static acts_t ELEMTB_actions[] = {
    {ILITYP, AC_GOTO,   NULL},   /* 1: NUMBER → integer (→INTGTB) */
    {VARTYP, AC_GOTO,   NULL},   /* 2: LETTER → variable (→VARTB) */
    {QLITYP, AC_GOTO,   NULL},   /* 3: ' single quote */
    {QLITYP, AC_GOTO,   NULL},   /* 4: " double quote */
    {NSTTYP, AC_STOP,   NULL},   /* 5: ( nested */
    {0,      AC_ERROR,  NULL},   /* 6: else */
};
/* Forward declarations for goto targets */
syntab_t VARTB, INTGTB, SQLITB, DQLITB;

syntab_t ELEMTB = { "ELEMTB", {
     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
     6, 6, 4, 6, 6, 6, 6, 3, 5, 6, 6, 6, 6, 6, 6, 6,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 6, 6, 6, 6, 6, 6,
     6, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 6, 6, 6, 6, 6,
     6, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 6, 6, 6, 6, 6,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
}, ELEMTB_actions };

/* ---- VARTB: variable / function name ---- */
static acts_t VARTB_actions[] = {
    {VARTYP, AC_STOPSH, NULL},  /* 1: TERMINATOR → plain variable */
    {FNCTYP, AC_STOP,   NULL},  /* 2: (  → function call */
    {ARYTYP, AC_STOP,   NULL},  /* 3: <  → array ref */
    {0,      AC_ERROR,  NULL},  /* 4: else */
};
syntab_t VARTB = { "VARTB", {
     4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     1, 4, 4, 4, 4, 4, 4, 4, 2, 1, 4, 4, 1, 4, 0, 4,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 1, 3, 4, 1, 4,
     4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 4, 1, 4, 0,
     4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
}, VARTB_actions };

/* ---- INTGTB: integer digits ---- */
syntab_t FLITB;  /* forward */
static acts_t INTGTB_actions[] = {
    {0,      AC_CONTIN, NULL},   /* 1: NUMBER  → continue */
    {ILITYP, AC_STOPSH, NULL},   /* 2: TERMINATOR → integer done */
    {FLITYP, AC_GOTO,   &FLITB}, /* 3: . → float */
    {FLITYP, AC_GOTO,   NULL},   /* 4: e/E exponent → float (→EXPTB) */
    {0,      AC_ERROR,  NULL},   /* 5: else */
};
syntab_t EXPTB, EXPBTB; /* forward */
syntab_t INTGTB = { "INTGTB", {
     5, 5, 5, 5, 5, 5, 5, 5, 5, 2, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     2, 5, 5, 5, 5, 5, 5, 5, 2, 2, 5, 5, 2, 5, 3, 5,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5, 2, 2, 5, 2, 5,
     5, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 2, 5, 2, 5, 2,
     5, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
}, INTGTB_actions };

/* ---- FLITB: float fraction digits ---- */
static acts_t FLITB_actions[] = {
    {0,      AC_CONTIN, NULL},   /* 1: NUMBER → continue */
    {FLITYP, AC_STOPSH, NULL},   /* 2: TERMINATOR → done */
    {FLITYP, AC_GOTO,   NULL},   /* 3: e/E → exponent (→EXPTB) */
    {0,      AC_ERROR,  NULL},   /* 4: else */
};
syntab_t FLITB = { "FLITB", {
     4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     2, 4, 4, 4, 4, 4, 4, 4, 2, 2, 4, 4, 2, 4, 4, 4,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 2, 2, 4, 2, 4,
     4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 2, 4, 2,
     4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
}, FLITB_actions };

/* ---- EXPTB / EXPBTB: exponent sign and digits ---- */
static acts_t EXPBTB_actions[] = {
    {0,      AC_CONTIN, NULL},   /* 1: NUMBER → continue */
    {FLITYP, AC_STOPSH, NULL},   /* 2: TERMINATOR → done */
    {0,      AC_ERROR,  NULL},   /* 3: else */
};
syntab_t EXPBTB = { "EXPBTB", {
     3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     2, 3, 3, 3, 3, 3, 3, 3, 2, 2, 3, 3, 2, 3, 3, 3,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 2, 2, 3, 2, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 3, 2,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
}, EXPBTB_actions };
static acts_t EXPTB_actions[] = {
    {0, AC_GOTO, &EXPBTB},  /* 1: +/- sign → EXPBTB */
    {0, AC_GOTO, &EXPBTB},  /* 2: NUMBER   → EXPBTB */
    {0, AC_ERROR, NULL},    /* 3: else */
};
syntab_t EXPTB = { "EXPTB", {
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 1, 3, 3,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
}, EXPTB_actions };

/* ---- SQLITB / DQLITB: string literals ---- */
static acts_t SQLITB_actions[] = {
    {0, AC_STOP,   NULL},  /* 1: ' closing quote → STOP (accept) */
    {0, AC_CONTIN, NULL},  /* 2: else → continue */
};
syntab_t SQLITB = { "SQLITB", {
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
}, SQLITB_actions };
static acts_t DQLITB_actions[] = {
    {0, AC_STOP,   NULL},  /* 1: " closing quote */
    {0, AC_CONTIN, NULL},  /* 2: else */
};
syntab_t DQLITB = { "DQLITB", {
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
}, DQLITB_actions };

/* ---- UNOPTB: unary prefix operators ---- */
#define UOP_PLS  301
#define UOP_MNS  302
#define UOP_DOT  303
#define UOP_IND  304
#define UOP_STR  305
#define UOP_SLH  306
#define UOP_PCT  307
#define UOP_AT   308
#define UOP_PD   309
#define UOP_KEY  310
#define UOP_NEG  311
#define UOP_BAR  312
#define UOP_QUE  313
#define UOP_ARW  314

static acts_t UNOPTB_actions[] = {
    {UOP_PLS,  AC_STOPSH, NULL},  /*  1: + */
    {UOP_MNS,  AC_STOPSH, NULL},  /*  2: - */
    {UOP_DOT,  AC_STOPSH, NULL},  /*  3: . */
    {UOP_IND,  AC_STOPSH, NULL},  /*  4: $ */
    {UOP_STR,  AC_STOPSH, NULL},  /*  5: * (deferred) */
    {UOP_SLH,  AC_STOPSH, NULL},  /*  6: / */
    {UOP_PCT,  AC_STOPSH, NULL},  /*  7: % */
    {UOP_AT,   AC_STOPSH, NULL},  /*  8: @ */
    {UOP_PD,   AC_STOPSH, NULL},  /*  9: # */
    {UOP_KEY,  AC_STOPSH, NULL},  /* 10: & keyword */
    {UOP_NEG,  AC_STOPSH, NULL},  /* 11: ~ */
    {UOP_BAR,  AC_STOPSH, NULL},  /* 12: | */
    {UOP_QUE,  AC_STOPSH, NULL},  /* 13: ? */
    {UOP_ARW,  AC_STOPSH, NULL},  /* 14: ^ */
    {0,        AC_ERROR,  NULL},  /* 15: else */
};
syntab_t UNOPTB = { "UNOPTB", {
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15, 9, 4, 7,10, 15,15,15, 5, 1,15, 2,15, 6,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,13,
     8,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,13,15,14,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,12,15,11,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
}, UNOPTB_actions };

/* Forward declarations for BIOPTB goto targets */
extern syntab_t TBLKTB, STARTB;

/* ---- BIOPTB: binary operators (SNOBOL4 mode) ---- */
static acts_t BIOPTB_actions[] = {
    {ADDFN,  AC_GOTO, &TBLKTB},   /*  1: + */
    {SUBFN,  AC_GOTO, &TBLKTB},   /*  2: - */
    {NAMFN,  AC_GOTO, &TBLKTB},   /*  3: . */
    {DOLFN,  AC_GOTO, &TBLKTB},   /*  4: $ */
    {MPYFN,  AC_GOTO, &STARTB},   /*  5: * (may be **) */
    {DIVFN,  AC_GOTO, &TBLKTB},   /*  6: / */
    {BIATFN, AC_GOTO, &TBLKTB},   /*  7: @ */
    {BIPDFN, AC_GOTO, &TBLKTB},   /*  8: # */
    {BIPRFN, AC_GOTO, &TBLKTB},   /*  9: % */
    {EXPFN,  AC_GOTO, &TBLKTB},   /* 10: ^ exponent */
    {ORFN,   AC_GOTO, &TBLKTB},   /* 11: | alt */
    {BIAMFN, AC_GOTO, &TBLKTB},   /* 12: & */
    {BINGFN, AC_GOTO, &TBLKTB},   /* 13: ~ */
    {BIQSFN, AC_GOTO, &TBLKTB},   /* 14: ? */
    {0,      AC_ERROR,NULL},       /* 15: else */
};

/* STARTB: disambiguate * vs ** */
static acts_t STARTB_actions[] = {
    {0,     AC_STOP,  NULL},       /* 1: blank → * (multiply) */
    {EXPFN, AC_GOTO,  &TBLKTB},   /* 2: * again → ** (exponent) */
    {0,     AC_ERROR, NULL},       /* 3: else */
};
syntab_t STARTB = { "STARTB", {
     3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
}, STARTB_actions };

/* TBLKTB: trailing blank after operator */
static acts_t TBLKTB_actions[] = {
    {0, AC_STOP,  NULL},  /* 1: blank → stop */
    {0, AC_ERROR, NULL},  /* 2: else  → error */
};
syntab_t TBLKTB = { "TBLKTB", {
     2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
}, TBLKTB_actions };

syntab_t BIOPTB = { "BIOPTB", {
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,11,15, 8, 4, 9,12,15,15,15, 5, 1,15, 2, 3, 6,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,14,
     7,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,13,15,10,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,11,15,13,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
}, BIOPTB_actions };

/* ---- GOTOTB / GOTSTB / GOTFTB: goto field ---- */
static acts_t GOTSTB_actions[] = {
    {SGOTYP, AC_STOP, NULL},  /* 1: ( → S goto */
    {STOTYP, AC_STOP, NULL},  /* 2: < → S direct goto */
    {0,      AC_ERROR,NULL},  /* 3: else */
};
syntab_t GOTSTB = { "GOTSTB", {
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 1, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
}, GOTSTB_actions };
static acts_t GOTFTB_actions[] = {
    {FGOTYP, AC_STOP, NULL},  /* 1: ( → F goto */
    {FTOTYP, AC_STOP, NULL},  /* 2: < → F direct goto */
    {0,      AC_ERROR,NULL},  /* 3: else */
};
syntab_t GOTFTB = { "GOTFTB", {
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 1, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
}, GOTFTB_actions };
static acts_t GOTOTB_actions[] = {
    /* Order matches chrs[]: chrs['(']=1, chrs['<']=2(?), chrs['S']=3, chrs['F']=4, else=5 */
    {UGOTYP, AC_STOP,  NULL},      /* [0] chrs=1: ( unconditional */
    {UTOTYP, AC_STOP,  NULL},      /* [1] chrs=2: < unconditional direct */
    {0,      AC_GOTO,  &GOTSTB},   /* [2] chrs=3: S -> success */
    {0,      AC_GOTO,  &GOTFTB},   /* [3] chrs=4: F -> failure */
    {0,      AC_ERROR, NULL},      /* [4] chrs=5: else */
};
syntab_t GOTOTB = { "GOTOTB", {
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 1, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 2, 5, 1, 5,
     5, 5, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 2, 5, 5,
     5, 5, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
}, GOTOTB_actions };

/* Wire up forward-declared goto targets */
static void init_tables(void) {
    ELEMTB_actions[0].go  = &INTGTB;
    ELEMTB_actions[1].go  = &VARTB;
    ELEMTB_actions[2].go  = &SQLITB;
    ELEMTB_actions[3].go  = &DQLITB;
    INTGTB_actions[3].go  = &EXPTB;   /* e/E exponent */
    FLITB_actions[2].go   = &EXPTB;
}

/* =========================================================================
 * Tree node — uses SIL names / codes throughout
 * ========================================================================= */

typedef struct NODE NODE;
struct NODE {
    int      stype;          /* STYPE code: QLITYP, VARTYP, FNCTYP, ADDFN, ... */
    char    *text;           /* token text (label, var name, literal value) */
    NODE   **children;
    int      nchildren, nalloc;
    /* for numeric literals */
    long long ival;
    double    fval;
};

static NODE *node_new(int stype, const char *text, int tlen) {
    NODE *n  = calloc(1, sizeof *n);
    n->stype = stype;
    n->text  = tlen >= 0 ? strndup(text, tlen) : strdup(text ? text : "");
    return n;
}

static void node_add(NODE *parent, NODE *child) {
    if (!child) return;
    if (parent->nchildren >= parent->nalloc) {
        parent->nalloc = parent->nalloc ? parent->nalloc * 2 : 4;
        parent->children = realloc(parent->children,
                                   parent->nalloc * sizeof(NODE*));
    }
    parent->children[parent->nchildren++] = child;
}

/* =========================================================================
 * Compiler state — mirrors SIL globals
 * TEXTSP = remaining source text
 * XSP    = last token prefix extracted by stream()
 * BRTYPE = break type set by FORBLK/FORWRD (mirrors SIL BRTYPE)
 * ========================================================================= */

static spec_t TEXTSP;    /* remaining input */
static spec_t XSP;       /* last prefix from stream() */
static int    BRTYPE;    /* break type from FORWRD/FORBLK */

/* Error reporting */
static int  g_error;
static char g_errmsg[256];
static void sil_error(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(g_errmsg, sizeof g_errmsg, fmt, ap);
    va_end(ap);
    g_error = 1;
}

/* =========================================================================
 * FORWRD / FORBLK — inter-field scanning (v311.sil lines 2214, 2241)
 *
 * FORWRD: STREAM XSP,TEXTSP,FRWDTB → sets BRTYPE
 * FORBLK: STREAM XSP,TEXTSP,IBLKTB → sets BRTYPE (skips leading blank first)
 * ========================================================================= */

static void FORWRD(void) {
    stream_ret_t r = stream(&XSP, &TEXTSP, &FRWDTB);
    if (r == ST_ERROR) { sil_error("FORWRD: scan error"); return; }
    BRTYPE = STYPE;
}

static void FORBLK(void) {
    stream_ret_t r = stream(&XSP, &TEXTSP, &IBLKTB);
    if (r == ST_ERROR) { sil_error("FORBLK: scan error"); return; }
    BRTYPE = STYPE;
    if (r == ST_STOP && STYPE == 0) {
        /* IBLKTB sent us to FRWDTB; BRTYPE already set via STYPE */
    }
}

/* =========================================================================
 * BINOP — reads binary operator via BIOPTB (called from EXPR)
 * Returns the STYPE (put code = ADDFN, SUBFN, etc.) or 0 if none.
 * Mirrors SIL's EXPR2: RCALL EXOPCL,BINOP
 * ========================================================================= */

static int BINOP(void) {
    spec_t saved = TEXTSP;
    spec_t tok;
    stream_ret_t r = stream(&tok, &TEXTSP, &BIOPTB);
    if (r == ST_ERROR) { TEXTSP = saved; return 0; }  /* no operator */
    return STYPE;  /* ADDFN, SUBFN, MPYFN, EXPFN, ORFN, NAMFN, DOLFN... */
}

/* =========================================================================
 * Operator precedence — mirrors SIL's precedence descriptor in function nodes
 * SIL stores prec in CODE+2*DESCR of each operator function descriptor.
 * We table it directly.
 * ========================================================================= */

static int op_prec(int fn) {
    switch (fn) {
    case ORFN:   return 2;   /* | alternation — lowest */
    case ADDFN:
    case SUBFN:  return 3;
    case MPYFN:
    case DIVFN:
    case BIPRFN: return 4;
    case EXPFN:  return 5;   /* ** — right-assoc */
    case BIATFN: return 6;   /* @ cursor */
    case NAMFN:
    case DOLFN:  return 7;   /* . $ captures — right-assoc */
    case BIAMFN: return 1;   /* & — lowest of all */
    default:     return 3;
    }
}

static int op_right_assoc(int fn) {
    return fn == EXPFN || fn == NAMFN || fn == DOLFN || fn == BIATFN;
}

static const char *fn_name(int fn) {
    switch(fn) {
    case ADDFN:  return "ADDFN(+)";
    case SUBFN:  return "SUBFN(-)";
    case MPYFN:  return "MPYFN(*)";
    case DIVFN:  return "DIVFN(/)";
    case EXPFN:  return "EXPFN(**)";
    case ORFN:   return "ORFN(|)";
    case NAMFN:  return "NAMFN(.)";
    case DOLFN:  return "DOLFN($)";
    case BIATFN: return "BIATFN(@)";
    case BIPDFN: return "BIPDFN(#)";
    case BIPRFN: return "BIPRFN(%)";
    case BIAMFN: return "BIAMFN(&)";
    case BINGFN: return "BINGFN(~)";
    case BIQSFN: return "BIQSFN(?)";
    default: { static char buf[16]; snprintf(buf,16,"FN(%d)",fn); return buf; }
    }
}

static const char *stype_name(int st) {
    /* SIL STYPE codes share numeric values across contexts;
       many are the same integer in different tables. Name by context-free label. */
    static const char *names[] = {
        "ST(0)",        /* 0 */
        "QLITYP/UGOTYP/NEWTYP/NBTYP", /* 1 */
        "ILITYP/SGOTYP/CMATYP/CMTTYP", /* 2 */
        "VARTYP/FGOTYP/RPTYP/CTLTYP",  /* 3 */
        "NSTTYP/EQTYP/UTOTYP/CNTTYP",  /* 4 */
        "FNCTYP/CLNTYP/STOTYP",         /* 5 */
        "FLITYP/EOSTYP/FTOTYP",         /* 6 */
        "ARYTYP/RBTYP",                  /* 7 */
    };
    if (st >= 0 && st <= 7) return names[st];
    { static char buf[20]; snprintf(buf,20,"ST(%d)",st); return buf; }
}

/* =========================================================================
 * ELEMNT — element analysis procedure (v311.sil line 1924)
 *
 * Mirrors SIL:
 *   RCALL ELEMND,UNOP     — get unary operator chain
 *   STREAM XSP,TEXTSP,ELEMTB — break out element type
 *   SELBRA STYPE,(ELEILT,ELEVBL,ELENST,ELEFNC,ELEFLT,ELEARY)
 * ========================================================================= */

static NODE *EXPR(void);  /* forward */
static NODE *EXPR1(void); /* forward */
static NODE *expr_prec_continue(NODE *left, int min_prec); /* forward */

static NODE *ELEMNT(void) {
    if (g_error) return NULL;

    /* --- UNOP: collect unary prefix operators (UNOPTB) --- */
    /* SIL: RCALL ELEMND,UNOP,,RTN2  — builds unary operator tree */
    NODE *unary_chain = NULL;
    NODE *unary_tail  = NULL;
    for (;;) {
        spec_t saved = TEXTSP;
        spec_t tok;
        stream_ret_t r = stream(&tok, &TEXTSP, &UNOPTB);
        if (r == ST_ERROR) { TEXTSP = saved; break; }
        int uop = STYPE;
        static const char *uop_names[] = {
            "?","UOP_PLS","UOP_MNS","UOP_DOT","UOP_IND","UOP_STR",
            "UOP_SLH","UOP_PCT","UOP_AT","UOP_PD","UOP_KEY",
            "UOP_NEG","UOP_BAR","UOP_QUE","UOP_ARW"
        };
        const char *nm = (uop >= 301 && uop <= 314)
                         ? uop_names[uop-300] : "UOP?";
        NODE *unode = node_new(uop, nm, -1);
        if (!unary_chain) { unary_chain = unary_tail = unode; }
        else              { node_add(unary_tail, unode); unary_tail = unode; }
    }

    /* --- STREAM XSP,TEXTSP,ELEMTB — classify element --- */
    stream_ret_t r = stream(&XSP, &TEXTSP, &ELEMTB);
    if (r == ST_ERROR) {
        sil_error("ELEMNT: illegal character");
        return NULL;
    }
    int elem_stype = STYPE;

    NODE *atom = NULL;

    /* Classify by first character consumed (XSP.ptr[0]),
       since STYPE reflects the terminal table after all GOTO chains:
       - LETTER  → VARTB final STYPE = VARTYP/FNCTYP/ARYTYP
       - NUMBER  → INTGTB final STYPE = ILITYP or FLITYP
       - QUOTE   → SQLITB/DQLITB final STYPE = 0 (just stopped)
       - LPAREN  → AC_STOP, elem_stype = NSTTYP
       SIL's SELBRA STYPE,(,ELEILT,ELEVBL,ELENST,ELEFNC,ELEFLT,ELEARY)
       works because ELEMTB put-codes go into STYPE before the GOTO. */
    unsigned char first = XSP.len > 0 ? (unsigned char)XSP.ptr[0] : 0;
    int is_digit  = (first >= '0' && first <= '9');
    int is_letter = ((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') || first == '_');
    int is_quote  = (first == 39 || first == 34);  /* 39=' 34=" */
    int is_lparen = (first == '(');

    /* elem_stype from ELEMTB put before GOTO: ILITYP=NUMBER, VARTYP=LETTER,
       QLITYP=QUOTE, NSTTYP=LPAREN. After chasing GOTOs STYPE changes.
       Use first-char to re-establish the branch. */
    (void)elem_stype; /* replaced by first-char dispatch */
    int dispatch = is_digit ? ILITYP : is_letter ? VARTYP :
                   is_quote ? QLITYP : is_lparen ? NSTTYP : 0;
    switch (dispatch) {

    case ILITYP: {  /* ELEILT: integer — STREAM consumed digits via INTGTB */
        /* XSP holds the digit string; STYPE may be FLITYP if it became real */
        /* INTGTB's AC_GOTO to FLITB already consumed the fraction */
        int final_type = STYPE; /* may be ILITYP or FLITYP */
        char buf[64]; memcpy(buf, XSP.ptr, XSP.len < 63 ? XSP.len : 63);
        buf[XSP.len < 63 ? XSP.len : 63] = '\0';
        atom = node_new(final_type, buf, -1);
        if (final_type == ILITYP) atom->ival = atoll(buf);
        else                      atom->fval = atof(buf);
        break;
    }

    case FLITYP: {  /* ELEFLT: real literal starting with . */
        char buf[64]; memcpy(buf, XSP.ptr, XSP.len < 63 ? XSP.len : 63);
        buf[XSP.len < 63 ? XSP.len : 63] = '\0';
        atom = node_new(FLITYP, buf, -1);
        atom->fval = atof(buf);
        break;
    }

    case QLITYP: {  /* ELEVBL (quote): STREAM consumed up to closing quote */
        /* XSP includes the closing quote; strip both delimiters */
        const char *p = XSP.ptr + 1;          /* skip open quote */
        int len = XSP.len - 2;                 /* strip both quotes */
        if (len < 0) len = 0;
        atom = node_new(QLITYP, p, len);
        break;
    }

    case VARTYP: {  /* ELEVBL: variable — STREAM via VARTB; STYPE = VARTYP/FNCTYP/ARYTYP */
        int final = STYPE;
        const char *p = XSP.ptr;
        int len = XSP.len;
        /* strip trailing ( or < consumed by VARTB STOP */
        if (final == FNCTYP || final == ARYTYP) len--;
        atom = node_new(final, p, len);

        if (final == FNCTYP) {   /* ELEFNC: function call */
            /* SIL ELEMN2: RCALL EXELND,EXPR; FORWRD to get delimiter */
            while (!g_error) {
                NODE *arg = EXPR();
                node_add(atom, arg);
                FORWRD();  /* FORWRD via FRWDTB sets BRTYPE to actual delimiter */
                if (BRTYPE == RPTYP) break;
                if (BRTYPE == CMATYP) continue;
                sil_error("ELEMNT: expected ) or , in arg list, got BRTYPE=%d", BRTYPE);
                break;
            }
        } else if (final == ARYTYP) {  /* ELEARY: array subscript */
            while (!g_error) {
                NODE *sub = EXPR();
                node_add(atom, sub);
                FORWRD();
                if (BRTYPE == RBTYP) break;
                if (BRTYPE == CMATYP) continue;
                sil_error("ELEMNT: expected > or , in subscript, got BRTYPE=%d", BRTYPE);
                break;
            }
        }
        break;
    }

    case NSTTYP: {  /* ELENST: parenthesized expression */
        atom = EXPR();
        /* BRTYPE should be RPTYP after EXPR returns */
        break;
    }

    default:
        sil_error("ELEMNT: unknown element STYPE=%d", elem_stype);
        return NULL;
    }

    /* Wrap atom in unary chain (innermost last) */
    if (unary_tail) {
        node_add(unary_tail, atom);
        atom = unary_chain;
    }

    return atom;
}

/* =========================================================================
 * EXPR / EXPR1 — expression compiler (v311.sil lines 2093, 2101)
 *
 * Mirrors SIL's operator-precedence tree builder:
 *   EXPR:  RCALL EXELND,ELEMNT → then EXPR2 loop (BINOP + precedence)
 *   EXPR1: same but called from CMPILE for right-hand side of SCAN
 *
 * SIL uses ADDSON/ADDSIB/INSERT to wire the tree.
 * We do Pratt-style precedence climbing — same result.
 * BRTYPE is set by ELEMNT's closing delimiter (RPTYP, CMATYP, EQTYP, etc.)
 * ========================================================================= */

static NODE *expr_prec(int min_prec);

static NODE *EXPR(void) {
    if (g_error) return NULL;
    /* EXPR: RCALL EXELND,ELEMNT,,(RTN1,EXPNUL) */
    spec_t saved = TEXTSP;
    NODE *left = ELEMNT();
    if (!left || g_error) {
        /* EXPNUL: return null node */
        TEXTSP = saved;
        return node_new(0, "NULL", -1);
    }
    /* EXPR2: RCALL EXOPCL,BINOP loop */
    return expr_prec_continue(left, 0);
}

static NODE *EXPR1(void) {
    /* EXPR1: PUSH EXPRND; RCALL EXELND,ELEMNT; POP EXPRND; → EXPR2 */
    return EXPR();
}

static NODE *expr_prec_continue(NODE *left, int min_prec);

static NODE *expr_prec(int min_prec) {
    NODE *left = ELEMNT();
    if (!left || g_error) return node_new(0, "NULL", -1);
    return expr_prec_continue(left, min_prec);
}

static NODE *expr_prec_continue(NODE *left, int min_prec) {
    for (;;) {
        spec_t saved = TEXTSP;
        int op = BINOP();
        if (!op) { TEXTSP = saved; break; }     /* no operator → done */

        int prec  = op_prec(op);
        if (prec < min_prec) { TEXTSP = saved; break; }

        int next_min = op_right_assoc(op) ? prec : prec + 1;
        NODE *right = expr_prec(next_min);

        /* Juxtaposition (blank-separated): BIOPTB returns 0 above.
         * Handled separately as CATFN below in CMPILE context. */

        NODE *binop = node_new(op, fn_name(op), -1);
        node_add(binop, left);
        node_add(binop, right);
        left = binop;
    }

    /* BRTYPE: set it based on what stopped us */
    /* FORWRD already set BRTYPE when FORBLK was called before ELEMNT */
    /* For closing delimiters inside expressions (args, subscripts),
       VARTB / ELEMTB set STYPE which we propagate as BRTYPE */
    BRTYPE = STYPE;
    return left;
}

/* =========================================================================
 * CMPILE — compile one statement (v311.sil line 1608)
 *
 * Statement node has children: [label?, subject, pattern?, =replacement?, goto?]
 * We label children by STYPE of the enclosing CMPILE branch.
 * ========================================================================= */

typedef struct STMT STMT;
struct STMT {
    char *label;
    NODE *subject;
    NODE *pattern;
    NODE *replacement;   /* present if has_eq */
    int   has_eq;
    char *go_s;          /* :S label */
    char *go_f;          /* :F label */
    char *go_u;          /* :( label */
    int   is_end;
    STMT *next;
};

/* Read a label/variable name from XSP (after stream set it) */
static char *xsp_dup(void) {
    return strndup(XSP.ptr, XSP.len);
}

static STMT *CMPILE(void) {
    if (g_error) return NULL;
    STMT *s = calloc(1, sizeof *s);

    /* CMPIL0: STREAM XSP,TEXTSP,LBLTB — break out label field */
    stream(&XSP, &TEXTSP, &LBLTB);
    if (XSP.len > 0)
        s->label = xsp_dup();

    /* CMPILA: RCALL ,FORBLK — get to next character */
    FORBLK();

    /* Check for END statement */
    if (XSP.len == 0 && BRTYPE == EOSTYP) {
        /* blank body */
        return s;
    }

    /* If first char after blank was a colon → goto field only */
    if (BRTYPE == CLNTYP) goto CMPGO;

    /* If non-blank (NBTYP) → subject field */
    /* CMPSUB: RCALL SUBJND,ELEMNT */
    if (BRTYPE != EOSTYP) {
        s->subject = ELEMNT();
        if (g_error) return s;

        /* Check for END label */
        if (s->subject && s->subject->stype == VARTYP &&
            strcmp(s->subject->text, "END") == 0) {
            s->is_end = 1;
            return s;
        }

        /* FORBLK: get to next field */
        FORBLK();
    
        /* CMPSB1: after subject */
        if (BRTYPE == EQTYP) goto CMPFRM;   /* = replacement */
        if (BRTYPE == CLNTYP) goto CMPGO;   /* : goto */
        if (BRTYPE == EOSTYP) return s;     /* bare invoke */

        /* Otherwise: pattern field follows */
        /* CMPAT2: RCALL PATND,EXPR */
        s->pattern = EXPR();
        if (g_error) return s;
        FORBLK();

        if (BRTYPE == EQTYP) goto CMPASP;
        if (BRTYPE == CLNTYP) goto CMPGO;
        return s;
    }
    return s;

CMPFRM:  /* SUBJECT = REPLACEMENT */
    s->has_eq = 1;
    FORBLK();
    s->replacement = EXPR();
    if (g_error) return s;
    FORBLK();
    if (BRTYPE == CLNTYP) goto CMPGO;
    return s;

CMPASP:  /* SUBJECT PATTERN = REPLACEMENT (= consumed; FORBLK skips space) */
    s->has_eq = 1;
    FORBLK();
    s->replacement = EXPR();
    if (g_error) return s;
    FORBLK();
    if (BRTYPE == CLNTYP) goto CMPGO;
    return s;

CMPGO: {
    /* STREAM XSP,TEXTSP,GOTOTB — classify goto type */
    stream_ret_t r = stream(&XSP, &TEXTSP, &GOTOTB);
    if (r == ST_ERROR) { sil_error("CMPGO: bad goto"); return s; }
    int gotype = STYPE;

    /* CMPSGO / CMPFGO / CMPUGO */
    if (gotype == UGOTYP || gotype == UTOTYP) {
        /* Unconditional: :( label ) */
        NODE *lbl = EXPR();
        s->go_u = lbl ? strdup(lbl->text ? lbl->text : "") : NULL;
        /* consume closing ) or > */
        FORBLK();
        return s;
    }

    if (gotype == SGOTYP || gotype == STOTYP) {
        /* Success: :S( label ) */
        NODE *lbl = EXPR();
        s->go_s = lbl ? strdup(lbl->text ? lbl->text : "") : NULL;
        FORBLK();
        /* may have :F after */
        if (BRTYPE != EOSTYP && BRTYPE != 0) {
            stream(&XSP, &TEXTSP, &GOTOTB);
            if (STYPE == FGOTYP || STYPE == FTOTYP) {
                NODE *fl = EXPR();
                s->go_f = fl ? strdup(fl->text ? fl->text : "") : NULL;
                FORBLK();
            }
        }
        return s;
    }

    if (gotype == FGOTYP || gotype == FTOTYP) {
        /* Failure: :F( label ) */
        NODE *lbl = EXPR();
        s->go_f = lbl ? strdup(lbl->text ? lbl->text : "") : NULL;
        FORBLK();
        if (BRTYPE != EOSTYP && BRTYPE != 0) {
            stream(&XSP, &TEXTSP, &GOTOTB);
            if (STYPE == SGOTYP || STYPE == STOTYP) {
                NODE *sl = EXPR();
                s->go_s = sl ? strdup(sl->text ? sl->text : "") : NULL;
                FORBLK();
            }
        }
        return s;
    }

    sil_error("CMPGO: unrecognized goto type %d", gotype);
    return s;
    }
}

/* =========================================================================
 * IR printer — s-expression, SIL names
 * ========================================================================= */

static void print_node(NODE *n, int depth) {
    if (!n) { printf("%*s(NULL)\n", depth*2, ""); return; }
    printf("%*s(%s", depth*2, "", stype_name(n->stype));
    if (n->text && n->text[0])
        printf(" \"%s\"", n->text);
    if (n->stype == ILITYP)
        printf(" ival=%lld", n->ival);
    if (n->stype == FLITYP)
        printf(" fval=%g", n->fval);
    if (n->nchildren == 0) {
        printf(")\n");
    } else {
        printf("\n");
        for (int i = 0; i < n->nchildren; i++)
            print_node(n->children[i], depth+1);
        printf("%*s)\n", depth*2, "");
    }
}

static void print_stmt(STMT *s, int idx) {
    printf("=== stmt %d ===\n", idx);
    if (s->is_end)      { printf("  END\n"); return; }
    if (s->label)         printf("  label:   %s\n", s->label);
    if (s->subject)     { printf("  subject:\n"); print_node(s->subject, 2); }
    if (s->pattern)     { printf("  pattern:\n"); print_node(s->pattern, 2); }
    if (s->has_eq && s->replacement)
                        { printf("  replace:\n"); print_node(s->replacement, 2); }
    if (s->go_s)          printf("  :S(%s)\n", s->go_s);
    if (s->go_f)          printf("  :F(%s)\n", s->go_f);
    if (s->go_u)          printf("  :(%s)\n",  s->go_u);
}

/* =========================================================================
 * main — read lines, call CARDTB + CMPILE per line
 * ========================================================================= */

/* NEWCRD: process card type (CARDTB STYPE) */
/* Mirrors SIL: SELBRA STYPE,(,CMTCRD,CTLCRD,CNTCRD) */

int main(int argc, char **argv) {
    init_tables();

    FILE *f = argc > 1 ? fopen(argv[1], "r") : stdin;
    if (!f) { perror(argv[1]); return 1; }

    char line[4096];
    int  lineno = 0;
    int  stmt_idx = 0;
    STMT *head = NULL, *tail = NULL;

    while (fgets(line, sizeof line, f)) {
        lineno++;
        /* strip \r\n */
        int len = strlen(line);
        while (len > 0 && (line[len-1]=='\n'||line[len-1]=='\r')) line[--len]='\0';
        if (len == 0) continue;

        /* CARDTB: determine card type */
        spec_t card = { line, len };
        spec_t tok;
        stream(&tok, &card, &CARDTB);
        int ctype = STYPE;

        if (ctype == CMTTYP || ctype == CTLTYP) continue; /* skip */
        if (ctype == CNTTYP) {
            /* continuation: append to TEXTSP of current statement */
            /* For now, treat as separate (TODO: join with previous) */
            continue;
        }

        /* NEWTYP: new statement */
        g_error = 0;
        TEXTSP.ptr = line;
        TEXTSP.len = len;
        STYPE  = 0;
        BRTYPE = 0;

        STMT *s = CMPILE();
        if (!s) continue;

        s->next = NULL;
        if (!head) head = s;
        else        tail->next = s;
        tail = s;

        if (g_error)
            fprintf(stderr, "line %d: %s\n  src: %s\n", lineno, g_errmsg, line);

        print_stmt(s, ++stmt_idx);
        if (s->is_end) break;
    }

    if (f != stdin) fclose(f);
    printf("=== %d statements ===\n", stmt_idx);
    return 0;
}
