#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
typedef enum { ACT_CONTIN=0, ACT_STOP, ACT_STOPSH, ACT_ERROR, ACT_GOTO } action_t;
typedef struct syntab syntab_t;
typedef struct { int put; action_t act; syntab_t *go; } acts_t;
struct syntab { const char *name; unsigned char chrs[256]; acts_t *actions; };
static int STYPE;
typedef enum { ST_STOP, ST_EOS, ST_ERROR } stream_ret_t;
typedef struct { const char *ptr; int len; } spec_t;
static int g_trace_stream = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static stream_ret_t stream(spec_t *sp1, spec_t *sp2, syntab_t *tp) {
    const char *tab_name  = tp->name;
    const char *input_ptr = sp2->ptr;
    int         input_len = sp2->len;
    unsigned char *cp = (unsigned char *)sp2->ptr;
    int len = sp2->len;
    stream_ret_t ret;
    int put = 0;
    for (; len > 0; cp++, len--) {
        unsigned aindex = tp->chrs[*cp];
        if (aindex == 0) continue;
        acts_t *ap = tp->actions + (aindex - 1);
        if (ap->put) put = ap->put;
        switch (ap->act) {
        case ACT_CONTIN: break;
        case ACT_STOP:   cp++; len--;
        case ACT_STOPSH: ret = ST_STOP; goto done;
        case ACT_ERROR:  STYPE = 0; return ST_ERROR;
        case ACT_GOTO:   tp = ap->go; break;
        }
    }
    ret = ST_EOS;
done:
    STYPE = put;
    sp1->ptr = sp2->ptr;
    sp1->len = sp2->len - len;
    if (ret != ST_EOS) sp2->ptr += sp1->len;
    sp2->len = len;
    if (g_trace_stream) {
        char ibuf[32]; int ilen = input_len < 20 ? input_len : 20;
        memcpy(ibuf, input_ptr, ilen); ibuf[ilen] = 0;
        for (int i=0;i<ilen;i++) if ((unsigned char)ibuf[i]<32||ibuf[i]==127) ibuf[i]='.';
        const char *retname = ret==ST_STOP?"STOP":ret==ST_EOS?"EOS":"ERROR";
        fprintf(stderr, "STREAM %-10s [%-20s] -> ret=%-5s stype=%d\n",
                tab_name, ibuf, retname, put);
    }
    return ret;
}
#define QLITYP  1
#define ILITYP  2
#define VARTYP  3   /* variable name: [A-Za-z][A-Za-z0-9]*     (equ.h) */
#define NSTTYP  4
#define FNCTYP  5
#define FLITYP  6
#define ARYTYP  7
#define SELTYP  50
#define NBTYP   1
#define EQTYP   4
#define CLNTYP  5
#define EOSTYP  6
#define RPTYP   3
#define CMATYP  2
#define RBTYP   7
#define SGOTYP  2
#define FGOTYP  3
#define UGOTYP  1
#define STOTYP  5
#define FTOTYP  6
#define UTOTYP  4
#define CMTTYP  2   /* comment card: first char is '*'          (equ.h) */
#define CTLTYP  3
#define CNTTYP  4
#define NEWTYP  1
#define ADDFN   201
#define SUBFN   202
#define MPYFN   203  /* X * Y  multiplication      (v311.sil:11682 "MPY,0,2") */
#define DIVFN   204
#define EXPFN   205  /* X ** Y exponentiation       (v311.sil:11679 "EXPOP,0,2") */
#define ORFN    206
#define NAMFN   207
#define DOLFN   208
#define BIATFN  209
#define BIPDFN  210
#define BIPRFN  211
#define BIAMFN  212
#define BINGFN  213
#define BIQSFN  214
#define BISNFN  215
#define BIEQFN  216
#define PLSFN   301
#define MNSFN   302
#define DOTFN   303
#define INDFN   304
#define STRFN   305  /* *X  unevaluated expression  (v311.sil:11720 "STR,0,1") */
#define SLHFN   306
#define PRFN    307
#define ATFN    308
#define PDFN    309
#define KEYFN   310
#define NEGFN   311
#define BARFN   312
#define QUESFN  313
#define AROWFN  314
syntab_t LBLXTB;
static acts_t LBLTB_actions[] = {
    {0, ACT_GOTO, &LBLXTB},
    {0, ACT_STOPSH, NULL},
    {0, ACT_ERROR, NULL},
};
static acts_t LBLXTB_actions[] = {
    {0, ACT_STOPSH, NULL},
};
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
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
}, LBLXTB_actions };
syntab_t LBLTB = { "LBLTB", {
     3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     2,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  3,  2,  3,  3,  3,  3,
     3,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  3,  3,  3,  3,  3,
     3,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  3,  3,  3,  3,  3,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
}, LBLTB_actions };
static acts_t CARDTB_actions[] = {
    {CMTTYP, ACT_STOPSH, NULL},
    {CTLTYP, ACT_STOPSH, NULL},
    {CNTTYP, ACT_STOPSH, NULL},
    {NEWTYP, ACT_STOPSH, NULL},
};
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
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
}, CARDTB_actions };
static acts_t FRWDTB_actions[] = {
    {EQTYP, ACT_STOP, NULL},
    {RPTYP, ACT_STOP, NULL},
    {RBTYP, ACT_STOP, NULL},
    {CMATYP, ACT_STOP, NULL},
    {CLNTYP, ACT_STOP, NULL},
    {EOSTYP, ACT_STOP, NULL},
    {NBTYP, ACT_STOPSH, NULL},
};
syntab_t FRWDTB = { "FRWDTB", {
     7,  7,  7,  7,  7,  7,  7,  7,  7,  0,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     0,  7,  7,  7,  7,  7,  7,  7,  7,  2,  7,  7,  4,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  5,  6,  7,  1,  3,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  3,  7,  1,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
     7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
}, FRWDTB_actions };
static acts_t IBLKTB_actions[] = {
    {0,      ACT_GOTO,   &FRWDTB},
    {EOSTYP, ACT_STOP,   NULL},
    {EOSTYP, ACT_STOP,   NULL},
    {NBTYP,  ACT_STOPSH, NULL},     /* 3: any non-blank token start — stop short,
                                     *    BRTYPE=NBTYP, leave byte for ELEMTB
                                     *    (ASCII A-Z a-z 0-9 ops + 0x80-0xFF UTF-8) */
};
syntab_t IBLKTB = { "IBLKTB", {
     3,  3,  3,  3,  3,  3,  3,  3,  3,  1,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     1,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
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
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
}, IBLKTB_actions };
static acts_t ELEMTB_actions[] = {
    {ILITYP, ACT_GOTO, NULL},
    {VARTYP, ACT_GOTO, NULL},
    {QLITYP, ACT_GOTO, NULL},
    {QLITYP, ACT_GOTO, NULL},
    {NSTTYP, ACT_STOP, NULL},
    {0,      ACT_ERROR, NULL},
    {0,      ACT_ERROR, NULL},
    {VARTYP, ACT_GOTO, NULL},
};
syntab_t VARTB, INTGTB, SQLITB, DQLITB;
syntab_t ELEMTB = { "ELEMTB", {
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  4,  6,  6,  6,  6,  3,  5,  6,  6,  6,  6,  6,  6,  6,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  6,  6,  6,  6,  6,  6,
     6,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  6,  6,  6,  6,  6,
     6,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
}, ELEMTB_actions };
static acts_t VARTB_actions[] = {
    {VARTYP, ACT_STOPSH, NULL},
    {FNCTYP, ACT_STOP,   NULL},
    {ARYTYP, ACT_STOP,   NULL},
    {0,      ACT_ERROR,  NULL},
    {0,      ACT_ERROR,  NULL},
    {VARTYP, ACT_GOTO,   NULL},
};
syntab_t VARTB = { "VARTB", {
     4,  4,  4,  4,  4,  4,  4,  4,  4,  1,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
     1,  4,  4,  4,  4,  4,  4,  4,  2,  1,  4,  4,  1,  4,  0,  4,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4,  1,  3,  4,  1,  4,
     4,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  4,  1,  4,  0,
     4,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4,  4,  4,  4,  4,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
}, VARTB_actions };
syntab_t FLITB;
static acts_t INTGTB_actions[] = {
    {ILITYP, ACT_STOPSH, NULL},
    {FLITYP, ACT_GOTO, NULL},
    {FLITYP, ACT_GOTO, NULL},
    {0, ACT_ERROR, NULL},
};
syntab_t EXPTB, EXPBTB;
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
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
}, INTGTB_actions };
static acts_t FLITB_actions[] = {
    {0, ACT_STOPSH, NULL},
    {0, ACT_GOTO, NULL},
    {0, ACT_ERROR, NULL},
};
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
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
}, FLITB_actions };
static acts_t EXPBTB_actions[] = {
    {0, ACT_STOPSH, NULL},
    {0, ACT_ERROR, NULL},
};
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
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
}, EXPBTB_actions };
static acts_t EXPTB_actions[] = {
    {0, ACT_GOTO, NULL},
    {0, ACT_ERROR, NULL},
};
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
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
}, EXPTB_actions };
static acts_t SQLITB_actions[] = {
    {0, ACT_STOP, NULL},
};
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
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
}, SQLITB_actions };
static acts_t DQLITB_actions[] = {
    {0, ACT_STOP, NULL},
};
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
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
}, DQLITB_actions };
syntab_t NBLKTB;
static acts_t NBLKTB_actions[] = {
    {0, ACT_ERROR, NULL},
    {0, ACT_STOPSH, NULL},
};
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
static acts_t UNOPTB_actions[] = {
    {PLSFN, ACT_GOTO, &NBLKTB},
    {MNSFN, ACT_GOTO, &NBLKTB},
    {DOTFN, ACT_GOTO, &NBLKTB},
    {INDFN, ACT_GOTO, &NBLKTB},
    {STRFN, ACT_GOTO, &NBLKTB},
    {SLHFN, ACT_GOTO, &NBLKTB},
    {PRFN, ACT_GOTO, &NBLKTB},
    {ATFN, ACT_GOTO, &NBLKTB},
    {PDFN, ACT_GOTO, &NBLKTB},
    {KEYFN, ACT_GOTO, &NBLKTB},
    {NEGFN, ACT_GOTO, &NBLKTB},
    {BARFN, ACT_GOTO, &NBLKTB},
    {QUESFN, ACT_GOTO, &NBLKTB},
    {AROWFN, ACT_GOTO, &NBLKTB},
    {0, ACT_ERROR, NULL},
};
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
extern syntab_t TBLKTB, STARTB;
static acts_t BIOPTB_actions[] = {
    {ADDFN, ACT_GOTO, &TBLKTB},
    {SUBFN, ACT_GOTO, &TBLKTB},
    {NAMFN, ACT_GOTO, &TBLKTB},
    {DOLFN, ACT_GOTO, &TBLKTB},
    {MPYFN, ACT_GOTO, &STARTB},
    {DIVFN, ACT_GOTO, &TBLKTB},
    {BIATFN, ACT_GOTO, &TBLKTB},
    {BIPDFN, ACT_GOTO, &TBLKTB},
    {BIPRFN, ACT_GOTO, &TBLKTB},
    {EXPFN, ACT_GOTO, &TBLKTB},
    {ORFN, ACT_GOTO, &TBLKTB},
    {BIAMFN, ACT_GOTO, &TBLKTB},
    {BINGFN, ACT_GOTO, &TBLKTB},
    {BIQSFN, ACT_GOTO, &TBLKTB},
    {0, ACT_ERROR, NULL},
};
static acts_t STARTB_actions[] = {
    {0, ACT_STOP, NULL},
    {EXPFN, ACT_GOTO, &TBLKTB},
    {0, ACT_ERROR, NULL},
};
syntab_t STARTB = { "STARTB", {
     3,  3,  3,  3,  3,  3,  3,  3,  3,  1,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     1,  3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
}, STARTB_actions };
static acts_t TBLKTB_actions[] = {
    {0, ACT_STOP, NULL},
    {0, ACT_ERROR, NULL},
};
syntab_t TBLKTB = { "TBLKTB", {
     2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     1,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
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
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
}, TBLKTB_actions };
syntab_t BIOPTB = { "BIOPTB", {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 11, 15,  8,  4,  9, 12, 15, 15, 15,  5,  1, 15,  2,  3,  6,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 14,
     7, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 13, 15, 10, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 11, 15, 13, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
}, BIOPTB_actions };
static acts_t GOTSTB_actions[] = {
    {SGOTYP, ACT_STOP, NULL},
    {STOTYP, ACT_STOP, NULL},
    {0, ACT_ERROR, NULL},
};
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
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
}, GOTSTB_actions };
static acts_t GOTFTB_actions[] = {
    {FGOTYP, ACT_STOP, NULL},
    {FTOTYP, ACT_STOP, NULL},
    {0, ACT_ERROR, NULL},
};
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
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
}, GOTFTB_actions };
static acts_t GOTOTB_actions[] = {
    {0, ACT_GOTO, &GOTSTB},
    {0, ACT_GOTO, &GOTFTB},
    {UGOTYP, ACT_STOP, NULL},
    {UTOTYP, ACT_STOP, NULL},
    {0, ACT_ERROR, NULL},
};
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
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
}, GOTOTB_actions };
static acts_t UTF8TB_actions[] = {
    {0,      ACT_CONTIN, NULL},
    {VARTYP, ACT_GOTO,   NULL},
};
syntab_t UTF8TB = { "UTF8TB", {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
}, UTF8TB_actions };
#include "unicode_alpha_ranges.h"   /* AUTO-GENERATED — 657 L* ranges */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int utf8_decode_first(const unsigned char *s, int len, uint32_t *out_cp)
{
    if (len < 1) return 0;
    unsigned char b0 = s[0];
    if (b0 < 0x80) { *out_cp = b0; return 1; }
    int nbytes;
    uint32_t cp;
    if      ((b0 & 0xE0) == 0xC0) { nbytes = 2; cp = b0 & 0x1F; }
    else if ((b0 & 0xF0) == 0xE0) { nbytes = 3; cp = b0 & 0x0F; }
    else if ((b0 & 0xF8) == 0xF0) { nbytes = 4; cp = b0 & 0x07; }
    else return 0;
    if (len < nbytes) return 0;
    for (int i = 1; i < nbytes; i++) {
        if ((s[i] & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    if (nbytes == 2 && cp < 0x0080) return 0;
    if (nbytes == 3 && cp < 0x0800) return 0;
    if (nbytes == 4 && cp < 0x10000) return 0;
    *out_cp = cp;
    return nbytes;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int unicode_is_alpha(uint32_t cp)
{
    int lo = 0, hi = UNICODE_ALPHA_RANGES_N - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if      (cp < unicode_alpha_ranges[mid][0]) hi = mid - 1;
        else if (cp > unicode_alpha_ranges[mid][1]) lo = mid + 1;
        else return 1;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int utf8_is_alpha_start(const unsigned char *s, int len)
{
    if (len < 1 || s[0] < 0x80) return 0;
    uint32_t cp = 0;
    if (utf8_decode_first(s, len, &cp) == 0) return 0;
    return unicode_is_alpha(cp);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void init_tables(void) {
    ELEMTB_actions[0].go  = &INTGTB;
    ELEMTB_actions[1].go  = &VARTB;
    ELEMTB_actions[2].go  = &SQLITB;
    ELEMTB_actions[3].go  = &DQLITB;
    ELEMTB_actions[7].go  = &UTF8TB;
    UTF8TB_actions[1].go  = &VARTB;
    VARTB_actions[5].go   = &UTF8TB;
    INTGTB_actions[1].go  = &FLITB;
    INTGTB_actions[2].go  = &EXPTB;
    FLITB_actions[1].go   = &EXPTB;
    EXPTB_actions[0].go   = &EXPBTB;
}
typedef struct CMPND_t CMPND_t;
struct CMPND_t {
    int      stype;
    char    *text;
    CMPND_t   **children;
    int      nchildren, nalloc;
    long long ival;
    double    fval;
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static CMPND_t *cmpnd_new(int stype, const char *text, int tlen) {
    CMPND_t *n  = calloc(1, sizeof *n);
    n->stype = stype;
    n->text  = tlen >= 0 ? strndup(text, tlen) : strdup(text ? text : "");
    return n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void cmpnd_add(CMPND_t *parent, CMPND_t *child) {
    if (!child) return;
    if (parent->n >= parent->_nalloc) {
        parent->_nalloc = parent->_nalloc ? parent->_nalloc * 2 : 4;
        parent->c = realloc(parent->c,
                                   parent->_nalloc * sizeof(CMPND_t*));
    }
    parent->c[parent->n++] = child;
}
static spec_t TEXTSP;
static spec_t XSP;
static int    BRTYPE;
static int  g_error;
static int  g_in_replacement = 0;
static char g_errmsg[256];
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void sil_error(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(g_errmsg, sizeof g_errmsg, fmt, ap);
    va_end(ap);
    g_error = 1;
}
#define MAX_INCLUDE_PATHS 64
const char *g_include_paths[MAX_INCLUDE_PATHS];
int         g_num_include_paths = 0;
#define IO_LINEBUF_SZ 4096
static FILE       *g_io_file     = NULL;
static const char *g_io_path     = NULL;
static int         g_io_lineno   = 0;
static int         g_io_depth    = 0;
static char g_io_linebuf[IO_LINEBUF_SZ];
static char g_pending_buf[IO_LINEBUF_SZ];
static int  g_pending_len  = 0;
static int  g_pending_lineno = 0;
static int  g_stmt_lineno  = 0;
static int  g_io_eof = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static char *resolve_include_path(const char *base_path, const char *incname);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int io_read_raw(FILE *f, char *buf, int bufsz) {
    if (!fgets(buf, bufsz, f)) return -1;
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1]=='\n' || buf[len-1]=='\r')) buf[--len] = '\0';
    return len;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int forrun(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void FORWRD(void) {
retry:;
    stream_ret_t r = stream(&XSP, &TEXTSP, &FRWDTB);
    if (r == ST_ERROR) { sil_error("FORWRD: scan error"); return; }
    if (r == ST_EOS) {
        int fr = forrun();
        if (fr == 2) goto retry;
        BRTYPE = EOSTYP;
        return;
    }
    BRTYPE = STYPE;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void FORBLK(void) {
retry:;
    stream_ret_t r = stream(&XSP, &TEXTSP, &IBLKTB);
    if (r == ST_ERROR) { return; }
    if (r == ST_EOS) {
        int fr = forrun();
        if (fr == 2) goto retry;
        BRTYPE = EOSTYP;
        return;
    }
    BRTYPE = STYPE;
}
#define CATFN  100  /* juxtaposition operator: blank-separated concatenation.
                    * SIL uses CONCL descriptor (v311.sil:10773 "CONFN,FNC,0 — concatenation").
                    * Returned by BINOP() when a blank was found but no explicit operator char followed.
                    * expr_prec_continue() builds a CAT/SEQ node when it sees CATFN. */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int BINOP(void) {
    spec_t saved_text = TEXTSP;
    spec_t blank_tok;
    stream_ret_t br = stream(&blank_tok, &TEXTSP, &IBLKTB);
    if (br == ST_EOS) {
        TEXTSP = saved_text;
        return 0;
    }
    if (br == ST_ERROR) {
        TEXTSP = saved_text;
        FORWRD();
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
    int stype_after_blank = STYPE;
    if (stype_after_blank == EOSTYP || stype_after_blank == CLNTYP ||
        stype_after_blank == EQTYP  || stype_after_blank == RPTYP  ||
        stype_after_blank == CMATYP || stype_after_blank == RBTYP) {
        if (stype_after_blank == EQTYP) BRTYPE = EQTYP;
        TEXTSP = saved_text;
        return 0;
    }
    spec_t op_tok;
    stream_ret_t or_ = stream(&op_tok, &TEXTSP, &BIOPTB);
    if (or_ == ST_ERROR) {
        return CATFN;
    }
    if (or_ == ST_EOS) {
        if (STYPE != 0) return STYPE;
        return CATFN;
    }
    int op = STYPE;
    return op;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int op_prec(int fn) {
    switch (fn) {
    case BIEQFN: return 0;
    case BIAMFN: return 1;
    case BISNFN: return 1;
    case BIQSFN: return 1;
    case ORFN:   return 2;
    case ADDFN:
    case SUBFN:  return 3;
    case MPYFN:
    case DIVFN:
    case BIPRFN: return 4;   /* '*' '/' '%' multiplicative */
    case EXPFN:  return 5;   /* '**' exponentiation — right-associative */
    case BIATFN: return 6;
    case NAMFN:
    case DOLFN:  return 7;
    default:     return 3;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int op_right_assoc(int fn) {
    return fn == BIEQFN
        || fn == EXPFN  /* '**' — 2**3**2 = 2**(3**2)=512, not (2**3)**2=64 */
        || fn == NAMFN
        || fn == DOLFN
        || fn == BIATFN;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
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
    case BISNFN: return "BISNFN(?)";
    case BINGFN: return "BINGFN(~)";
    case BIQSFN: return "BIQSFN(?)";
    case BIEQFN: return "BIEQFN(=)";
    case BARFN:  return "BARFN(|)";
    case AROWFN: return "AROWFN(^)";
    default: { static char buf[16]; snprintf(buf,16,"FN(%d)",fn); return buf; }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const char *stype_name(int st) {
    static const char *names[] = {
        "ST(0)",
        "QLITYP=1/UGOTYP=1/NEWTYP=1/NBTYP=1",
        "ILITYP=2/SGOTYP=2/CMATYP=2/CMTTYP=2",
        "VARTYP=3/FGOTYP=3/RPTYP=3/CTLTYP=3",
        "NSTTYP=4/EQTYP=4/UTOTYP=4/CNTTYP=4",
        "FNCTYP=5/CLNTYP=5/STOTYP=5",
        "FLITYP=6/EOSTYP=6/FTOTYP=6",
        "ARYTYP=7/RBTYP=7",
    };
    if (st >= 0 && st <= 7) return names[st];
    if (st == SELTYP) return "SELECT";
    { static char buf[20]; snprintf(buf,20,"ST(%d)",st); return buf; }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CMPND_t *EXPR(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static CMPND_t *EXPR1(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static CMPND_t *expr_prec_continue(CMPND_t *left, int min_prec);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static CMPND_t *ELEMNT(void) {
    if (g_error) return NULL;
    CMPND_t *unary_chain = NULL;
    CMPND_t *unary_tail  = NULL;
    int   unary_nospace = 0;
    for (;;) {
        spec_t saved = TEXTSP;
        spec_t tok;
        stream_ret_t r = stream(&tok, &TEXTSP, &UNOPTB);
        if (r == ST_EOS) {
            TEXTSP = saved;
            break;
        }
        if (r == ST_ERROR) {
            if (TEXTSP.len >= 2) {
                unsigned char opc = (unsigned char)TEXTSP.ptr[0];
                unsigned char nxt = (unsigned char)TEXTSP.ptr[1];
                unsigned aidx = UNOPTB.chrs[opc];
                int nxt_is_token = (nxt >= 'A' && nxt <= 'Z') ||
                                   (nxt >= 'a' && nxt <= 'z') ||
                                   (nxt >= '0' && nxt <= '9') ||
                                   nxt == '\'' || nxt == '"'  || nxt == '(';
                if (aidx != 15 && aidx != 0 && nxt_is_token) {
                    acts_t *ap = &UNOPTB.actions[aidx - 1];
                    STYPE = ap->put;   /* e.g. STRFN for '*' */
                    TEXTSP.ptr++;
                    TEXTSP.len--;
                    unary_nospace = 1;
                } else {
                    TEXTSP = saved; break;
                }
            } else {
                TEXTSP = saved; break;
            }
        } else {
            unary_nospace = 0;
        }
        int uop = STYPE;
        static const char *uop_names[] = {
            "?","PLSFN","MNSFN","DOTFN","INDFN","STRFN",
            "SLHFN","PRFN","ATFN","PDFN","KEYFN",
            "NEGFN","BARFN","QUESFN","AROWFN"
        };
        const char *nm = (uop >= 301 && uop <= 314)
                         ? uop_names[uop-300] : "UOP?";
        CMPND_t *unode = cmpnd_new(uop, nm, -1);
        if (!unary_chain) { unary_chain = unary_tail = unode; }
        else              { cmpnd_add(unary_tail, unode); unary_tail = unode; }
    }
    if (unary_chain && !unary_nospace) FORWRD();
    stream_ret_t r = stream(&XSP, &TEXTSP, &ELEMTB);
    if (r == ST_ERROR) {
        sil_error("ELEMNT: illegal character");
        return NULL;
    }
    if (r == ST_EOS) {
        if (STYPE == 0) { sil_error("ELEMNT: illegal character"); return NULL; }
    }
    int elem_stype = STYPE;
    CMPND_t *atom = NULL;
    unsigned char first = XSP.len > 0 ? (unsigned char)XSP.ptr[0] : 0;
    int is_digit  = (first >= '0' && first <= '9');
    int is_letter = ((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')
                     || first == '_' || first >= 0x80);
    int is_quote  = (first == 39 || first == 34);
    int is_lparen = (first == '(');
    (void)elem_stype;
    int dispatch = is_digit ? ILITYP : is_letter ? VARTYP :
                   is_quote ? QLITYP : is_lparen ? NSTTYP : 0;
    switch (dispatch) {
    case ILITYP: {
        int final_type = STYPE;
        char buf[64]; memcpy(buf, XSP.ptr, XSP.len < 63 ? XSP.len : 63);
        buf[XSP.len < 63 ? XSP.len : 63] = '\0';
        atom = cmpnd_new(final_type, buf, -1);
        if (final_type == ILITYP) atom->v.ival = atoll(buf);
        else                      atom->fval = atof(buf);
        break;
    }
    case FLITYP: {
        char buf[64]; memcpy(buf, XSP.ptr, XSP.len < 63 ? XSP.len : 63);
        buf[XSP.len < 63 ? XSP.len : 63] = '\0';
        atom = cmpnd_new(FLITYP, buf, -1);
        atom->fval = atof(buf);
        break;
    }
    case QLITYP: {
        const char *p = XSP.ptr + 1;
        int len = XSP.len - 2;
        if (len < 0) len = 0;
        atom = cmpnd_new(QLITYP, p, len);
        break;
    }
    case VARTYP: {
        int final = STYPE;
        if ((unsigned char)XSP.ptr[0] >= 0x80) {
            if (!utf8_is_alpha_start((const unsigned char *)XSP.ptr, XSP.len)) {
                sil_error("ELEMNT: non-alpha Unicode identifier start");
                return NULL;
            }
        }
        const char *p = XSP.ptr;
        int len = XSP.len;
        if (final == FNCTYP || final == ARYTYP) len--;
        atom = cmpnd_new(final, p, len);
        if (final == FNCTYP) {
            FORWRD();
            while (BRTYPE == NBTYP) {; break; }
            while (!g_error) {
                if (BRTYPE == RPTYP) break;
                if (BRTYPE == CMATYP) {
                    cmpnd_add(atom, cmpnd_new(0, "NULL", -1));
                    FORWRD();
                    continue;
                }
                CMPND_t *arg = EXPR();
                cmpnd_add(atom, arg);
                FORWRD();
                if (BRTYPE == RPTYP) break;
                if (BRTYPE == CMATYP) {
                    FORWRD();
                    continue;
                }
                if (BRTYPE == EOSTYP || BRTYPE == CLNTYP) break;
                FORWRD();
            }
        } else if (final == ARYTYP) {
            FORWRD();
            while (!g_error) {
                if (BRTYPE == RBTYP) break;
                if (BRTYPE == CMATYP) {
                    cmpnd_add(atom, cmpnd_new(0, "NULL", -1));
                    FORWRD();
                    continue;
                }
                CMPND_t *sub = EXPR();
                cmpnd_add(atom, sub);
                if (BRTYPE == EQTYP) { FORWRD(); FORWRD(); continue; }
                FORWRD();
                if (BRTYPE == RBTYP) break;
                if (BRTYPE == CMATYP) {
                    FORWRD();
                    continue;
                }
                sil_error("ELEMNT: expected > or , in subscript, got BRTYPE=%d", BRTYPE);
                break;
            }
        }
        break;
    }
    case NSTTYP: {
        FORWRD();
        CMPND_t *first = EXPR();
        if (BRTYPE == CMATYP && !g_error) {
            atom = cmpnd_new(SELTYP, "SELECT", -1);
            cmpnd_add(atom, first);
            while (BRTYPE == CMATYP && !g_error) {
                FORWRD();
                FORWRD();
                cmpnd_add(atom, EXPR());
            }
        } else {
            atom = first;
        }
        FORWRD();
        BRTYPE = RPTYP;
        break;
    }
    default:
        sil_error("ELEMNT: unknown element STYPE=%d", elem_stype);
        return NULL;
    }
    if (unary_tail) {
        cmpnd_add(unary_tail, atom);
        atom = unary_chain;
    }
    return atom;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static CMPND_t *expr_prec(int min_prec);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CMPND_t *EXPR(void) {
    if (g_error) return NULL;
    spec_t saved = TEXTSP;
    CMPND_t *left = ELEMNT();
    if (!left || g_error) {
        TEXTSP = saved;
        return cmpnd_new(0, "NULL", -1);
    }
    return expr_prec_continue(left, 0);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static CMPND_t *EXPR1(void) {
    return EXPR();
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static CMPND_t *expr_prec_continue(CMPND_t *left, int min_prec);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static CMPND_t *expr_prec(int min_prec) {
    CMPND_t *left = ELEMNT();
    if (!left || g_error) return cmpnd_new(0, "NULL", -1);
    return expr_prec_continue(left, min_prec);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static CMPND_t *expr_prec_continue(CMPND_t *left, int min_prec) {
#define CATFN_PREC 10
    for (;;) {
        if (TEXTSP.len == 0 && BRTYPE == RBTYP) {
            FORWRD();
            if (!(TEXTSP.len > 0 && *TEXTSP.ptr == '[')) {
                break;
            }
        }
        if (TEXTSP.len > 0 && *TEXTSP.ptr == '[') {
            TEXTSP.ptr++; TEXTSP.len--;
            CMPND_t *idx = cmpnd_new(ARYTYP, "IDX", -1);
            cmpnd_add(idx, left);
            FORWRD();
            while (!g_error) {
                if (BRTYPE == RBTYP) break;
                if (BRTYPE == CMATYP) {
                    cmpnd_add(idx, cmpnd_new(0, "NULL", -1));
                    FORWRD();
                    continue;
                }
                CMPND_t *sub = EXPR();
                cmpnd_add(idx, sub);
                if (BRTYPE == EQTYP) { FORWRD(); FORWRD(); continue; }
                FORWRD();
                if (BRTYPE == RBTYP) break;
                if (BRTYPE == CMATYP) {
                    FORWRD();
                    continue;
                }
                sil_error("ELEMNT: expected ] or , in [] subscript, got BRTYPE=%d", BRTYPE);
                break;
            }
            left = idx;
            continue;
        }
        if (TEXTSP.len == 0) {
            if (CATFN_PREC < min_prec) break;
            FORBLK();
            if (BRTYPE == EOSTYP || TEXTSP.len == 0) break;
            spec_t saved_cont = TEXTSP;
            spec_t op_tok2;
            stream_ret_t or2_ = stream(&op_tok2, &TEXTSP, &BIOPTB);
            int cont_op = 0;
            if ((or2_ == ST_STOP || or2_ == ST_EOS) && STYPE != 0) cont_op = STYPE;
            if (cont_op && cont_op != CATFN) {
                int prec = op_prec(cont_op);
                if (prec < min_prec) { TEXTSP = saved_cont; break; }
                int next_min2 = (cont_op == NAMFN || cont_op == DOLFN)
                    ? 99 : op_right_assoc(cont_op) ? prec : prec + 1;
                FORWRD();
                CMPND_t *right = expr_prec(next_min2);
                CMPND_t *binop = cmpnd_new(cont_op, fn_name(cont_op), -1);
                if ((cont_op == NAMFN || cont_op == DOLFN)
                        && left->stype == CATFN && left->n >= 2) {
                    CMPND_t *new_cat = cmpnd_new(CATFN, "CAT", -1);
                    for (int _ci = 0; _ci < left->n - 1; _ci++)
                        cmpnd_add(new_cat, left->c[_ci]);
                    CMPND_t *last2 = left->c[left->n - 1];
                    cmpnd_add(binop, last2);
                    cmpnd_add(binop, right);
                    left = new_cat;
                    cmpnd_add(left, binop);
                } else {
                    cmpnd_add(binop, left);
                    cmpnd_add(binop, right);
                    left = binop;
                }
            } else {
                TEXTSP = saved_cont;
                CMPND_t *right = expr_prec(CATFN_PREC + 1);
                if (!right) break;
                if (left->stype == CATFN) {
                    cmpnd_add(left, right);
                } else {
                    CMPND_t *cat = cmpnd_new(CATFN, "CAT", -1);
                    cmpnd_add(cat, left);
                    cmpnd_add(cat, right);
                    left = cat;
                }
            }
            continue;
        }
        spec_t saved = TEXTSP;
        int op = BINOP();
        if (!op) {
            if (BRTYPE == EQTYP && 0 >= min_prec && g_in_replacement) {
                FORWRD();
                FORWRD();
                CMPND_t *rhs = expr_prec(0);
                CMPND_t *asgn = cmpnd_new(BIEQFN, "BIEQFN(=)", -1);
                cmpnd_add(asgn, left);
                cmpnd_add(asgn, rhs);
                left = asgn;
                continue;
            }
            TEXTSP = saved; break;
        }
        if (op == CATFN) {
            if (CATFN_PREC < min_prec) { TEXTSP = saved; break; }
            CMPND_t *right = expr_prec(CATFN_PREC + 1);
            if (!right) { TEXTSP = saved; break; }
            if (left->stype == CATFN) {
                cmpnd_add(left, right);
            } else {
                CMPND_t *cat = cmpnd_new(CATFN, "CAT", -1);
                cmpnd_add(cat, left);
                cmpnd_add(cat, right);
                left = cat;
            }
            continue;
        }
        int prec  = op_prec(op);
        if (prec < min_prec) { TEXTSP = saved; break; }
        int next_min = (op == NAMFN || op == DOLFN)
            ? 99
            : op_right_assoc(op) ? prec : prec + 1;
        FORWRD();
        CMPND_t *right = expr_prec(next_min);
        CMPND_t *binop = cmpnd_new(op, fn_name(op), -1);
        if ((op == NAMFN || op == DOLFN) && left->stype == CATFN && left->n >= 2) {
            CMPND_t *new_cat  = cmpnd_new(CATFN, "CAT", -1);
            for (int _ci = 0; _ci < left->n - 1; _ci++)
                cmpnd_add(new_cat, left->c[_ci]);
            CMPND_t *last = left->c[left->n - 1];
            cmpnd_add(binop, last);
            cmpnd_add(binop, right);
            left = new_cat;
            cmpnd_add(left, binop);
        } else {
            cmpnd_add(binop, left);
            cmpnd_add(binop, right);
            left = binop;
        }
    }
    BRTYPE = STYPE;
    return left;
}
typedef struct CMPILE_t CMPILE_t;
struct CMPILE_t {
    char *label;
    CMPND_t *subject;
    CMPND_t *pattern;
    CMPND_t *replacement;
    int   has_eq;
    int   is_scan;
    char *go_s;
    char *go_f;
    char *go_u;
    int   is_end;
    CMPILE_t *next;
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static char *xsp_dup(void) {
    return strndup(XSP.ptr, XSP.len);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static CMPILE_t *CMPILE(void) {
    if (g_error) return NULL;
    CMPILE_t *s = calloc(1, sizeof *s);
    {
        stream_ret_t lr = stream(&XSP, &TEXTSP, &LBLTB);
        if (lr == ST_ERROR) { sil_error("CMPILE: label error"); return s; }
    }
    if (XSP.len > 0)
        s->label = xsp_dup();
    FORBLK();
    if (BRTYPE == 0 || BRTYPE == EOSTYP) {
        return s;
    }
    if (BRTYPE == CLNTYP) goto CMPGO;
    if (BRTYPE == EQTYP) goto CMPFRM;
    if (BRTYPE != EOSTYP) {
        s->subject = ELEMNT();
        if (g_error) return s;
        while (!g_error) {
            if (TEXTSP.len == 0 && BRTYPE == RBTYP) {
                FORWRD();
            }
            if (!(TEXTSP.len > 0 && *TEXTSP.ptr == '['))
                break;
            TEXTSP.ptr++; TEXTSP.len--;
            CMPND_t *idx = cmpnd_new(ARYTYP, "IDX", -1);
            cmpnd_add(idx, s->subject);
            FORWRD();
            while (!g_error) {
                if (BRTYPE == RBTYP) break;
                if (BRTYPE == CMATYP) {
                    cmpnd_add(idx, cmpnd_new(0, "NULL", -1));
                    FORWRD();
                    continue;
                }
                CMPND_t *sub = EXPR();
                cmpnd_add(idx, sub);
                if (BRTYPE == EQTYP) { FORWRD(); FORWRD(); continue; }
                FORWRD();
                if (BRTYPE == RBTYP) break;
                if (BRTYPE == CMATYP) { FORWRD(); continue; }
                sil_error("ELEMNT: expected ] in chained subscript, got BRTYPE=%d", BRTYPE);
                break;
            }
            s->subject = idx;
        }
        if (s->subject && s->subject->stype == VARTYP &&
            strcmp(s->subject->text, "END") == 0) {
            s->is_end = 1;
            return s;
        }
        FORBLK();
        if (BRTYPE == EQTYP) goto CMPFRM;
        if (BRTYPE == CLNTYP) goto CMPGO;
        if (BRTYPE == EOSTYP) return s;
        if (BRTYPE == NBTYP) {
            if (TEXTSP.len >= 1 && TEXTSP.ptr[0] == '?' &&
                (TEXTSP.len == 1 || TEXTSP.ptr[1] == ' ' || TEXTSP.ptr[1] == '\t')) {
                TEXTSP.ptr++; TEXTSP.len--;
                s->is_scan = 1;
                FORBLK();
                if (BRTYPE == EOSTYP || BRTYPE == 0) return s;
                if (BRTYPE == CLNTYP) goto CMPGO;
            } else {
            }
        }
        s->pattern = EXPR();
        if (g_error) return s;
        FORBLK();
        if (BRTYPE == EQTYP) goto CMPASP;
        if (BRTYPE == CLNTYP) goto CMPGO;
        return s;
    }
    return s;
CMPFRM:
    s->has_eq = 1;
    FORWRD();
    if (BRTYPE == EOSTYP || BRTYPE == 0) return s;
    if (BRTYPE == CLNTYP) goto CMPGO;
    g_in_replacement = 1; s->replacement = EXPR(); g_in_replacement = 0;
    if (g_error) return s;
    FORBLK();
    if (BRTYPE == CLNTYP) goto CMPGO;
    return s;
CMPASP:
    s->has_eq = 1;
    FORWRD();
    if (BRTYPE == EOSTYP || BRTYPE == 0) return s;
    if (BRTYPE == CLNTYP) goto CMPGO;
    g_in_replacement = 1; s->replacement = EXPR(); g_in_replacement = 0;
    if (g_error) return s;
    FORBLK();
    if (BRTYPE == CLNTYP) goto CMPGO;
    return s;
CMPGO: {
    stream_ret_t r = stream(&XSP, &TEXTSP, &GOTOTB);
    if (r == ST_ERROR) { sil_error("CMPGO: bad goto"); return s; }
    if (r == ST_EOS)   { sil_error("CMPGO: truncated goto"); return s; }
    int gotype = STYPE;
    if (gotype == UGOTYP || gotype == UTOTYP) {
        if (gotype == UTOTYP) FORWRD();
        CMPND_t *lbl = EXPR();
        s->go_u = lbl ? strdup(lbl->text ? lbl->text : "") : NULL;
        FORWRD();
        return s;
    }
    if (gotype == SGOTYP || gotype == STOTYP) {
        FORWRD();
        CMPND_t *lbl = EXPR();
        s->go_s = lbl ? strdup(lbl->text ? lbl->text : "") : NULL;
        FORWRD();
        if (BRTYPE != EOSTYP && BRTYPE != 0 && TEXTSP.len > 0) {
            spec_t saved2 = TEXTSP;
            if (TEXTSP.ptr[0] == ':') {
                TEXTSP.ptr++; TEXTSP.len--;
            }
            stream_ret_t gr = stream(&XSP, &TEXTSP, &GOTOTB);
            if (gr != ST_ERROR && (STYPE == FGOTYP || STYPE == FTOTYP)) {
                if (STYPE == FTOTYP) FORWRD();
                CMPND_t *fl = EXPR();
                s->go_f = fl ? strdup(fl->text ? fl->text : "") : NULL;
                FORWRD();
            } else {
                TEXTSP = saved2;
            }
        }
        return s;
    }
    if (gotype == FGOTYP || gotype == FTOTYP) {
        FORWRD();
        CMPND_t *lbl = EXPR();
        s->go_f = lbl ? strdup(lbl->text ? lbl->text : "") : NULL;
        FORWRD();
        if (BRTYPE != EOSTYP && BRTYPE != 0 && TEXTSP.len > 0) {
            spec_t saved2 = TEXTSP;
            if (TEXTSP.ptr[0] == ':') {
                TEXTSP.ptr++; TEXTSP.len--;
            }
            stream_ret_t gr = stream(&XSP, &TEXTSP, &GOTOTB);
            if (gr != ST_ERROR && (STYPE == SGOTYP || STYPE == STOTYP)) {
                if (STYPE == STOTYP) FORWRD();
                CMPND_t *sl = EXPR();
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
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void cmpnd_print_sexp(CMPND_t *n, FILE *out, int oneline, int depth) {
    if (!n) { fprintf(out, oneline ? "(NULL)" : "%*s(NULL)\n", oneline ? 0 : depth*2, ""); return; }
    if (!oneline) fprintf(out, "%*s", depth*2, "");
    fprintf(out, "(%s", stype_name(n->stype));
    if (n->text && n->text[0])
        fprintf(out, " \"%s\"", n->text);
    if (n->stype == ILITYP)
        fprintf(out, " ival=%lld", n->v.ival);
    if (n->stype == FLITYP)
        fprintf(out, " fval=%g", n->fval);
    if (n->n == 0) {
        fprintf(out, oneline ? ")" : ")\n");
    } else {
        if (!oneline) fprintf(out, "\n");
        for (int i = 0; i < n->n; i++) {
            if (oneline && i > 0) fprintf(out, " ");
            cmpnd_print_sexp(n->c[i], out, oneline, depth+1);
        }
        if (!oneline) fprintf(out, "%*s)\n", depth*2, "");
        else          fprintf(out, ")");
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void cmpile_print(CMPILE_t *s, FILE *out, int oneline, int idx) {
    if (oneline) {
        fprintf(out, "(STMT %d", idx);
        if (s->is_end)  { fprintf(out, " END)\n"); return; }
        if (s->label)     fprintf(out, " :label \"%s\"", s->label);
        if (s->subject)   { fprintf(out, " :subj "); cmpnd_print_sexp(s->subject,  out, 1, 0); }
        if (s->is_scan)   fprintf(out, " :op ?");
        if (s->pattern)   { fprintf(out, " :pat ");  cmpnd_print_sexp(s->pattern,  out, 1, 0); }
        if (s->has_eq && s->replacement)
                          { fprintf(out, " :repl "); cmpnd_print_sexp(s->replacement, out, 1, 0); }
        if (s->go_s)      fprintf(out, " :S(%s)", s->go_s);
        if (s->go_f)      fprintf(out, " :F(%s)", s->go_f);
        if (s->go_u)      fprintf(out, " :(%s)",  s->go_u);
        fprintf(out, ")\n");
        return;
    }
    fprintf(out, "=== stmt %d ===\n", idx);
    if (s->is_end)      { fprintf(out, "  END\n"); return; }
    if (s->label)         fprintf(out, "  label:   %s\n", s->label);
    if (s->subject)     { fprintf(out, "  subject:\n"); cmpnd_print_sexp(s->subject,  out, 0, 2); }
    if (s->is_scan)       fprintf(out, "  op:      ?\n");
    if (s->pattern)     { fprintf(out, "  pattern:\n"); cmpnd_print_sexp(s->pattern,  out, 0, 2); }
    if (s->has_eq && s->replacement)
                        { fprintf(out, "  replace:\n"); cmpnd_print_sexp(s->replacement, out, 0, 2); }
    if (s->go_s)          fprintf(out, "  :S(%s)\n", s->go_s);
    if (s->go_f)          fprintf(out, "  :F(%s)\n", s->go_f);
    if (s->go_u)          fprintf(out, "  :(%s)\n",  s->go_u);
}
typedef struct {
    CMPILE_t  *head;
    CMPILE_t  *tail;
    int    stmt_idx;
    int    done;
} compile_state_t;
static compile_state_t *g_cst = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void compile_one_stmt(void) {
    if (!g_cst || g_cst->done) return;
    g_error = 0;
    STYPE = 0; BRTYPE = 0;
    CMPILE_t *s = CMPILE();
    if (!s) {
        if (g_error)
            fprintf(stderr, "line %d: %s\n", g_stmt_lineno, g_errmsg);
        return;
    }
    s->next = NULL;
    if (!g_cst->head) g_cst->head = s; else g_cst->tail->next = s;
    g_cst->tail = s;
    g_cst->stmt_idx++;
    if (g_error)
        fprintf(stderr, "line %d: %s\n", g_stmt_lineno, g_errmsg);
    if (s->is_end) g_cst->done = 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int forrun(void) {
    if (g_pending_len > 0) return 1;
retry:
    if (!g_io_file || g_io_eof) return 1;
    char rawline[IO_LINEBUF_SZ];
    int len = io_read_raw(g_io_file, rawline, sizeof rawline);
    if (len < 0) {
        g_io_eof = 1;
        return 1;
    }
    g_io_lineno++;
    if (len == 0) goto retry;
    spec_t card = { rawline, len };
    spec_t tok;
    stream(&tok, &card, &CARDTB);
    int ctype = STYPE;
    switch (ctype) {
    case CMTTYP:
        goto retry;
    case CTLTYP: {
        const char *p = rawline + 1;
        while (*p == '-') p++;
        const char *name_start = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        int namelen = (int)(p - name_start);
        if (namelen == 7 && strncmp(name_start, "INCLUDE", 7) == 0) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p) {
                char *incpath = resolve_include_path(g_io_path, p);
                FILE *incf = fopen(incpath, "r");
                if (!incf) { char *cr = strchr(incpath,'\r'); if(cr)*cr='\0'; incf=fopen(incpath,"r"); }
                if (incf) {
                    FILE *save_file = g_io_file;
                    const char *save_path = g_io_path;
                    int save_lineno = g_io_lineno;
                    int save_eof    = g_io_eof;
                    int save_depth  = g_io_depth;
                    g_io_file = incf; g_io_path = incpath;
                    g_io_lineno = 0; g_io_eof = 0; g_io_depth++;
                    while (!g_io_eof && !(g_cst && g_cst->done)) {
                        int fr2 = forrun();
                        if (fr2 == 2) {
                        } else {
                            if (g_pending_len > 0) {
                                memcpy(g_io_linebuf, g_pending_buf, g_pending_len);
                                g_io_linebuf[g_pending_len] = '\0';
                                TEXTSP.ptr = g_io_linebuf;
                                TEXTSP.len = g_pending_len;
                                g_stmt_lineno = g_pending_lineno;
                                g_pending_len = 0;
                                compile_one_stmt();
                            }
                            if (g_io_eof) break;
                        }
                    }
                    fclose(incf);
                    free(incpath);
                    g_io_file = save_file; g_io_path = save_path;
                    g_io_lineno = save_lineno; g_io_eof = save_eof;
                    g_io_depth = save_depth;
                } else {
                    sil_error("sno4parse: include not found: %s", incpath);
                    free(incpath);
                }
            }
        }
        goto retry;
    }
    case CNTTYP: {
        const char *p = rawline;
        if (*p == '+') p++;
        int clen = len - (int)(p - rawline);
        memcpy(g_io_linebuf, p, clen);
        g_io_linebuf[clen] = '\0';
        TEXTSP.ptr = g_io_linebuf;
        TEXTSP.len = clen;
        return 2;
    }
    default:
        memcpy(g_pending_buf, rawline, len);
        g_pending_buf[len] = '\0';
        g_pending_len = len;
        g_pending_lineno = g_io_lineno;
        return 1;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static char *resolve_include_path(const char *base_path, const char *incname) {
    char name[1024];
    int nlen = strlen(incname);
    if (nlen >= 2 && (incname[0]=='\'' || incname[0]=='"') &&
        incname[nlen-1] == incname[0]) {
        nlen -= 2;
        memcpy(name, incname+1, nlen);
        name[nlen] = '\0';
    } else {
        memcpy(name, incname, nlen+1);
    }
    { int l = strlen(name); while (l > 0 && name[l-1]=='\r') name[--l]='\0'; }
    if (name[0] == '/') return strdup(name);
    char base[4096];
    strncpy(base, base_path ? base_path : ".", sizeof(base)-1);
    base[sizeof(base)-1] = '\0';
    char *slash = strrchr(base, '/');
    if (slash) { *slash = '\0'; }
    else        { strcpy(base, "."); }
    char result[4096];
    snprintf(result, sizeof result, "%s/%s", base, name);
    if (access(result, R_OK) == 0) return strdup(result);
    extern const char *g_include_paths[];
    extern int         g_num_include_paths;
    for (int i = 0; i < g_num_include_paths; i++) {
        snprintf(result, sizeof result, "%s/%s", g_include_paths[i], name);
        if (access(result, R_OK) == 0) return strdup(result);
    }
    snprintf(result, sizeof result, "%s/%s", base, name);
    return strdup(result);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void cmpile_file_internal(FILE *f, const char *base_path, compile_state_t *st) {
    g_error     = 0;
    g_io_file   = f;
    g_io_path   = base_path;
    g_io_lineno = 0;
    g_io_eof    = 0;
    g_cst       = st;
    if (g_pending_len > 0) {
        memcpy(g_io_linebuf, g_pending_buf, g_pending_len);
        g_io_linebuf[g_pending_len] = '\0';
        TEXTSP.ptr = g_io_linebuf;
        TEXTSP.len = g_pending_len;
        g_stmt_lineno = g_pending_lineno;
        g_pending_len = 0;
        compile_one_stmt();
    }
    char rawline[IO_LINEBUF_SZ];
    while (!st->done && !g_io_eof) {
        int len = io_read_raw(f, rawline, sizeof rawline);
        if (len < 0) break;
        g_io_lineno++;
        if (len == 0) continue;
        spec_t card = { rawline, len };
        spec_t tok;
        stream(&tok, &card, &CARDTB);
        int ctype = STYPE;
        if (ctype == CMTTYP) continue;
        if (ctype == CTLTYP) {
            const char *p = rawline + 1;
            while (*p == '-') p++;
            const char *ns = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            int nlen = (int)(p - ns);
            if (nlen == 7 && strncmp(ns, "INCLUDE", 7) == 0) {
                while (*p == ' ' || *p == '\t') p++;
                if (*p) {
                    char *incpath = resolve_include_path(base_path, p);
                    FILE *incf = fopen(incpath, "r");
                    if (!incf) { char *cr=strchr(incpath,'\r'); if(cr)*cr='\0'; incf=fopen(incpath,"r"); }
                    if (incf) {
                        FILE *sv_f=g_io_file; const char *sv_p=g_io_path;
                        int sv_ln=g_io_lineno, sv_eof=g_io_eof, sv_dep=g_io_depth;
                        g_io_depth++;
                        cmpile_file_internal(incf, incpath, st);
                        fclose(incf); free(incpath);
                        g_io_file=sv_f; g_io_path=sv_p;
                        g_io_lineno=sv_ln; g_io_eof=sv_eof; g_io_depth=sv_dep;
                    } else {
                        sil_error("sno4parse: include not found: %s", incpath);
                        free(incpath);
                    }
                }
            }
            continue;
        }
        if (ctype == CNTTYP) continue;
        memcpy(g_io_linebuf, rawline, len);
        g_io_linebuf[len] = '\0';
        TEXTSP.ptr = g_io_linebuf;
        TEXTSP.len = len;
        g_stmt_lineno = g_io_lineno;
        compile_one_stmt();
        while (!st->done && BRTYPE == EOSTYP && TEXTSP.len > 0) {
            while (TEXTSP.len > 0 && (*TEXTSP.ptr == ' ' || *TEXTSP.ptr == '\t')) {
                TEXTSP.ptr++; TEXTSP.len--;
            }
            if (TEXTSP.len == 0) break;
            if (*TEXTSP.ptr == '*') break;       /* ;* inline comment — rest is comment */
            int rem = (int)TEXTSP.len;
            const char *src = TEXTSP.ptr;
            if (rem + 2 <= (int)sizeof(g_io_linebuf)) {
                g_io_linebuf[0] = ' ';
                memcpy(g_io_linebuf + 1, src, rem);
                g_io_linebuf[rem + 1] = '\0';
                TEXTSP.ptr = g_io_linebuf;
                TEXTSP.len = rem + 1;
                compile_one_stmt();
            } else break;
        }
        while (!st->done && g_pending_len > 0) {
            memcpy(g_io_linebuf, g_pending_buf, g_pending_len);
            g_io_linebuf[g_pending_len] = '\0';
            TEXTSP.ptr = g_io_linebuf;
            TEXTSP.len = g_pending_len;
            g_stmt_lineno = g_pending_lineno;
            g_pending_len = 0;
            compile_one_stmt();
        }
    }
    while (!st->done && g_pending_len > 0) {
        memcpy(g_io_linebuf, g_pending_buf, g_pending_len);
        g_io_linebuf[g_pending_len] = '\0';
        TEXTSP.ptr = g_io_linebuf;
        TEXTSP.len = g_pending_len;
        g_stmt_lineno = g_pending_lineno;
        g_pending_len = 0;
        compile_one_stmt();
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void cmpile_init(void) {
    init_tables();
    g_trace_stream = (getenv("SNO_TRACE") != NULL);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void cmpile_add_include(const char *path) {
    if (path && g_num_include_paths < MAX_INCLUDE_PATHS)
        g_include_paths[g_num_include_paths++] = path;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CMPILE_t *cmpile_file(FILE *f, const char *base_path) {
    compile_state_t st;
    memset(&st, 0, sizeof st);
    cmpile_file_internal(f, base_path, &st);
    return st.head;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CMPILE_t *cmpile_string(const char *src) {
    FILE *f = fmemopen((void *)src, strlen(src), "r");
    if (!f) return NULL;
    CMPILE_t *result = cmpile_file(f, NULL);
    fclose(f);
    return result;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void cmpile_free(CMPILE_t *s) {
    while (s) {
        CMPILE_t *next = s->next;
        free(s->label);
        free(s->go_s); free(s->go_f); free(s->go_u);
        free(s);
        s = next;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CMPND_t *cmpile_eval_expr(const char *src) {
    if (!src || !*src) return NULL;
    init_tables();
    g_error = 0;
    TEXTSP.ptr = src; TEXTSP.len = (int)strlen(src);
    XSP.ptr    = src; XSP.len   = 0;
    BRTYPE = 0; STYPE = 0;
    FORWRD();
    if (g_error) return NULL;
    CMPND_t *n = EXPR();
    if (g_error) return NULL;
    return n;
}
