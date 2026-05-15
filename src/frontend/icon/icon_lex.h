#ifndef ICON_LEX_H
#define ICON_LEX_H
#include <stddef.h>
typedef enum {
    TK_EOF = 0,
    TK_ERROR,
    TK_INT,
    TK_REAL,
    TK_STRING,
    TK_CSET,
    TK_IDENT,
    TK_PLUS,
    TK_MINUS,
    TK_STAR,       /* * */
    TK_SLASH,
    TK_MOD,
    TK_CARET,
    TK_LT,
    TK_LE,
    TK_GT,
    TK_GE,
    TK_EQ,
    TK_NEQ,
    TK_SLT,
    TK_SLE,
    TK_SGT,
    TK_SGE,
    TK_SEQ,
    TK_SNE,
    TK_CONCAT,
    TK_LCONCAT,
    TK_ASSIGN,
    TK_SWAP,
    TK_REVASSIGN,
    TK_AUGPLUS,
    TK_AUGMINUS,
    TK_AUGSTAR,    /* *:= */
    TK_AUGSLASH,
    TK_AUGMOD,
    TK_AUGPOW,
    TK_AUGCONCAT,
    TK_AUGCSET_UNION,
    TK_AUGCSET_DIFF,
    TK_AUGCSET_INTER, /* **:= */
    TK_AUGSCAN,
    TK_AUGEQ,
    TK_AUGSEQ,
    TK_EQSWAP,
    TK_AUGLT,
    TK_AUGLE,
    TK_AUGGT,
    TK_AUGGE,
    TK_AUGNE,
    TK_AUGSLT,
    TK_AUGSLE,
    TK_AUGSGT,
    TK_AUGSGE,
    TK_AUGSNE,
    TK_VALSWAP,
    TK_IDENTICAL,
    TK_NOTIDENT,
    TK_PLUSCOLON,
    TK_MINUSCOLON,
    TK_PLUSPLUS,
    TK_MINUSMINUS,
    TK_STARSTAR,   /* ** (cset intersection) */
    TK_AND,
    TK_BAR,
    TK_BACKSLASH,
    TK_BANG,
    TK_QMARK,
    TK_AT,
    TK_TILDE,
    TK_DOT,
    TK_TO,
    TK_BY,
    TK_EVERY,
    TK_DO,
    TK_IF,
    TK_THEN,
    TK_ELSE,
    TK_WHILE,
    TK_UNTIL,
    TK_REPEAT,
    TK_RETURN,
    TK_SUSPEND,
    TK_FAIL,
    TK_BREAK,
    TK_NEXT,
    TK_NOT,
    TK_PROCEDURE,
    TK_END,
    TK_GLOBAL,
    TK_LOCAL,
    TK_STATIC,
    TK_RECORD,
    TK_LINK,
    TK_INVOCABLE,
    TK_CASE,
    TK_OF,
    TK_DEFAULT,
    TK_CREATE,
    TK_INITIAL,
    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACE,
    TK_RBRACE,
    TK_LBRACK,
    TK_RBRACK,
    TK_COMMA,
    TK_SEMICOL,
    TK_COLON,
    TK_COUNT
} IcnTkKind;
typedef union {
    long   ival;
    double fval;
    struct {
        char  *data;
        size_t len;
    } sval;
} IcnTkVal;
typedef struct {
    IcnTkKind kind;
    IcnTkVal  val;
    int       line;
    int       col;
} IcnToken;
typedef struct {
    const char *src;
    size_t      src_len;
    size_t      pos;
    int         line;
    int         col;
    char        errmsg[256];
    int         had_error;
} IcnLexer;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void icn_lex_init(IcnLexer *lex, const char *src);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IcnToken icn_lex_next(IcnLexer *lex);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IcnToken icn_lex_peek(IcnLexer *lex);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *icn_tk_name(IcnTkKind kind);
#endif
