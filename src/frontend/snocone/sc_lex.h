/*
 * sc_lex.h — Snocone lexer public API
 *
 * Ported from snobol4jvm/src/SNOBOL4clojure/snocone.clj (Clojure reference).
 * Token kind names match the JVM KIND-* constants exactly (SC_ prefix here).
 *
 * Source model (Snocone language spec / snocone.clj):
 *   - Comments: # to end of line; not in token stream
 *   - Continuation: newline suppressed when last non-ws char is one of:
 *       @ $ % ^ & * ( - + = [ < > | ~ , ? :
 *   - Semicolons terminate statements within a line (same as newline)
 *   - Strings: delimited by matching ' or "
 *   - Numbers: integer or real (optional exponent eEdD+/-); leading-dot (.5) legal
 *   - Identifiers: [A-Za-z_][A-Za-z0-9_]*  reclassified to keyword if match
 *   - Operators: longest-match from op-entries table
 */

#ifndef SC_LEX_H
#define SC_LEX_H

#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Token kinds  (matches JVM KIND-* enum order)
 * ------------------------------------------------------------------------- */
typedef enum {
    /* Literals */
    SC_INTEGER,
    SC_REAL,
    SC_STRING,
    SC_IDENT,

    /* Keywords -- matched as identifiers, then reclassified */
    SC_KW_IF,
    SC_KW_ELSE,
    SC_KW_WHILE,
    SC_KW_DO,
    SC_KW_FOR,
    SC_KW_RETURN,
    SC_KW_FRETURN,
    SC_KW_NRETURN,
    SC_KW_GO,
    SC_KW_TO,
    SC_KW_PROCEDURE,
    SC_KW_STRUCT,

    /* Punctuation */
    SC_LPAREN,
    SC_RPAREN,
    SC_LBRACE,
    SC_RBRACE,
    SC_LBRACKET,
    SC_RBRACKET,
    SC_COMMA,
    SC_SEMICOLON,
    SC_COLON,

    /* Binary operators (precedence low->high, from bconv table in snocone.sc) */
    SC_ASSIGN,       /* =    prec 1/2  -> SNOBOL4 =          */
    SC_QUESTION,     /* ?    prec 2    -> SNOBOL4 ?           */
    SC_PIPE,         /* |    prec 3    -> SNOBOL4 |           */
    SC_OR,           /* ||   prec 4    -> SNOBOL4 (a,b)       */
    SC_CONCAT,       /* &&   prec 5    -> SNOBOL4 blank       */
    SC_EQ,           /* ==   prec 6    -> EQ(a,b)             */
    SC_NE,           /* !=   prec 6    -> NE(a,b)             */
    SC_LT,           /* <    prec 6    -> LT(a,b)             */
    SC_GT,           /* >    prec 6    -> GT(a,b)             */
    SC_LE,           /* <=   prec 6    -> LE(a,b)             */
    SC_GE,           /* >=   prec 6    -> GE(a,b)             */
    SC_STR_IDENT,    /* ::   prec 6    -> IDENT(a,b)          */
    SC_STR_DIFFER,   /* :!:  prec 6    -> DIFFER(a,b)         */
    SC_STR_LT,       /* :<:  prec 6    -> LLT(a,b)            */
    SC_STR_GT,       /* :>:  prec 6    -> LGT(a,b)            */
    SC_STR_LE,       /* :<=: prec 6    -> LLE(a,b)            */
    SC_STR_GE,       /* :>=: prec 6    -> LGE(a,b)            */
    SC_STR_EQ,       /* :==: prec 6    -> LEQ(a,b)            */
    SC_STR_NE,       /* :!=: prec 6    -> LNE(a,b)            */
    SC_PLUS,         /* +    prec 7                           */
    SC_MINUS,        /* -    prec 7                           */
    SC_SLASH,        /* /    prec 8                           */
    SC_STAR,         /* *    prec 8                           */
    SC_PERCENT,      /* %    prec 8    -> REMDR(a,b)          */
    SC_CARET,        /* ^ ** prec 9/10 right-assoc -> **      */
    SC_PERIOD,       /* .    prec 10   -> SNOBOL4 .           */
    SC_DOLLAR,       /* $    prec 10   -> SNOBOL4 $           */

    /* Unary-only operators */
    SC_AT,           /* @                                     */
    SC_AMPERSAND,    /* &                                     */
    SC_TILDE,        /* ~ logical negation                    */

    /* Synthetic */
    SC_NEWLINE,      /* logical end-of-statement              */
    SC_EOF,
    SC_UNKNOWN,
    SC_KW_THEN       /* appended last — never shifts existing values */
} ScKind;

/* ---------------------------------------------------------------------------
 * Token struct
 * ------------------------------------------------------------------------- */
typedef struct {
    ScKind  kind;
    char   *text;   /* heap-allocated, NUL-terminated verbatim source text */
    int     line;   /* 1-based physical line where token began              */
} ScToken;

/* ---------------------------------------------------------------------------
 * Token array (returned by sc_lex)
 * ------------------------------------------------------------------------- */
typedef struct {
    ScToken *tokens;
    int      count;   /* includes the trailing SC_EOF */
} ScTokenArray;

/* ---------------------------------------------------------------------------
 * Public API
 *
 * sc_lex(source)
 *   Tokenise a complete Snocone source string.
 *   Returns a heap-allocated ScTokenArray terminated by SC_EOF.
 *   Caller must free with sc_tokens_free().
 *
 * sc_tokens_free(arr)
 *   Free all memory owned by the token array.
 *
 * sc_kind_name(kind)
 *   Return a static string name for debugging/testing.
 * ------------------------------------------------------------------------- */
ScTokenArray sc_lex(const char *source);
void         sc_tokens_free(ScTokenArray *arr);
const char  *sc_kind_name(ScKind kind);

#endif /* SC_LEX_H */
