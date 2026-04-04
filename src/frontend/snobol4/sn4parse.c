/*
 * sn4parse.c — Faithful C translation of SNOBOL4 SIL lexer/parser
 *
 * Follows v311.sil exactly:                    (v311.sil = CSNOBOL4 2.3.3 SIL source)
 *   CMPILE → compile one statement             (v311.sil:1608  "Procedure to compile statement")
 *   FORBLK / FORWRD → inter-field scanning     (v311.sil:2241  "Procedure to get to nonblank")
 *   ELEMNT → element analysis                  (v311.sil:1924  "Element analysis procedure")
 *   EXPR / EXPR1 → expression compiler         (v311.sil:2093  "Procedure to compile expression")
 *
 * SIL = SNOBOL4 Implementation Language — the macro-assembly used in CSNOBOL4.
 * Each procedure here is a direct C translation of the corresponding SIL procedure.
 * 256-byte chrs[] arrays lifted verbatim from snobol4-2.3.3/syn.c.
 * stream() lifted verbatim from snobol4-2.3.3/lib/stream.c.
 * SIL names used throughout. Tree nodes carry SIL STYPE token-type codes.
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
 *
 * A syntab (syntax table) is a finite-state scanner step:
 *   chrs[256]   — maps each input byte to an index into the actions[] array.
 *                 Index 0 means AC_CONTIN (fast-path: no action, keep scanning).
 *   actions[]   — parallel array; each entry says what to do when chrs[c] fires.
 *   put         — integer token code written to the global STYPE on a trigger.
 *   act         — AC_CONTIN / AC_STOP / AC_STOPSH / AC_ERROR / AC_GOTO.
 *   go          — pointer to next syntab when act == AC_GOTO (table chain).
 *
 * stream(sp1, sp2, tp) scans sp2 byte-by-byte through table tp, fills sp1
 * with the consumed prefix, shrinks sp2 to the remainder, sets STYPE.
 * ========================================================================= */

typedef enum { AC_CONTIN=0, AC_STOP, AC_STOPSH, AC_ERROR, AC_GOTO } action_t;
/*            consume+continue  consume+stop  stop(no consume)  error  chain→go */
typedef struct syntab syntab_t;
typedef struct { int put; action_t act; syntab_t *go; } acts_t;
struct syntab { const char *name; unsigned char chrs[256]; acts_t *actions; };

/* STYPE — "Descriptor returned by STREAM" (v311.sil:10842 "STYPE DESCR 0,FNC,0").
 * stream() writes the triggering action's put-code here.
 * Callers read STYPE after stream() returns to learn what token type was found. */
static int STYPE;

/* =========================================================================
 * stream() — verbatim from snobol4-2.3.3/lib/stream.c
 *
 * SIL calling convention (v311.sil "STREAM XSP,TEXTSP,IBLKTB,..."):
 *   sp1   = XSP   — output: spec of the consumed prefix (token text)
 *   sp2   = TEXTSP — input/output: remaining source text (shrinks as consumed)
 *   tp    = table  — the syntab to drive (IBLKTB, FRWDTB, BIOPTB, etc.)
 *
 * A "spec" (specifier) is a (ptr, len) pair — SIL's SPEC type, two words.
 * Returns ST_STOP (AC_STOP hit), ST_EOS (input exhausted), ST_ERROR (AC_ERROR).
 * Sets STYPE to the put-code of the triggering action.
 * ========================================================================= */

typedef enum { ST_STOP, ST_EOS, ST_ERROR } stream_ret_t;

typedef struct { const char *ptr; int len; } spec_t; /* SIL SPEC: (pointer, length) pair */

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
 * SIL token type constants — from snobol4-2.3.3/equ.h
 *
 * These integers are "put" codes written to STYPE by stream().
 * The same integer value can mean different things in different table contexts;
 * the comments below give the meaning in each table where the code appears.
 * All values and names are verbatim from equ.h.
 * ========================================================================= */

/* ELEMTB / VARTB / INTGTB / FLITB result codes — element (token) type */
#define QLITYP  1   /* quoted literal: 'text' or "text"         (equ.h) */
#define ILITYP  2   /* integer literal: digits                  (equ.h) */
#define VARTYP  3   /* variable name: [A-Za-z][A-Za-z0-9]*     (equ.h) */
#define NSTTYP  4   /* nested/parenthesized expression: (       (equ.h) */
#define FNCTYP  5   /* function call: IDENT(                    (equ.h) */
#define FLITYP  6   /* floating-point literal: digits.digits    (equ.h) */
#define ARYTYP  7   /* array/table subscript: IDENT<            (equ.h) */

/* FRWDTB / FORBLK result codes — inter-field delimiter type */
#define NBTYP   1   /* non-blank: a real token starts here      (equ.h) — "non-blank" */
#define EQTYP   4   /* '=' equals sign: replacement separator   (equ.h) */
#define CLNTYP  5   /* ':' colon: goto field separator          (equ.h) */
#define EOSTYP  6   /* end of statement (';' or line end)       (equ.h) */

/* ELEMTB/VARTB inner delimiters */
#define RPTYP   3   /* ')' right parenthesis: ends arg list     (equ.h) */
#define CMATYP  2   /* ',' comma: separates function arguments  (equ.h) */
#define RBTYP   7   /* ']' or '>' right bracket: ends subscript (equ.h) */

/* GOTOTB / GOTSTB / GOTFTB result codes — goto field sub-type */
#define SGOTYP  2   /* :S(  success goto with label in parens   (equ.h) */
#define FGOTYP  3   /* :F(  failure goto with label in parens   (equ.h) */
#define UGOTYP  1   /* :(   unconditional goto                  (equ.h) */
#define STOTYP  5   /* :S<  success direct goto                 (equ.h) */
#define FTOTYP  6   /* :F<  failure direct goto                 (equ.h) */
#define UTOTYP  4   /* :<   unconditional direct goto           (equ.h) */

/* CARDTB result codes — source card (line) type */
#define CMTTYP  2   /* comment card: first char is '*'          (equ.h) */
#define CTLTYP  3   /* control card: first char is '-'          (equ.h) */
#define CNTTYP  4   /* continuation card: first char is '+'     (equ.h) */
#define NEWTYP  1   /* new statement card: any other first char  (equ.h) */

/* Binary operator function descriptor codes — put-codes from BIOPTB.
 * Each maps to a SIL DESCR in v311.sil data section (line 11629+).
 * "FN" suffix = "FuNction descriptor" in SIL terminology. */
#define ADDFN   201  /* X + Y  addition           (v311.sil:11629 "ADD,0,2") */
#define SUBFN   202  /* X - Y  subtraction         (v311.sil:11691 "SUB,0,2") */
#define MPYFN   203  /* X * Y  multiplication      (v311.sil:11682 "MPY,0,2") */
#define DIVFN   204  /* X / Y  division             (v311.sil:11673 "DIV,0,2") */
#define EXPFN   205  /* X ** Y exponentiation       (v311.sil:11679 "EXPOP,0,2") */
#define ORFN    206  /* X | Y  alternation (pattern)(v311.sil:11688 "OR,0,2") */
#define NAMFN   207  /* X . Y  naming / cond.assign (v311.sil:11685 "NAM,0,2") */
#define DOLFN   208  /* X $ Y  immediate naming     (v311.sil:11676 "DOL,0,2") */
#define BIATFN  209  /* X @ Y  user-definable       (v311.sil:11635 "UNDF") */
#define BIPDFN  210  /* X # Y  user-definable       (v311.sil:11641 "UNDF") */
#define BIPRFN  211  /* X % Y  user-definable       (v311.sil:11644 "UNDF") */
#define BIAMFN  212  /* X & Y  user-definable       (v311.sil:11632 "UNDF") */
#define BINGFN  213  /* X ~ Y  user-definable       (v311.sil:11638 "UNDF") */
#define BIQSFN  214  /* X ? Y  user-definable       (v311.sil:11659 "UNDF") */
#define BISNFN  215  /* X ? Y  SPITBOL scan-replace (SPITBOL extension) */
#define BIEQFN  216  /* X = Y  SPITBOL assignment   (SPITBOL extension) */

/* Unary prefix operator function descriptor codes — put-codes from UNOPTB.
 * Each maps to a SIL DESCR in v311.sil data section (line 11694+). */
#define PLSFN   301  /* +X  unary plus              (v311.sil:11712 "PLS,0,1") */
#define MNSFN   302  /* -X  unary minus             (v311.sil:11706 "MNS,0,1") */
#define DOTFN   303  /* .X  name-of (lvalue ref)    (v311.sil:11700 "NAME,0,1") */
#define INDFN   304  /* $X  indirect reference      (v311.sil:11702 "IND,0,1") */
#define STRFN   305  /* *X  unevaluated expression  (v311.sil:11720 "STR,0,1") */
#define SLHFN   306  /* /X  user-definable          (v311.sil:11718 "UNDF") */
#define PRFN    307  /* %X  user-definable          (v311.sil:11714 "UNDF") */
#define ATFN    308  /* @X  scanner cursor position (v311.sil:11696 "ATOP,0,1") */
#define PDFN    309  /* #X  user-definable          (v311.sil:11710 "UNDF") */
#define KEYFN   310  /* &X  keyword reference       (v311.sil:11704 "KEYWRD,0,1") */
#define NEGFN   311  /* ~X  not/negation            (v311.sil:11708 "NEG,0,1") */
#define BARFN   312  /* |X  user-definable          (v311.sil:11698 "UNDF") */
#define QUESFN  313  /* ?X  interrogation/test       (v311.sil:11716 "QUES,0,1") */
#define AROWFN  314  /* ^X  user-definable          (v311.sil:11694 "UNDF") */

/* =========================================================================
 * Syntax tables — chrs[] verbatim from snobol4-2.3.3/syn.c
 *
 * Each table implements one step of the SIL STREAM instruction.
 * The 256-byte chrs[] array maps ASCII byte values to 1-based action indices.
 * chrs[c] == 0  → AC_CONTIN fast path (no entry in actions[], just keep going).
 * chrs[c] == N  → actions[N-1] is the entry to fire.
 *
 * Table naming convention (SIL):
 *   *TB suffix = "Table"
 *   LBL = label field,  CARD = card type,  IBLK = inter-field blank,
 *   FRWD = forward scan, ELEM = element, VAR = variable, INTG = integer,
 *   FLIT = float literal, SQL/DQL = single/double quote literal,
 *   UNOP = unary operator, BIOP = binary operator,
 *   START = disambiguate * vs **, TBLK = trailing blank after operator,
 *   GOTO = goto field, GOTS/GOTF = S-goto / F-goto.
 * ========================================================================= */

/* ---- LBLTB: label field scanner (v311.sil:1613 "STREAM XSP,TEXTSP,LBLTB")
 * Accepts alphanumeric chars (columns 1+) as label text.
 * Stops on blank or ';' (end-of-statement).  Errors on any other char. ---- */
static acts_t LBLTB_actions[3];
/* LBLXTB: continuation of label scan — same as LBLTB but accepts digits too.
 * SIL transitions LBLTB→LBLXTB after the first alphanumeric char. Here merged. */
static acts_t LBLXTB_actions[1];
syntab_t LBLXTB = { "LBLXTB", {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
}, LBLXTB_actions };

syntab_t LBLTB = { "LBLTB", {
     3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 2, 3, 3, 3, 3,
     3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1,  /* 0x5F '_' → 1 (alphanumeric, SNOBOL4+) */
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

/* ---- CARDTB: card (line) type scanner (v311.sil:1033 "STREAM XSP,TEXTSP,CARDTB")
 * First char of each source line determines card type.  All stops are STOPSH
 * (char is NOT consumed — it stays as the start of the next field).
 * "Card" = punch-card terminology from SNOBOL4's batch origins. ---- */
static acts_t CARDTB_actions[4];
syntab_t CARDTB = { "CARDTB", {
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  1,  4,  1,  4,  4,  4,  4,  4,  4,  1,  3,  4,  2,  3,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  1,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  1,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4
}, CARDTB_actions };

/* ---- FRWDTB: forward scan — skips transparent chars (space=0x20, tab=0x09
 * have chrs[]=0 → AC_CONTIN fast-path) and stops on the next field delimiter.
 * (v311.sil:2215 "STREAM XSP,TEXTSP,FRWDTB — Break for next nonblank") ---- */
/* ---- IBLKTB: inter-field blank scanner — expects a leading space/tab,
 * chains via AC_GOTO to FRWDTB on the first non-blank, fires EOSTYP on ';'.
 * Returns ST_ERROR when there is NO leading blank (BINOP's no-blank / BINOP1 path).
 * (v311.sil:2242 "STREAM XSP,TEXTSP,IBLKTB — Break out nonblank from blank") ---- */
static acts_t FRWDTB_actions[7];
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

static acts_t IBLKTB_actions[3];
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

/* ---- ELEMTB: element type dispatch (v311.sil:1926 "STREAM XSP,TEXTSP,ELEMTB")
 * Classifies the first character of a token and chains to the appropriate
 * sub-scanner.  SIL then does "SELBRA STYPE,(ELEILT,ELEVBL,ELENST,ELEFNC,ELEFLT,ELEARY)"
 * to branch on the resulting STYPE. ---- */
static acts_t ELEMTB_actions[6];
/* Forward declarations for goto targets wired in init_tables() */
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

/* ---- VARTB: variable / function-name scanner (v311.sil:ELEVBL branch)
 * Entered after ELEMTB fires AC_GOTO for a letter.  Accumulates alphanumeric
 * chars (chrs[]=0 fast-path).  Stops on the first non-identifier char and
 * classifies it: plain variable, function call IDENT(, or array ref IDENT<. ---- */
static acts_t VARTB_actions[4];
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

/* ---- INTGTB: integer digit accumulator (v311.sil:ELEILT branch)
 * Entered after ELEMTB fires AC_GOTO for a digit.  Continues on digits,
 * stops on terminators, chains to FLITB on '.', chains to EXPTB on 'e'/'E'. ---- */
syntab_t FLITB;  /* forward — INTGTB references FLITB before it is defined */
static acts_t INTGTB_actions[4];
syntab_t EXPTB, EXPBTB; /* forward declarations — used by INTGTB/FLITB AC_GOTO */
syntab_t INTGTB = { "INTGTB", {
     4,  4,  4,  4,  4,  4,  4,  4,  4,  1,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     1,  4,  4,  4,  4,  4,  4,  4,  4,  1,  4,  4,  1,  4,  2,  4,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4,  1,  4,  4,  1,  4,
     4,  4,  4,  4,  4,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  1,  4,  4,
     4,  4,  4,  4,  4,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4
}, INTGTB_actions };

/* ---- FLITB: floating-point fraction digit accumulator
 * Entered after INTGTB sees '.'.  Continues on digits, stops on terminators,
 * chains to EXPTB on 'e'/'E' (scientific notation exponent). ---- */
static acts_t FLITB_actions[3];
syntab_t FLITB = { "FLITB", {
     3,  3,  3,  3,  3,  3,  3,  3,  3,  1,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     1,  3,  3,  3,  3,  3,  3,  3,  3,  1,  3,  3,  1,  3,  3,  3,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  1,  3,  3,  1,  3,
     3,  3,  3,  3,  3,  2,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  1,  3,  3,
     3,  3,  3,  3,  3,  2,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3
}, FLITB_actions };

/* ---- EXPTB / EXPBTB: exponent sign and digit scanners
 * EXPTB: entered after 'e'/'E' in a float; accepts optional '+'/'-' then chains to EXPBTB.
 * EXPBTB: accumulates exponent digits, stops on any token terminator. ---- */
static acts_t EXPBTB_actions[2];
syntab_t EXPBTB = { "EXPBTB", {
     2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     1,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  2,  1,  2,  2,  2,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  1,  2,  2,  1,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2
}, EXPBTB_actions };
static acts_t EXPTB_actions[2];
syntab_t EXPTB = { "EXPTB", {
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  1,  2,  2,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2
}, EXPTB_actions };

/* ---- SQLITB / DQLITB: string literal body scanners
 * SQLITB: scans body of a single-quoted string '...'; stops on matching '.
 * DQLITB: scans body of a double-quoted string "..."; stops on matching ".
 * Both accept ALL other bytes including spaces and operators (AC_CONTIN). ---- */
static acts_t SQLITB_actions[1];
syntab_t SQLITB = { "SQLITB", {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
}, SQLITB_actions };
static acts_t DQLITB_actions[1];
syntab_t DQLITB = { "DQLITB", {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
}, DQLITB_actions };

/* ---- UNOPTB: unary prefix operator scanner (v311.sil:2510 "STREAM XSP,TEXTSP,UNOPTB")
 * Entered by ELEMNT's UNOP loop before the element itself.
 * Each operator char fires AC_STOPSH (does NOT consume the char — leaves it for re-use)
 * and puts its operator code into STYPE.  All are AC_STOPSH because the operator
 * IS the single-char token; the char must not be consumed into XSP here. ---- */
/* UOP_* aliases match the *FN defines above — same numeric values, kept separate
 * only for readability inside the UNOPTB_actions table. */
#define UOP_PLS  301  /* = PLSFN:  +X unary plus              */
#define UOP_MNS  302  /* = MNSFN:  -X unary minus             */
#define UOP_DOT  303  /* = DOTFN:  .X name-of (lvalue)        */
#define UOP_IND  304  /* = INDFN:  $X indirect reference      */
#define UOP_STR  305  /* = STRFN:  *X unevaluated expression  */
#define UOP_SLH  306  /* = SLHFN:  /X user-definable          */
#define UOP_PCT  307  /* = PRFN:   %X user-definable          */
#define UOP_AT   308  /* = ATFN:   @X scanner cursor position */
#define UOP_PD   309  /* = PDFN:   #X user-definable          */
#define UOP_KEY  310  /* = KEYFN:  &X keyword reference       */
#define UOP_NEG  311  /* = NEGFN:  ~X not/negation            */
#define UOP_BAR  312  /* = BARFN:  |X user-definable          */
#define UOP_QUE  313  /* = QUESFN: ?X interrogation/test      */
#define UOP_ARW  314  /* = AROWFN: ^X user-definable          */

static acts_t UNOPTB_actions[15];
syntab_t UNOPTB = { "UNOPTB", {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 12, 15,  9,  4,  7, 10, 15, 15, 15,  5,  1, 15,  2,  3,  6,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 13,
     8, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 11, 15, 14, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 12, 15, 11, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
}, UNOPTB_actions };

/* BIOPTB and TBLKTB are mutually referential — forward-declare both. */
extern syntab_t TBLKTB, STARTB;

/* ---- BIOPTB: binary operator scanner — SNOBOL4 mode
 * (v311.sil:1567 "STREAM XSP,TEXTSP,BIOPTB,BINCON")
 * Called by BINOP() AFTER the leading blank has been consumed by IBLKTB.
 * Each entry: set STYPE to the operator function code, then AC_GOTO to TBLKTB
 * (which must consume the mandatory trailing blank that follows every binary op).
 * Special case: '*' goes to STARTB instead of TBLKTB to handle '**' vs '*'.  ---- */
static acts_t BIOPTB_actions[15];

/* STARTB: disambiguate '*' (multiply) from '**' (exponentiation)
 * (v311.sil: BIOPTB action[4] goes here after consuming the first '*')
 * Peeks at the next char: if it is another '*' consume it and set EXPFN;
 * if it is a blank/tab consume it and keep STYPE=MPYFN (already set). ---- */
static acts_t STARTB_actions[3];
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

/* TBLKTB: trailing blank consumer — every binary operator must be followed by a blank.
 * (v311.sil: every BIOPTB action goes to TBLKTB after setting STYPE)
 * Consumes exactly one space or tab (the mandatory post-operator blank), then stops.  ---- */
static acts_t TBLKTB_actions[2];
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

/* ---- GOTOTB: goto field top-level scanner (v311.sil:1730 "STREAM XSP,TEXTSP,GOTOTB")
 * Entered after ':' is detected by FORBLK.  Classifies the goto type:
 *   :( or :< → unconditional;  :S → success (chains to GOTSTB);  :F → failure (chains to GOTFTB).
 * GOTSTB: sub-scanner after 'S' — expects '(' (label in parens) or '<' (direct address).
 * GOTFTB: sub-scanner after 'F' — same but for failure branch. ---- */
static acts_t GOTSTB_actions[3];
syntab_t GOTSTB = { "GOTSTB", {
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  1,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3
}, GOTSTB_actions };
static acts_t GOTFTB_actions[3];
syntab_t GOTFTB = { "GOTFTB", {
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  1,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3
}, GOTFTB_actions };
static acts_t GOTOTB_actions[5];
syntab_t GOTOTB = { "GOTOTB", {
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  3,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  4,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  2,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  1,  5,  5,  5,  5,  5,  5,  5,  4,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  2,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  1,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5
}, GOTOTB_actions };

/* =========================================================================
 * Stub declarations for tables in syn_init.h not used by sn4parse.
 * SBIPTB/BBIOPTB/BSBIPTB = SPITBOL/BLOCKS operator variants.
 * EOSTB/NBLKTB/NUMBTB/NUMCTB/SPANTB/BRKTB/VARATB/VARBTB = unused here.
 * All declared so init_tables() (verbatim syn_init.h) compiles cleanly.
 * ========================================================================= */
#define BIBDFN  BIPDFN   /* BLOCKS binary dot — alias */
#define BIBRFN  BIPRFN   /* BLOCKS binary % — alias */
#define LPTYP   8        /* left-paren type (VARBTB) */
#define DIMTYP  9        /* dimension separator (NUMBTB/NUMCTB) */
static acts_t SBIPTB_actions[16], BBIOPTB_actions[15], BSBIPTB_actions[16];
static acts_t EOSTB_actions[1],   NBLKTB_actions[2];
static acts_t NUMBTB_actions[4],  NUMCTB_actions[3];
static acts_t SPANTB_actions[3],  BRKTB_actions[3];
static acts_t VARATB_actions[4],  VARBTB_actions[4];
static syntab_t SBIPTB, BBIOPTB, BSBIPTB;
static syntab_t EOSTB;
syntab_t NBLKTB = { "NBLKTB", {
     2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     1,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  2,  1,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  2,  1,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
}, NBLKTB_actions };
static syntab_t NUMBTB, NUMCTB;
static syntab_t SPANTB, BRKTB;
static syntab_t VARATB, VARBTB;

/* Wire up forward-declared goto targets in action tables.
 * These cannot be static initialisers because C does not allow forward references
 * to syntab_t objects in struct literals.  Called once from main() before parsing. */
static void init_tables(void) {
    /* init_tables: verbatim from snobol4-2.3.3/syn_init.h */
    BIOPTB_actions[0].put = ADDFN;
    BIOPTB_actions[0].act = AC_GOTO;
    BIOPTB_actions[0].go  = &TBLKTB;
    BIOPTB_actions[1].put = SUBFN;
    BIOPTB_actions[1].act = AC_GOTO;
    BIOPTB_actions[1].go  = &TBLKTB;
    BIOPTB_actions[2].put = NAMFN;
    BIOPTB_actions[2].act = AC_GOTO;
    BIOPTB_actions[2].go  = &TBLKTB;
    BIOPTB_actions[3].put = DOLFN;
    BIOPTB_actions[3].act = AC_GOTO;
    BIOPTB_actions[3].go  = &TBLKTB;
    BIOPTB_actions[4].put = MPYFN;
    BIOPTB_actions[4].act = AC_GOTO;
    BIOPTB_actions[4].go  = &STARTB;
    BIOPTB_actions[5].put = DIVFN;
    BIOPTB_actions[5].act = AC_GOTO;
    BIOPTB_actions[5].go  = &TBLKTB;
    BIOPTB_actions[6].put = BIATFN;
    BIOPTB_actions[6].act = AC_GOTO;
    BIOPTB_actions[6].go  = &TBLKTB;
    BIOPTB_actions[7].put = BIPDFN;
    BIOPTB_actions[7].act = AC_GOTO;
    BIOPTB_actions[7].go  = &TBLKTB;
    BIOPTB_actions[8].put = BIPRFN;
    BIOPTB_actions[8].act = AC_GOTO;
    BIOPTB_actions[8].go  = &TBLKTB;
    BIOPTB_actions[9].put = EXPFN;
    BIOPTB_actions[9].act = AC_GOTO;
    BIOPTB_actions[9].go  = &TBLKTB;
    BIOPTB_actions[10].put = ORFN;
    BIOPTB_actions[10].act = AC_GOTO;
    BIOPTB_actions[10].go  = &TBLKTB;
    BIOPTB_actions[11].put = BIAMFN;
    BIOPTB_actions[11].act = AC_GOTO;
    BIOPTB_actions[11].go  = &TBLKTB;
    BIOPTB_actions[12].put = BINGFN;
    BIOPTB_actions[12].act = AC_GOTO;
    BIOPTB_actions[12].go  = &TBLKTB;
    BIOPTB_actions[13].put = BIQSFN;
    BIOPTB_actions[13].act = AC_GOTO;
    BIOPTB_actions[13].go  = &TBLKTB;
    BIOPTB_actions[14].act = AC_ERROR;
    SBIPTB_actions[0].put = ADDFN;
    SBIPTB_actions[0].act = AC_GOTO;
    SBIPTB_actions[0].go  = &TBLKTB;
    SBIPTB_actions[1].put = SUBFN;
    SBIPTB_actions[1].act = AC_GOTO;
    SBIPTB_actions[1].go  = &TBLKTB;
    SBIPTB_actions[2].put = NAMFN;
    SBIPTB_actions[2].act = AC_GOTO;
    SBIPTB_actions[2].go  = &TBLKTB;
    SBIPTB_actions[3].put = DOLFN;
    SBIPTB_actions[3].act = AC_GOTO;
    SBIPTB_actions[3].go  = &TBLKTB;
    SBIPTB_actions[4].put = MPYFN;
    SBIPTB_actions[4].act = AC_GOTO;
    SBIPTB_actions[4].go  = &STARTB;
    SBIPTB_actions[5].put = DIVFN;
    SBIPTB_actions[5].act = AC_GOTO;
    SBIPTB_actions[5].go  = &TBLKTB;
    SBIPTB_actions[6].put = BIATFN;
    SBIPTB_actions[6].act = AC_GOTO;
    SBIPTB_actions[6].go  = &TBLKTB;
    SBIPTB_actions[7].put = BIPDFN;
    SBIPTB_actions[7].act = AC_GOTO;
    SBIPTB_actions[7].go  = &TBLKTB;
    SBIPTB_actions[8].put = BIPRFN;
    SBIPTB_actions[8].act = AC_GOTO;
    SBIPTB_actions[8].go  = &TBLKTB;
    SBIPTB_actions[9].put = EXPFN;
    SBIPTB_actions[9].act = AC_GOTO;
    SBIPTB_actions[9].go  = &TBLKTB;
    SBIPTB_actions[10].put = ORFN;
    SBIPTB_actions[10].act = AC_GOTO;
    SBIPTB_actions[10].go  = &TBLKTB;
    SBIPTB_actions[11].put = BIAMFN;
    SBIPTB_actions[11].act = AC_GOTO;
    SBIPTB_actions[11].go  = &TBLKTB;
    SBIPTB_actions[12].put = BINGFN;
    SBIPTB_actions[12].act = AC_GOTO;
    SBIPTB_actions[12].go  = &TBLKTB;
    SBIPTB_actions[13].put = BISNFN;
    SBIPTB_actions[13].act = AC_GOTO;
    SBIPTB_actions[13].go  = &TBLKTB;
    SBIPTB_actions[14].put = BIEQFN;
    SBIPTB_actions[14].act = AC_GOTO;
    SBIPTB_actions[14].go  = &TBLKTB;
    SBIPTB_actions[15].act = AC_ERROR;
    BBIOPTB_actions[0].put = ADDFN;
    BBIOPTB_actions[0].act = AC_GOTO;
    BBIOPTB_actions[0].go  = &TBLKTB;
    BBIOPTB_actions[1].put = SUBFN;
    BBIOPTB_actions[1].act = AC_GOTO;
    BBIOPTB_actions[1].go  = &TBLKTB;
    BBIOPTB_actions[2].put = NAMFN;
    BBIOPTB_actions[2].act = AC_GOTO;
    BBIOPTB_actions[2].go  = &TBLKTB;
    BBIOPTB_actions[3].put = DOLFN;
    BBIOPTB_actions[3].act = AC_GOTO;
    BBIOPTB_actions[3].go  = &TBLKTB;
    BBIOPTB_actions[4].put = MPYFN;
    BBIOPTB_actions[4].act = AC_GOTO;
    BBIOPTB_actions[4].go  = &STARTB;
    BBIOPTB_actions[5].put = DIVFN;
    BBIOPTB_actions[5].act = AC_GOTO;
    BBIOPTB_actions[5].go  = &TBLKTB;
    BBIOPTB_actions[6].put = BIATFN;
    BBIOPTB_actions[6].act = AC_GOTO;
    BBIOPTB_actions[6].go  = &TBLKTB;
    BBIOPTB_actions[7].put = BIBDFN;
    BBIOPTB_actions[7].act = AC_GOTO;
    BBIOPTB_actions[7].go  = &TBLKTB;
    BBIOPTB_actions[8].put = BIBRFN;
    BBIOPTB_actions[8].act = AC_GOTO;
    BBIOPTB_actions[8].go  = &TBLKTB;
    BBIOPTB_actions[9].put = EXPFN;
    BBIOPTB_actions[9].act = AC_GOTO;
    BBIOPTB_actions[9].go  = &TBLKTB;
    BBIOPTB_actions[10].put = ORFN;
    BBIOPTB_actions[10].act = AC_GOTO;
    BBIOPTB_actions[10].go  = &TBLKTB;
    BBIOPTB_actions[11].put = BIAMFN;
    BBIOPTB_actions[11].act = AC_GOTO;
    BBIOPTB_actions[11].go  = &TBLKTB;
    BBIOPTB_actions[12].put = BINGFN;
    BBIOPTB_actions[12].act = AC_GOTO;
    BBIOPTB_actions[12].go  = &TBLKTB;
    BBIOPTB_actions[13].put = BIQSFN;
    BBIOPTB_actions[13].act = AC_GOTO;
    BBIOPTB_actions[13].go  = &TBLKTB;
    BBIOPTB_actions[14].act = AC_ERROR;
    BSBIPTB_actions[0].put = ADDFN;
    BSBIPTB_actions[0].act = AC_GOTO;
    BSBIPTB_actions[0].go  = &TBLKTB;
    BSBIPTB_actions[1].put = SUBFN;
    BSBIPTB_actions[1].act = AC_GOTO;
    BSBIPTB_actions[1].go  = &TBLKTB;
    BSBIPTB_actions[2].put = NAMFN;
    BSBIPTB_actions[2].act = AC_GOTO;
    BSBIPTB_actions[2].go  = &TBLKTB;
    BSBIPTB_actions[3].put = DOLFN;
    BSBIPTB_actions[3].act = AC_GOTO;
    BSBIPTB_actions[3].go  = &TBLKTB;
    BSBIPTB_actions[4].put = MPYFN;
    BSBIPTB_actions[4].act = AC_GOTO;
    BSBIPTB_actions[4].go  = &STARTB;
    BSBIPTB_actions[5].put = DIVFN;
    BSBIPTB_actions[5].act = AC_GOTO;
    BSBIPTB_actions[5].go  = &TBLKTB;
    BSBIPTB_actions[6].put = BIATFN;
    BSBIPTB_actions[6].act = AC_GOTO;
    BSBIPTB_actions[6].go  = &TBLKTB;
    BSBIPTB_actions[7].put = BIBDFN;
    BSBIPTB_actions[7].act = AC_GOTO;
    BSBIPTB_actions[7].go  = &TBLKTB;
    BSBIPTB_actions[8].put = BIBRFN;
    BSBIPTB_actions[8].act = AC_GOTO;
    BSBIPTB_actions[8].go  = &TBLKTB;
    BSBIPTB_actions[9].put = EXPFN;
    BSBIPTB_actions[9].act = AC_GOTO;
    BSBIPTB_actions[9].go  = &TBLKTB;
    BSBIPTB_actions[10].put = ORFN;
    BSBIPTB_actions[10].act = AC_GOTO;
    BSBIPTB_actions[10].go  = &TBLKTB;
    BSBIPTB_actions[11].put = BIAMFN;
    BSBIPTB_actions[11].act = AC_GOTO;
    BSBIPTB_actions[11].go  = &TBLKTB;
    BSBIPTB_actions[12].put = BINGFN;
    BSBIPTB_actions[12].act = AC_GOTO;
    BSBIPTB_actions[12].go  = &TBLKTB;
    BSBIPTB_actions[13].put = BISNFN;
    BSBIPTB_actions[13].act = AC_GOTO;
    BSBIPTB_actions[13].go  = &TBLKTB;
    BSBIPTB_actions[14].put = BIEQFN;
    BSBIPTB_actions[14].act = AC_GOTO;
    BSBIPTB_actions[14].go  = &TBLKTB;
    BSBIPTB_actions[15].act = AC_ERROR;
    CARDTB_actions[0].put = CMTTYP;
    CARDTB_actions[0].act = AC_STOPSH;
    CARDTB_actions[1].put = CTLTYP;
    CARDTB_actions[1].act = AC_STOPSH;
    CARDTB_actions[2].put = CNTTYP;
    CARDTB_actions[2].act = AC_STOPSH;
    CARDTB_actions[3].put = NEWTYP;
    CARDTB_actions[3].act = AC_STOPSH;
    DQLITB_actions[0].act = AC_STOP;
    ELEMTB_actions[0].put = ILITYP;
    ELEMTB_actions[0].act = AC_GOTO;
    ELEMTB_actions[0].go  = &INTGTB;
    ELEMTB_actions[1].put = VARTYP;
    ELEMTB_actions[1].act = AC_GOTO;
    ELEMTB_actions[1].go  = &VARTB;
    ELEMTB_actions[2].put = QLITYP;
    ELEMTB_actions[2].act = AC_GOTO;
    ELEMTB_actions[2].go  = &SQLITB;
    ELEMTB_actions[3].put = QLITYP;
    ELEMTB_actions[3].act = AC_GOTO;
    ELEMTB_actions[3].go  = &DQLITB;
    ELEMTB_actions[4].put = NSTTYP;
    ELEMTB_actions[4].act = AC_STOP;
    ELEMTB_actions[5].act = AC_ERROR;
    EOSTB_actions[0].act = AC_STOP;
    EXPTB_actions[0].act = AC_GOTO;
    EXPTB_actions[0].go  = &EXPBTB;
    EXPTB_actions[1].act = AC_ERROR;
    EXPBTB_actions[0].act = AC_STOPSH;
    EXPBTB_actions[1].act = AC_ERROR;
    FLITB_actions[0].act = AC_STOPSH;
    FLITB_actions[1].act = AC_GOTO;
    FLITB_actions[1].go  = &EXPTB;
    FLITB_actions[2].act = AC_ERROR;
    FRWDTB_actions[0].put = EQTYP;
    FRWDTB_actions[0].act = AC_STOP;
    FRWDTB_actions[1].put = RPTYP;
    FRWDTB_actions[1].act = AC_STOP;
    FRWDTB_actions[2].put = RBTYP;
    FRWDTB_actions[2].act = AC_STOP;
    FRWDTB_actions[3].put = CMATYP;
    FRWDTB_actions[3].act = AC_STOP;
    FRWDTB_actions[4].put = CLNTYP;
    FRWDTB_actions[4].act = AC_STOP;
    FRWDTB_actions[5].put = EOSTYP;
    FRWDTB_actions[5].act = AC_STOP;
    FRWDTB_actions[6].put = NBTYP;
    FRWDTB_actions[6].act = AC_STOPSH;
    GOTFTB_actions[0].put = FGOTYP;
    GOTFTB_actions[0].act = AC_STOP;
    GOTFTB_actions[1].put = FTOTYP;
    GOTFTB_actions[1].act = AC_STOP;
    GOTFTB_actions[2].act = AC_ERROR;
    GOTOTB_actions[0].act = AC_GOTO;
    GOTOTB_actions[0].go  = &GOTSTB;
    GOTOTB_actions[1].act = AC_GOTO;
    GOTOTB_actions[1].go  = &GOTFTB;
    GOTOTB_actions[2].put = UGOTYP;
    GOTOTB_actions[2].act = AC_STOP;
    GOTOTB_actions[3].put = UTOTYP;
    GOTOTB_actions[3].act = AC_STOP;
    GOTOTB_actions[4].act = AC_ERROR;
    GOTSTB_actions[0].put = SGOTYP;
    GOTSTB_actions[0].act = AC_STOP;
    GOTSTB_actions[1].put = STOTYP;
    GOTSTB_actions[1].act = AC_STOP;
    GOTSTB_actions[2].act = AC_ERROR;
    IBLKTB_actions[0].act = AC_GOTO;
    IBLKTB_actions[0].go  = &FRWDTB;
    IBLKTB_actions[1].put = EOSTYP;
    IBLKTB_actions[1].act = AC_STOP;
    IBLKTB_actions[2].act = AC_ERROR;
    INTGTB_actions[0].put = ILITYP;
    INTGTB_actions[0].act = AC_STOPSH;
    INTGTB_actions[1].put = FLITYP;
    INTGTB_actions[1].act = AC_GOTO;
    INTGTB_actions[1].go  = &FLITB;
    INTGTB_actions[2].put = FLITYP;
    INTGTB_actions[2].act = AC_GOTO;
    INTGTB_actions[2].go  = &EXPTB;
    INTGTB_actions[3].act = AC_ERROR;
    LBLTB_actions[0].act = AC_GOTO;
    LBLTB_actions[0].go  = &LBLXTB;
    LBLTB_actions[1].act = AC_STOPSH;
    LBLTB_actions[2].act = AC_ERROR;
    LBLXTB_actions[0].act = AC_STOPSH;
    NBLKTB_actions[0].act = AC_ERROR;
    NBLKTB_actions[1].act = AC_STOPSH;
    NUMBTB_actions[0].act = AC_GOTO;
    NUMBTB_actions[0].go  = &NUMCTB;
    NUMBTB_actions[1].put = CMATYP;
    NUMBTB_actions[1].act = AC_STOPSH;
    NUMBTB_actions[2].put = DIMTYP;
    NUMBTB_actions[2].act = AC_STOPSH;
    NUMBTB_actions[3].act = AC_ERROR;
    NUMCTB_actions[0].put = CMATYP;
    NUMCTB_actions[0].act = AC_STOPSH;
    NUMCTB_actions[1].put = DIMTYP;
    NUMCTB_actions[1].act = AC_STOPSH;
    NUMCTB_actions[2].act = AC_ERROR;
    SPANTB_actions[0].act = AC_STOP;
    SPANTB_actions[1].act = AC_STOPSH;
    SPANTB_actions[2].act = AC_ERROR;
    BRKTB_actions[0].act = AC_STOP;
    BRKTB_actions[1].act = AC_STOPSH;
    BRKTB_actions[2].act = AC_ERROR;
    SQLITB_actions[0].act = AC_STOP;
    STARTB_actions[0].act = AC_STOP;
    STARTB_actions[1].put = EXPFN;
    STARTB_actions[1].act = AC_GOTO;
    STARTB_actions[1].go  = &TBLKTB;
    STARTB_actions[2].act = AC_ERROR;
    TBLKTB_actions[0].act = AC_STOP;
    TBLKTB_actions[1].act = AC_ERROR;
    UNOPTB_actions[0].put = PLSFN;
    UNOPTB_actions[0].act = AC_GOTO;
    UNOPTB_actions[0].go  = &NBLKTB;
    UNOPTB_actions[1].put = MNSFN;
    UNOPTB_actions[1].act = AC_GOTO;
    UNOPTB_actions[1].go  = &NBLKTB;
    UNOPTB_actions[2].put = DOTFN;
    UNOPTB_actions[2].act = AC_GOTO;
    UNOPTB_actions[2].go  = &NBLKTB;
    UNOPTB_actions[3].put = INDFN;
    UNOPTB_actions[3].act = AC_GOTO;
    UNOPTB_actions[3].go  = &NBLKTB;
    UNOPTB_actions[4].put = STRFN;
    UNOPTB_actions[4].act = AC_GOTO;
    UNOPTB_actions[4].go  = &NBLKTB;
    UNOPTB_actions[5].put = SLHFN;
    UNOPTB_actions[5].act = AC_GOTO;
    UNOPTB_actions[5].go  = &NBLKTB;
    UNOPTB_actions[6].put = PRFN;
    UNOPTB_actions[6].act = AC_GOTO;
    UNOPTB_actions[6].go  = &NBLKTB;
    UNOPTB_actions[7].put = ATFN;
    UNOPTB_actions[7].act = AC_GOTO;
    UNOPTB_actions[7].go  = &NBLKTB;
    UNOPTB_actions[8].put = PDFN;
    UNOPTB_actions[8].act = AC_GOTO;
    UNOPTB_actions[8].go  = &NBLKTB;
    UNOPTB_actions[9].put = KEYFN;
    UNOPTB_actions[9].act = AC_GOTO;
    UNOPTB_actions[9].go  = &NBLKTB;
    UNOPTB_actions[10].put = NEGFN;
    UNOPTB_actions[10].act = AC_GOTO;
    UNOPTB_actions[10].go  = &NBLKTB;
    UNOPTB_actions[11].put = BARFN;
    UNOPTB_actions[11].act = AC_GOTO;
    UNOPTB_actions[11].go  = &NBLKTB;
    UNOPTB_actions[12].put = QUESFN;
    UNOPTB_actions[12].act = AC_GOTO;
    UNOPTB_actions[12].go  = &NBLKTB;
    UNOPTB_actions[13].put = AROWFN;
    UNOPTB_actions[13].act = AC_GOTO;
    UNOPTB_actions[13].go  = &NBLKTB;
    UNOPTB_actions[14].act = AC_ERROR;
    VARATB_actions[0].act = AC_GOTO;
    VARATB_actions[0].go  = &VARBTB;
    VARATB_actions[1].put = CMATYP;
    VARATB_actions[1].act = AC_STOPSH;
    VARATB_actions[2].put = RPTYP;
    VARATB_actions[2].act = AC_STOPSH;
    VARATB_actions[3].act = AC_ERROR;
    VARBTB_actions[0].put = LPTYP;
    VARBTB_actions[0].act = AC_STOPSH;
    VARBTB_actions[1].put = CMATYP;
    VARBTB_actions[1].act = AC_STOPSH;
    VARBTB_actions[2].put = RPTYP;
    VARBTB_actions[2].act = AC_STOPSH;
    VARBTB_actions[3].act = AC_ERROR;
    VARTB_actions[0].put = VARTYP;
    VARTB_actions[0].act = AC_STOPSH;
    VARTB_actions[1].put = FNCTYP;
    VARTB_actions[1].act = AC_STOP;
    VARTB_actions[2].put = ARYTYP;
    VARTB_actions[2].act = AC_STOP;
    VARTB_actions[3].act = AC_ERROR;

}

/* =========================================================================
 * Tree node — uses SIL names / codes throughout
 *
 * Each NODE corresponds to one SIL "tree node" (NODESZ = 3*DESCR words in SIL).
 * In SIL the tree is built with ADDSON/ADDSIB; here we use a flat children[] array.
 * stype mirrors the SIL STYPE field: the same integer codes from equ.h that
 * stream() writes into STYPE are stored here to label every node kind.
 * ========================================================================= */

typedef struct NODE NODE;
struct NODE {
    int      stype;          /* SIL STYPE code: QLITYP/ILITYP/VARTYP/FNCTYP/ADDFNe tc. */
    char    *text;           /* token text: var name, literal value, operator name */
    NODE   **children;       /* child nodes (args, operands) — grown with realloc */
    int      nchildren, nalloc; /* used / allocated slots in children[] */
    /* numeric literal payloads (only one is valid, selected by stype) */
    long long ival;          /* integer value when stype==ILITYP */
    double    fval;          /* float value when stype==FLITYP */
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

/* FORWRD — "Procedure to get to next character" (v311.sil:2214)
 * Streams TEXTSP through FRWDTB: skips transparent chars (space/tab, chrs[]=0),
 * stops on the first field delimiter and sets BRTYPE to that delimiter's STYPE.
 * Equivalent to SIL:  STREAM XSP,TEXTSP,FRWDTB  then  MOVD BRTYPE,STYPE. */
static void FORWRD(void) {
    stream_ret_t r = stream(&XSP, &TEXTSP, &FRWDTB); /* scan to next delimiter */
    if (r == ST_ERROR) { sil_error("FORWRD: scan error"); return; }
    BRTYPE = STYPE;  /* STYPE = EQTYP/CLNTYP/EOSTYP/NBTYP/RPTYP etc. */
}

/* FORBLK — "Procedure to get to nonblank" (v311.sil:2241)
 * Like FORWRD but starts with IBLKTB to skip the mandatory inter-field blank.
 * IBLKTB chains to FRWDTB on the first non-blank char (AC_GOTO).
 * Returns ST_ERROR if the current position is NOT a blank (no inter-field gap);
 * that condition is the BINOP1 / no-blank path in BINOP(). */
static void FORBLK(void) {
    stream_ret_t r = stream(&XSP, &TEXTSP, &IBLKTB); /* skip blank, classify delimiter */
    if (r == ST_ERROR) { sil_error("FORBLK: scan error"); return; }
    BRTYPE = STYPE;  /* FORJRN: "MOVD BRTYPE,STYPE" (v311.sil:2217) */
}

/* =========================================================================
 * CATFN — sentinel return value meaning juxtaposition (blank concatenation).
 * SIL uses CONCL descriptor for this; we use a distinct integer.
 * ========================================================================= */
#define CATFN  100  /* juxtaposition operator: blank-separated concatenation.
                    * SIL uses CONCL descriptor (v311.sil:10773 "CONFN,FNC,0 — concatenation").
                    * Returned by BINOP() when a blank was found but no explicit operator char followed.
                    * expr_prec_continue() builds a CAT/SEQ node when it sees CATFN. */

/* =========================================================================
 * BINOP — reads binary operator via BIOPTB (called from EXPR)
 *
 * Mirrors SIL BINOP procedure exactly (v311.sil line 1558):
 *
 *   BINOP:  RCALL ,FORBLK,,BINOP1   — skip leading blank; if no blank → BINOP1
 *           AEQLC BRTYPE,NBTYP,RTN2 — if BRTYPE=NBTYP (non-blank field break) → fail (RTN2)
 *           STREAM XSP,TEXTSP,BIOPTB,BINCON  — read operator; if fail → BINCON
 *           BINCON: return CATFN (juxtaposition)
 *   BINOP1: RCALL ,FORWRD,,COMP3    — if no leading blank, use FORWRD
 *           SELBRA BRTYPE,(...RTN2...) — break chars → fail
 *
 * Returns: operator code (ADDFN, SUBFN, MPYFN, EXPFN, ORFN, ...) on success,
 *          CATFN (100) for juxtaposition (blank with no following operator),
 *          0 if no operator and no juxtaposition (field delimiter or EOS).
 * ========================================================================= */

static int BINOP(void) {
    spec_t saved_text = TEXTSP;

    /* SIL BINOP: RCALL ,FORBLK,,BINOP1 */
    /* Try to skip a leading blank */
    spec_t blank_tok;
    stream_ret_t br = stream(&blank_tok, &TEXTSP, &IBLKTB);

    if (br == ST_EOS) {
        /* End of input — no operator */
        TEXTSP = saved_text;
        return 0;
    }

    if (br == ST_ERROR) {
        /* IBLKTB error = no blank at current position → BINOP1 path */
        TEXTSP = saved_text;
        /* BINOP1: RCALL ,FORWRD,,COMP3 — find next character */
        FORWRD();
        /* SELBRA BRTYPE,(,RTN2,RTN2,,,RTN2,RTN2) — field delimiters fail */
        switch (BRTYPE) {
        case RPTYP: case CMATYP: case EOSTYP: case EQTYP:
        case CLNTYP: case RBTYP:
            TEXTSP = saved_text;
            return 0;
        default:
            TEXTSP = saved_text;
            return 0;
        }
    }

    /* br == ST_STOP: blank was found and consumed.
     * IBLKTB chains to FRWDTB which sets STYPE:
     *   NBTYP  = stopped before a real token (e.g. '+', '*') — proceed to BIOPTB.
     *   EOSTYP/CLNTYP/EQTYP/RPTYP/CMATYP/RBTYP = field delimiter → no operator.
     * SIL AEQLC BRTYPE,NBTYP,RTN2 only fires on the BINOP1 (no-blank) path, NOT here.
     * On the blank-found path, NBTYP is the normal/expected result — the operator char
     * is sitting right there in TEXTSP. We must fall through to BIOPTB. */
    int stype_after_blank = STYPE;  /* what IBLKTB→FRWDTB found after the blank */
    if (stype_after_blank == EOSTYP || stype_after_blank == CLNTYP ||
        stype_after_blank == EQTYP  || stype_after_blank == RPTYP  ||
        stype_after_blank == CMATYP || stype_after_blank == RBTYP) {
        /* Field delimiter after blank → end of expression */
        TEXTSP = saved_text;
        return 0;
    }

    /* Blank was found, TEXTSP now points past it at the operator or next token.
     * Try BIOPTB to consume the operator character(s). */
    spec_t op_tok;
    stream_ret_t or_ = stream(&op_tok, &TEXTSP, &BIOPTB);

    if (or_ == ST_ERROR || or_ == ST_EOS) {
        /* BINCON: no explicit operator → juxtaposition (concatenation) */
        /* Do NOT restore TEXTSP — the blank was real, stay past it */
        return CATFN;
    }

    /* Got an operator. STYPE = ADDFN, SUBFN, MPYFN (→STARTB), EXPFN, etc. */
    int op = STYPE;

    /* STARTB disambiguates * vs **: consume the trailing space or second * */
    /* stream() already chased the BIOPTB GOTO to STARTB/TBLKTB if needed,
     * so STYPE is already resolved: MPYFN for *, EXPFN for **. */

    return op;
}

/* =========================================================================
 * Operator precedence — mirrors SIL's precedence descriptor in function nodes
 * SIL stores prec in CODE+2*DESCR of each operator function descriptor.
 * We table it directly.
 * ========================================================================= */

/* op_prec — operator precedence for Pratt-style precedence climbing.
 * Mirrors the precedence encoded in SIL's operator CODE+2*DESCR fields
 * (v311.sil:2155 "GETDC EXOPCL,EXOPCL,2*DESCR — Get left precedence").
 * Higher number = binds tighter.  BIAMFN(&) is lowest; NAMFN/DOLFN are highest. */
static int op_prec(int fn) {
    switch (fn) {
    case BIAMFN: return 1;   /* '&' user-definable — lowest precedence of all */
    case ORFN:   return 2;   /* '|' pattern alternation — second lowest */
    case ADDFN:
    case SUBFN:  return 3;   /* '+' '-' addition/subtraction */
    case MPYFN:
    case DIVFN:
    case BIPRFN: return 4;   /* '*' '/' '%' multiplicative */
    case EXPFN:  return 5;   /* '**' exponentiation — right-associative */
    case BIATFN: return 6;   /* '@' cursor — binds tight */
    case NAMFN:
    case DOLFN:  return 7;   /* '.' '$' naming/capture — tightest, right-associative */
    default:     return 3;   /* other user-definable operators default to additive prec */
    }
}

/* op_right_assoc — true for operators that associate right-to-left.
 * SIL's tree builder (ADDSON/INSERT) achieves right-associativity by inserting
 * nodes at the right spine of the tree.  We replicate this with next_min=prec
 * instead of prec+1 in expr_prec_continue(). */
static int op_right_assoc(int fn) {
    return fn == EXPFN  /* '**' — 2**3**2 = 2**(3**2)=512, not (2**3)**2=64 */
        || fn == NAMFN  /* '.'  — A.B.C → A.(B.C) */
        || fn == DOLFN  /* '$'  — immediate naming, same right-spine rule */
        || fn == BIATFN;/* '@'  — cursor capture, right-associative */
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

/* stype_name — human-readable label for an STYPE integer.
 * STYPE codes are context-dependent: the same integer means different things
 * depending on which table produced it.  The labels here list all meanings
 * for each numeric value so the debug output is self-documenting. */
static const char *stype_name(int st) {
    static const char *names[] = {
        "ST(0)",
        "QLITYP=1/UGOTYP=1/NEWTYP=1/NBTYP=1", /* 1: quoted-lit | uncond-goto | new-card | non-blank */
        "ILITYP=2/SGOTYP=2/CMATYP=2/CMTTYP=2", /* 2: int-lit | :S( goto | comma | comment */
        "VARTYP=3/FGOTYP=3/RPTYP=3/CTLTYP=3",  /* 3: variable | :F( goto | ) | control-card */
        "NSTTYP=4/EQTYP=4/UTOTYP=4/CNTTYP=4",  /* 4: nested-( | = repl | :< direct | continuation */
        "FNCTYP=5/CLNTYP=5/STOTYP=5",           /* 5: func-call | : goto-field | :S< direct */
        "FLITYP=6/EOSTYP=6/FTOTYP=6",           /* 6: float-lit | end-of-stmt | :F< direct */
        "ARYTYP=7/RBTYP=7",                      /* 7: array-ref | ) or > closing bracket */
    };
    if (st >= 0 && st <= 7) return names[st];
    { static char buf[20]; snprintf(buf,20,"ST(%d)",st); return buf; }
}

/* =========================================================================
 * ELEMNT — element analysis procedure (v311.sil:1924 "Element analysis procedure")
 *
 * Mirrors SIL exactly:
 *   RCALL ELEMND,UNOP       — collect chain of unary prefix operators into ELEMND tree
 *   STREAM XSP,TEXTSP,ELEMTB — classify first char of the next token
 *   SELBRA STYPE,(ELEILT,ELEVBL,ELENST,ELEFNC,ELEFLT,ELEARY)
 *                             — branch to handler: integer / variable / nested /
 *                               function-call / float / array-ref
 *
 * ELEMND  = "ELEMent Node" — SIL temporary holding the built tree
 * ELEXND  = "ELEment eXtra Node" — SIL temporary for the leaf node
 * ELEILT  = integer literal handler
 * ELEVBL  = variable / literal handler
 * ELENST  = nested (parenthesized) expression handler
 * ELEFNC  = function call handler
 * ELEFLT  = float literal handler
 * ELEARY  = array subscript handler
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
        if (r == ST_ERROR || r == ST_EOS) { TEXTSP = saved; break; }
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

        if (final == FNCTYP) {   /* ELEFNC: function call — v311.sil:1989 */
            /* SIL: RCALL ELEXND,EXPR,,RTN1 — evaluate first arg.
             * EXPR sets BRTYPE via BINOP->FORWRD when it hits ) or ,.
             * AEQLC BRTYPE,RPTYP,,ELEMN3 — if ), done.
             * AEQLC BRTYPE,CMATYP,ELECMA — if ,, loop for next arg.
             * No FORWRD/FORBLK here — EXPR consumes its own delimiter. */
            while (!g_error) {
                NODE *arg = EXPR();   /* EXPR sets BRTYPE on exit */
                if (g_error) break;
                if (BRTYPE == RPTYP || BRTYPE == 0 || BRTYPE == EOSTYP) {
                    /* RPTYP: ) — done (SIL ELEMN3). Also handle EOS for zero-arg. */
                    if (arg && arg->stype != 0) node_add(atom, arg);
                    break;
                }
                node_add(atom, arg);
                if (BRTYPE == CMATYP) continue;   /* comma — next arg */
                sil_error("ELEMNT: expected ) or , in arg list, got BRTYPE=%d", BRTYPE);
                break;
            }
        } else if (final == ARYTYP) {  /* ELEARY: array subscript — v311.sil:2032 */
            while (!g_error) {
                NODE *sub = EXPR();
                if (g_error) break;
                if (BRTYPE == RBTYP || BRTYPE == 0 || BRTYPE == EOSTYP) {
                    if (sub && sub->stype != 0) node_add(atom, sub);
                    break;
                }
                node_add(atom, sub);
                if (BRTYPE == CMATYP) continue;
                sil_error("ELEMNT: expected > or , in subscript, got BRTYPE=%d", BRTYPE);
                break;
            }
        }
        break;
    }

    case NSTTYP: {  /* ELENST: parenthesized expression — SIL v311.sil:1977 */
        /* SIL: PUSH ELEMND; RCALL ELEXND,EXPR,,RTN1; POP ELEMND
         * AEQLC BRTYPE,RPTYP,,ELEMN1 — EXPR sets BRTYPE=RPTYP on the closing ).
         * No FORWRD needed — EXPR->BINOP->FORWRD already consumed the ). */
        atom = EXPR();
        /* BRTYPE is already RPTYP (set by EXPR via BINOP->FORWRD on closing )) */
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
 * EXPR / EXPR1 — expression compiler (v311.sil:2093 "Procedure to compile expression")
 *
 * Mirrors SIL's operator-precedence tree builder:
 *   EXPR:  RCALL EXELND,ELEMNT  — compile one element into EXELND
 *          then EXPR2 loop: RCALL EXOPCL,BINOP → get operator → insert into tree
 *   EXPR1: same entry point but called from CMPILE's scan-replace path
 *
 * EXELND = "EXpression ELement Node" — SIL temp for the element just parsed
 * EXOPCL = "EXpression OPerator CLass" — SIL temp for the current operator descriptor
 * EXOPND = "EXpression OPerand Node"  — SIL temp for the current operand node
 * EXPRND = "EXPRession Node"          — SIL temp for the accumulated expression tree
 *
 * SIL builds the tree with ADDSON (add child) and INSERT (insert between nodes).
 * We use Pratt-style precedence climbing (expr_prec_continue) — same result,
 * simpler to follow.  BRTYPE is updated by each FORBLK/FORWRD call inside ELEMNT. *
 * CATFN (juxtaposition): when BINOP() finds a blank but no operator char,
 * expr_prec_continue() builds a CAT node, mirroring SIL's BINCON → CONCL path.
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
    /* CATFN juxtaposition precedence = 10 (highest — tighter than any explicit op).
     * SIL BINCON path: blank found, no explicit operator char → CONCL concatenation.
     * We use a flat n-ary CAT node: accumulate all juxtaposed elements into one. */
#define CATFN_PREC 10
    for (;;) {
        spec_t saved = TEXTSP;
        int op = BINOP();
        if (!op) { TEXTSP = saved; break; }     /* no operator → done */

        if (op == CATFN) {
            /* Juxtaposition — CATFN_PREC=10, left-associative (collect into flat CAT) */
            if (CATFN_PREC < min_prec) { TEXTSP = saved; break; }
            NODE *right = expr_prec(CATFN_PREC + 1);
            if (!right) { TEXTSP = saved; break; }
            /* Flatten: if left is already a CAT node, append; else create new CAT */
            if (left->stype == CATFN) {
                node_add(left, right);
            } else {
                NODE *cat = node_new(CATFN, "CAT", -1);
                node_add(cat, left);
                node_add(cat, right);
                left = cat;
            }
            continue;
        }

        int prec  = op_prec(op);
        if (prec < min_prec) { TEXTSP = saved; break; }

        int next_min = op_right_assoc(op) ? prec : prec + 1;
        NODE *right = expr_prec(next_min);

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
 * CMPILE — compile one statement (v311.sil:1608 "Procedure to compile statement")
 *
 * Implements the full SNOBOL4 statement grammar:
 *   [label:]  subject  [pattern]  [= replacement]  [:goto]
 *
 * SIL branch labels and their meanings:
 *   CMPIL0 = after label scan ("Break out label")
 *   CMPILA = after FORBLK; start of body ("Get to next character")
 *   CMPSUB = subject field (RCALL SUBJND,ELEMNT)
 *   CMPSB1 = after subject; decide next field
 *   CMPAT2 = pattern field (RCALL PATND,EXPR)
 *   CMPFRM = '=' replacement without pattern (SUBJECT = REPLACEMENT)
 *   CMPASP = '=' replacement after pattern  (SUBJECT PATTERN = REPLACEMENT)
 *   CMPGO  = goto field (STREAM XSP,TEXTSP,GOTOTB)
 *   CMPSGO / CMPFGO / CMPUGO = success / failure / unconditional goto handlers
 *
 * BOSCL  (v311.sil:11049 "Offset of beginning of statement") — we ignore this;
 *         in SIL it tracks object-code offset for the compiler's output buffer.
 * ========================================================================= */

/* STMT — one compiled SNOBOL4 statement; mirrors CSNOBOL4's STMT_t (src/frontend/snobol4/scrip_cc.h)
 * and the SIL object-code layout produced by CMPILE. */
typedef struct STMT STMT;
struct STMT {
    char *label;          /* column-1 label text, or NULL if no label */
    NODE *subject;        /* subject expression (always present per SNOBOL4 grammar) */
    NODE *pattern;        /* pattern expression, or NULL if no pattern field */
    NODE *replacement;    /* replacement expression, or NULL (present when has_eq) */
    int   has_eq;         /* 1 if '=' was present in source (distinguishes assign from match) */
    char *go_s;           /* :S(label) success-goto target, or NULL */
    char *go_f;           /* :F(label) failure-goto target, or NULL */
    char *go_u;           /* :(label)  unconditional-goto target, or NULL */
    int   is_end;         /* 1 if this is the END statement (terminates program) */
    STMT *next;           /* linked-list chain to next statement in program */
};

/* xsp_dup — copy the last token text from XSP into a malloc'd C string.
 * XSP (eXtra SPecifier) is the output spec filled by stream(); it points
 * into the source line buffer and is valid only until the next stream() call. */
static char *xsp_dup(void) {
    return strndup(XSP.ptr, XSP.len); /* XSP.ptr/XSP.len = (pointer,length) into source line */
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

    /* BRTYPE==0 means ST_EOS on empty body (bare label line or blank line).
     * Treat same as EOSTYP — no body to parse. */
    if (BRTYPE == 0 || BRTYPE == EOSTYP) {
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
        if (BRTYPE == EOSTYP || BRTYPE == 0) return s;  /* bare invoke (0=ST_EOS) */

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
        FORWRD();   /* consume closing ) → BRTYPE=RPTYP */
        return s;
    }

    if (gotype == SGOTYP || gotype == STOTYP) {
        /* Success: :S( label ) */
        NODE *lbl = EXPR();
        s->go_s = lbl ? strdup(lbl->text ? lbl->text : "") : NULL;
        FORWRD();   /* consume closing ) → TEXTSP now at F, :F, or EOS */
        /* SNOBOL4 allows :S(x)F(y) or :S(x):F(y) — skip optional ':' */
        if (BRTYPE != EOSTYP && BRTYPE != 0 && TEXTSP.len > 0) {
            spec_t saved2 = TEXTSP;
            if (TEXTSP.ptr[0] == ':') { /* skip optional colon */
                TEXTSP.ptr++; TEXTSP.len--;
            }
            stream_ret_t gr = stream(&XSP, &TEXTSP, &GOTOTB);
            if (gr != ST_ERROR && (STYPE == FGOTYP || STYPE == FTOTYP)) {
                NODE *fl = EXPR();
                s->go_f = fl ? strdup(fl->text ? fl->text : "") : NULL;
                FORWRD();
            } else {
                TEXTSP = saved2;  /* not a goto — restore */
            }
        }
        return s;
    }

    if (gotype == FGOTYP || gotype == FTOTYP) {
        /* Failure: :F( label ) */
        NODE *lbl = EXPR();
        s->go_f = lbl ? strdup(lbl->text ? lbl->text : "") : NULL;
        FORWRD();   /* consume closing ) */
        if (BRTYPE != EOSTYP && BRTYPE != 0 && TEXTSP.len > 0) {
            spec_t saved2 = TEXTSP;
            if (TEXTSP.ptr[0] == ':') {
                TEXTSP.ptr++; TEXTSP.len--;
            }
            stream_ret_t gr = stream(&XSP, &TEXTSP, &GOTOTB);
            if (gr != ST_ERROR && (STYPE == SGOTYP || STYPE == STOTYP)) {
                NODE *sl = EXPR();
                s->go_s = sl ? strdup(sl->text ? sl->text : "") : NULL;
                FORWRD();
            } else {
                TEXTSP = saved2;
            }
        }
        return s;
    }

    sil_error("CMPGO: unrecognized goto type %d", gotype);
    return s;
    }
}

/* =========================================================================
 * IR printer — s-expression output using SIL node type names
 * Prints the parse tree to stdout in a Lisp-style nested form for debugging.
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
 * main — read source lines, classify each with CARDTB, compile with CMPILE
 *
 * SIL equivalent: the outer read loop in XLATNX/FORRN0 (v311.sil:1033+).
 * NEWCRD (v311.sil:SELBRA STYPE,(,CMTCRD,CTLCRD,CNTCRD)) dispatches on
 * card type; we inline that logic with a simple switch on ctype. */

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
