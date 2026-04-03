#ifndef SNOBOL4_LEX_H
#define SNOBOL4_LEX_H

#include <stdio.h>

/* Token kinds — named after SPITBOL manual Chapter 15 operator names.
 *
 * Binary ops (WHITE op WHITE):   space on both sides distinguishes from unary.
 * Unary ops  (no leading space): directly prefix an atom or sub-expression.
 *
 * Unary operator names (p.181-182):
 *   @  at-sign         ~ tilde        ? question-mark  & ampersand
 *   +  plus            - minus        * asterisk        $ dollar-sign
 *   .  period
 *   User-definable: ! exclamation  % percent  / slash  # pound  = equal  | vertical-bar
 *
 * Binary operator names (p.182-183):
 *   =  assignment      ? match        | alternation     (space) concatenation
 *   +  addition        - subtraction  / division        * multiplication
 *   ^ ! ** exponentiation             $ immediate-assign  . conditional-assign
 *   User-definable: & ampersand  @ at-sign  # pound  % percent  ~ tilde      */

typedef enum {
    /* Atoms */
    T_IDENT,            /* plain variable reference                           */
    T_FUNCTION,         /* IDENT immediately followed by '(' — function call  */
    T_KEYWORD,          /* &NAME keyword reference                            */
    T_END,              /* END identifier                                     */
    T_INT,              /* integer literal                                    */
    T_REAL,             /* real/float literal                                 */
    T_STR,              /* quoted string literal                              */
    /* Binary operators (WHITE op WHITE) */
    T_ASSIGNMENT,       /* =   assignment                                     */
    T_MATCH,            /* ?   pattern match (binary)                         */
    T_ALTERNATION,      /* |   pattern alternation                            */
    T_ADDITION,         /* +   addition                                       */
    T_SUBTRACTION,      /* -   subtraction                                    */
    T_MULTIPLICATION,   /* *   multiplication                                 */
    T_DIVISION,         /* /   division                                       */
    T_EXPONENTIATION,   /* ^ ! **  exponentiation                             */
    T_IMMEDIATE_ASSIGN, /* $   immediate assignment (binary capture)          */
    T_COND_ASSIGN,      /* .   conditional assignment (binary capture)        */
    T_CONCAT,           /* @   cursor-capture binary / user-definable         */
    T_AMPERSAND,        /* &   opsyn / user-definable binary                  */
    T_AT_SIGN,          /* @   user-definable binary                          */
    T_POUND,            /* #   user-definable binary                          */
    T_PERCENT,          /* %   user-definable binary                          */
    T_TILDE,            /* ~   user-definable binary / cond-capture           */
    /* Unary operators (no preceding space) */
    T_UN_AT_SIGN,       /* @   at-sign:  assigns cursor position              */
    T_UN_TILDE,         /* ~   tilde:    negates failure or success           */
    T_UN_QUESTION_MARK, /* ?   question-mark: interrogation                   */
    T_UN_AMPERSAND,     /* &   ampersand: keyword prefix                      */
    T_UN_PLUS,          /* +   plus:     positive numeric coerce              */
    T_UN_MINUS,         /* -   minus:    negate numeric                       */
    T_UN_ASTERISK,      /* *   asterisk: defers evaluation                    */
    T_UN_DOLLAR_SIGN,   /* $   dollar-sign: indirection                       */
    T_UN_PERIOD,        /* .   period:   name operator                        */
    T_UN_EXCLAMATION,   /* !   exclamation: user-definable unary              */
    T_UN_PERCENT,       /* %   percent:  user-definable unary                 */
    T_UN_SLASH,         /* /   slash:    user-definable unary                 */
    T_UN_POUND,         /* #   pound:    user-definable unary                 */
    T_UN_EQUAL,         /* =   equal:    user-definable unary                 */
    T_UN_VERTICAL_BAR,  /* |   vertical-bar: user-definable unary             */
    /* Structural */
    T_CONCAT,           /* whitespace between two atoms — concatenation    */
    T_COMMA,
    T_LPAREN,
    T_RPAREN,
    T_LBRACK,           /* [                                                  */
    T_RBRACK,           /* ]                                                  */
    T_LANGLE,           /* <                                                  */
    T_RANGLE,           /* >                                                  */
    /* Statement structure (one-pass lexer) */
    T_LABEL,            /* col-1 identifier; sval=name, ival=1 if END label  */
    T_GOTO,             /* goto field raw text                                */
    T_STMT_END,         /* logical-line boundary                              */
    T_EOF,
    T_ERR,
    T_WS                /* never returned; kept so old references compile     */
} TokKind;

typedef struct {
    TokKind     kind;
    const char *sval;
    long        ival;
    double      dval;
    int         lineno;
} Token;

typedef struct Lex {
    int    lineno;
    Token  peek;
    int    peeked;
    void  *_scanner;
    void  *_extra;
} Lex;

void  lex_open_str(Lex *lx, const char *s, int len, int lineno);
Token lex_next    (Lex *lx);
Token lex_peek    (Lex *lx);
int   lex_at_end  (Lex *lx);
void  lex_destroy (Lex *lx);

void  flex_lex_open   (Lex *lx, FILE *f, const char *fname);
Token flex_lex_next   (Lex *lx);
void  flex_lex_destroy(Lex *lx);

typedef struct SnoLine {
    char *label; char *body; char *goto_str; int lineno; int is_end;
} SnoLine;

typedef struct { SnoLine *a; int n, cap; } LineArray;

#endif /* SNOBOL4_LEX_H */
