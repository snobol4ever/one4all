%{
/*
 * lex.l — SNOBOL4 body tokeniser (M-LEX-1)
 *
 * Replaces raw_next_str() in lex.c for the lex_open_str / body-parsing path.
 * Compiled to lex.yy.c; object linked alongside lex.c.
 *
 * Interface to lex.c (declared in lex_flex.h, included by lex.c):
 *   void  flex_lex_open   (Lex *lx)  — init scanner from lx->src/len/lineno
 *   Token flex_lex_next   (Lex *lx)  — return next Token
 *   void  flex_lex_destroy(Lex *lx)  — release scanner resources
 *
 * Token contract — matches CSNOBOL4 syntax.tbl exactly:
 *   SQLITB/DQLITB: FOR(SQUOTE/DQUOTE) STOP; ELSE CONTIN — no doubled-quote escape
 *   INTGTB: digits, then optional DOT+digits or EXP — no leading-dot real
 *   VARTB:  ALPHANUMERIC+BREAK+UNI continues; LEFTPAREN→func; LEFTBR→array
 *   UNOPTB: all 14 unary operator characters
 *   ** before * (longest match)
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "scrip_cc.h"
#include "lex.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    int         lineno;
    const char *sval;   /* scratch for current token sval */
    long        ival;   /* scratch for current token ival */
    double      dval;   /* scratch for current token dval */
} FlexExtra;
%}

%option reentrant noyywrap nounput noinput
%option extra-type="FlexExtra *"

DIGIT    [0-9]
ALPHA    [A-Za-z]
ALNUM    [A-Za-z0-9_]
UNI      [\x80-\xFF]
IDSTART  ({ALPHA}|{UNI})
IDCONT   ({ALNUM}|"."|{UNI})
EXP      [eE][+-]?{DIGIT}+

%%

[ \t]+   { return T_WS; }

\'[^']*\'  {
    /* single-quoted string — SQLITB: scan to first SQUOTE, stop */
    int len = yyleng - 2;
    yyextra->sval = intern_n(yytext + 1, len);
    return T_STR;
}

\"[^"]*\"  {
    /* double-quoted string — DQLITB: scan to first DQUOTE, stop */
    int len = yyleng - 2;
    yyextra->sval = intern_n(yytext + 1, len);
    return T_STR;
}

{DIGIT}+"."{DIGIT}+({EXP})?  {
    /* float with decimal point — INTGTB: DOT then FLITB */
    yyextra->dval = atof(yytext);
    return T_REAL;
}

{DIGIT}+{EXP}  {
    /* float with exponent only — INTGTB: EXP branch */
    yyextra->dval = atof(yytext);
    return T_REAL;
}

{DIGIT}+  {
    /* integer — INTGTB: TERMINATOR stop */
    yyextra->ival = atol(yytext);
    return T_INT;
}

"&"{ALNUM}+  {
    /* &KEYWORD — strip & prefix */
    yyextra->sval = intern_n(yytext + 1, yyleng - 1);
    return T_KEYWORD;
}

"&"  { return T_AMP; }

{IDSTART}{IDCONT}*  {
    /* identifier or END — VARTB */
    yyextra->sval = intern_n(yytext, yyleng);
    if (strcasecmp(yyextra->sval, "END") == 0) return T_END;
    return T_IDENT;
}

"**"  { return T_STARSTAR; }
"+"   { return T_PLUS;     }
"-"   { return T_MINUS;    }
"*"   { return T_STAR;     }
"/"   { return T_SLASH;    }
"%"   { return T_PCT;      }
"^"   { return T_CARET;    }
"!"   { return T_BANG;     }
"@"   { return T_AT;       }
"~"   { return T_TILDE;    }
"$"   { return T_DOLLAR;   }
"."   { return T_DOT;      }
"#"   { return T_HASH;     }
"|"   { return T_PIPE;     }
"="   { return T_EQ;       }
"?"   { return T_QMARK;    }
","   { return T_COMMA;    }
"("   { return T_LPAREN;   }
")"   { return T_RPAREN;   }
"["   { return T_LBRACKET; }
"]"   { return T_RBRACKET; }
"<"   { return T_LANGLE;   }
">"   { return T_RANGLE;   }

.  {
    snoc_error(yyextra->lineno, "unexpected character '%c'", yytext[0]);
    return T_ERR;
}

<<EOF>>  { return T_EOF; }

%%

/*
 * ── Public interface called by lex.c ────────────────────────────────────
 *
 * lex.c guards calls with (lx->src != NULL).
 * These three functions replace raw_next_str() entirely.
 */

void flex_lex_open(Lex *lx) {
    FlexExtra *ex = calloc(1, sizeof(FlexExtra));
    ex->lineno = lx->lineno;
    yyscan_t sc;
    yylex_init_extra(ex, &sc);
    yy_scan_bytes(lx->src, lx->len, sc);
    lx->_scanner = sc;
    lx->_extra   = ex;
}

Token flex_lex_next(Lex *lx) {
    FlexExtra *ex = (FlexExtra *)lx->_extra;
    /* reset scratch */
    ex->sval = NULL; ex->ival = 0; ex->dval = 0.0;
    int kind = yylex((yyscan_t)lx->_scanner);
    Token t;
    t.kind   = (TokKind)kind;
    t.sval   = ex->sval;
    t.ival   = ex->ival;
    t.dval   = ex->dval;
    t.lineno = ex->lineno;
    return t;
}

void flex_lex_destroy(Lex *lx) {
    if (lx->_scanner) {
        free(lx->_extra);
        yylex_destroy((yyscan_t)lx->_scanner);
        lx->_scanner = NULL;
        lx->_extra   = NULL;
    }
}
