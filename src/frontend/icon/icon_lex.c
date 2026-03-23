/*
 * icon_lex.c — Tiny-ICON hand-rolled lexer
 *
 * No auto-semicolon insertion (deliberate deviation from standard Icon).
 * All expression sequences require explicit ';'.
 */
#define _POSIX_C_SOURCE 200809L
/*
 * Follows structural template of src/frontend/prolog/prolog_lex.c.
 */

#include "icon_lex.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

/* =========================================================================
 * Internal helpers
 * ======================================================================= */

static char lex_cur(const IcnLexer *lx) {
    if (lx->pos >= lx->src_len) return '\0';
    return lx->src[lx->pos];
}
static char lex_peek1(const IcnLexer *lx) {
    if (lx->pos + 1 >= lx->src_len) return '\0';
    return lx->src[lx->pos + 1];
}
static char lex_advance(IcnLexer *lx) {
    if (lx->pos >= lx->src_len) return '\0';
    char c = lx->src[lx->pos++];
    if (c == '\n') { lx->line++; lx->col = 1; } else { lx->col++; }
    return c;
}

/* Grow buffer and append character */
static void buf_push(char **buf, int *len, int *cap, char c) {
    if (*len + 2 > *cap) {
        *cap = (*cap) ? (*cap) * 2 : 32;
        *buf = realloc(*buf, *cap);
    }
    (*buf)[(*len)++] = c;
    (*buf)[*len] = '\0';
}

static IcnToken make_tok(IcnTkKind kind, int line, int col) {
    IcnToken t;
    memset(&t, 0, sizeof(t));
    t.kind = kind; t.line = line; t.col = col;
    return t;
}

static IcnToken make_error(IcnLexer *lx, const char *msg) {
    snprintf(lx->errmsg, sizeof(lx->errmsg), "line %d col %d: %s",
             lx->line, lx->col, msg);
    lx->had_error = 1;
    IcnToken t = make_tok(TK_ERROR, lx->line, lx->col);
    return t;
}

/* =========================================================================
 * Keyword table
 * ======================================================================= */

typedef struct { const char *word; IcnTkKind kind; } KwEntry;

static const KwEntry keywords[] = {
    {"to",         TK_TO},
    {"by",         TK_BY},
    {"every",      TK_EVERY},
    {"do",         TK_DO},
    {"if",         TK_IF},
    {"then",       TK_THEN},
    {"else",       TK_ELSE},
    {"while",      TK_WHILE},
    {"until",      TK_UNTIL},
    {"repeat",     TK_REPEAT},
    {"return",     TK_RETURN},
    {"suspend",    TK_SUSPEND},
    {"fail",       TK_FAIL},
    {"break",      TK_BREAK},
    {"next",       TK_NEXT},
    {"not",        TK_NOT},
    {"procedure",  TK_PROCEDURE},
    {"end",        TK_END},
    {"global",     TK_GLOBAL},
    {"local",      TK_LOCAL},
    {"static",     TK_STATIC},
    {"record",     TK_RECORD},
    {"link",       TK_LINK},
    {"invocable",  TK_INVOCABLE},
    {"case",       TK_CASE},
    {"of",         TK_OF},
    {"default",    TK_DEFAULT},
    {"create",     TK_CREATE},
    {"initial",    TK_INITIAL},
    {NULL,         TK_EOF}
};

static IcnTkKind lookup_keyword(const char *word) {
    for (int i = 0; keywords[i].word; i++)
        if (strcmp(keywords[i].word, word) == 0)
            return keywords[i].kind;
    return TK_IDENT;
}

/* =========================================================================
 * Skip whitespace and # comments
 * ======================================================================= */

static void skip_ws(IcnLexer *lx) {
    for (;;) {
        while (lex_cur(lx) && isspace((unsigned char)lex_cur(lx)))
            lex_advance(lx);
        /* Icon uses # for line comments */
        if (lex_cur(lx) == '#') {
            while (lex_cur(lx) && lex_cur(lx) != '\n')
                lex_advance(lx);
            continue;
        }
        break;
    }
}

/* =========================================================================
 * String / cset literal scanning
 * Icon strings: "..." with escape sequences \n \t \\ \" \uXXXX \xXX
 * Icon csets:   '...' — no escape sequences (verbatim)
 * ======================================================================= */

static IcnToken scan_string(IcnLexer *lx) {
    int line = lx->line, col = lx->col;
    lex_advance(lx); /* consume opening " */
    char *buf = NULL; int len = 0, cap = 0;
    while (lex_cur(lx) && lex_cur(lx) != '"') {
        char c = lex_advance(lx);
        if (c == '\\') {
            char esc = lex_advance(lx);
            switch (esc) {
                case 'n':  buf_push(&buf, &len, &cap, '\n'); break;
                case 't':  buf_push(&buf, &len, &cap, '\t'); break;
                case 'r':  buf_push(&buf, &len, &cap, '\r'); break;
                case '\\': buf_push(&buf, &len, &cap, '\\'); break;
                case '"':  buf_push(&buf, &len, &cap, '"');  break;
                case '\'': buf_push(&buf, &len, &cap, '\''); break;
                case '0':  buf_push(&buf, &len, &cap, '\0'); break;
                default:   buf_push(&buf, &len, &cap, '\\');
                           buf_push(&buf, &len, &cap, esc);  break;
            }
        } else {
            buf_push(&buf, &len, &cap, c);
        }
    }
    if (!lex_cur(lx)) { free(buf); return make_error(lx, "unterminated string literal"); }
    lex_advance(lx); /* consume closing " */
    if (!buf) buf = strdup("");
    IcnToken t = make_tok(TK_STRING, line, col);
    t.val.sval.data = buf;
    t.val.sval.len  = (size_t)len;
    return t;
}

static IcnToken scan_cset(IcnLexer *lx) {
    int line = lx->line, col = lx->col;
    lex_advance(lx); /* consume opening ' */
    char *buf = NULL; int len = 0, cap = 0;
    while (lex_cur(lx) && lex_cur(lx) != '\'') {
        buf_push(&buf, &len, &cap, lex_advance(lx));
    }
    if (!lex_cur(lx)) { free(buf); return make_error(lx, "unterminated cset literal"); }
    lex_advance(lx); /* consume closing ' */
    if (!buf) buf = strdup("");
    IcnToken t = make_tok(TK_CSET, line, col);
    t.val.sval.data = buf;
    t.val.sval.len  = (size_t)len;
    return t;
}

/* =========================================================================
 * Numeric literal scanning
 * Decimal: 42  3.14  1e-5
 * Octal-style: 0377 (Icon doesn't have octal; we just parse as decimal)
 * Hex: 0x1F or 16rFF (Icon radix notation) — we support 0x only
 * ======================================================================= */

static IcnToken scan_number(IcnLexer *lx) {
    int line = lx->line, col = lx->col;
    char *buf = NULL; int len = 0, cap = 0;
    int is_real = 0;

    /* Hex: 0x... */
    if (lex_cur(lx) == '0' &&
        (lex_peek1(lx) == 'x' || lex_peek1(lx) == 'X')) {
        buf_push(&buf, &len, &cap, lex_advance(lx)); /* 0 */
        buf_push(&buf, &len, &cap, lex_advance(lx)); /* x */
        while (isxdigit((unsigned char)lex_cur(lx)))
            buf_push(&buf, &len, &cap, lex_advance(lx));
        long val = strtol(buf, NULL, 16);
        free(buf);
        IcnToken t = make_tok(TK_INT, line, col);
        t.val.ival = val;
        return t;
    }

    /* Decimal integer or real */
    while (isdigit((unsigned char)lex_cur(lx)))
        buf_push(&buf, &len, &cap, lex_advance(lx));

    /* Fractional part */
    if (lex_cur(lx) == '.' && isdigit((unsigned char)lex_peek1(lx))) {
        is_real = 1;
        buf_push(&buf, &len, &cap, lex_advance(lx)); /* . */
        while (isdigit((unsigned char)lex_cur(lx)))
            buf_push(&buf, &len, &cap, lex_advance(lx));
    }

    /* Exponent */
    if (lex_cur(lx) == 'e' || lex_cur(lx) == 'E') {
        is_real = 1;
        buf_push(&buf, &len, &cap, lex_advance(lx));
        if (lex_cur(lx) == '+' || lex_cur(lx) == '-')
            buf_push(&buf, &len, &cap, lex_advance(lx));
        while (isdigit((unsigned char)lex_cur(lx)))
            buf_push(&buf, &len, &cap, lex_advance(lx));
    }

    IcnToken t;
    if (is_real) {
        t = make_tok(TK_REAL, line, col);
        t.val.fval = strtod(buf, NULL);
    } else {
        t = make_tok(TK_INT, line, col);
        t.val.ival = strtol(buf, NULL, 10);
    }
    free(buf);
    return t;
}

/* =========================================================================
 * Identifier / keyword scanning
 * ======================================================================= */

static IcnToken scan_ident(IcnLexer *lx) {
    int line = lx->line, col = lx->col;
    char *buf = NULL; int len = 0, cap = 0;
    while (isalnum((unsigned char)lex_cur(lx)) || lex_cur(lx) == '_')
        buf_push(&buf, &len, &cap, lex_advance(lx));
    if (!buf) buf = strdup("");

    IcnTkKind kind = lookup_keyword(buf);
    IcnToken t = make_tok(kind, line, col);
    if (kind == TK_IDENT) {
        t.val.sval.data = buf;
        t.val.sval.len  = (size_t)len;
    } else {
        free(buf); /* keyword — no string payload needed */
    }
    return t;
}

/* =========================================================================
 * Main token dispatch
 * ======================================================================= */

static IcnToken lex_one(IcnLexer *lx) {
    skip_ws(lx);
    int line = lx->line, col = lx->col;
    char c = lex_cur(lx);

    if (c == '\0') return make_tok(TK_EOF, line, col);

    /* Literals */
    if (c == '"')  return scan_string(lx);
    if (c == '\'') return scan_cset(lx);
    if (isdigit((unsigned char)c)) return scan_number(lx);
    if (isalpha((unsigned char)c) || c == '_') return scan_ident(lx);

    lex_advance(lx);

    switch (c) {
        case '+':
            if (lex_cur(lx) == ':' && lex_peek1(lx) == '=') {
                lex_advance(lx); lex_advance(lx);
                return make_tok(TK_AUGPLUS, line, col);
            }
            return make_tok(TK_PLUS, line, col);

        case '-':
            if (lex_cur(lx) == ':' && lex_peek1(lx) == '=') {
                lex_advance(lx); lex_advance(lx);
                return make_tok(TK_AUGMINUS, line, col);
            }
            if (lex_cur(lx) == '>') {
                lex_advance(lx);
                return make_tok(TK_MINUS, line, col); /* -> not in our set; treat as - > */
            }
            return make_tok(TK_MINUS, line, col);

        case '*':
            if (lex_cur(lx) == ':' && lex_peek1(lx) == '=') {
                lex_advance(lx); lex_advance(lx);
                return make_tok(TK_AUGSTAR, line, col);
            }
            return make_tok(TK_STAR, line, col);

        case '/':
            if (lex_cur(lx) == ':' && lex_peek1(lx) == '=') {
                lex_advance(lx); lex_advance(lx);
                return make_tok(TK_AUGSLASH, line, col);
            }
            return make_tok(TK_SLASH, line, col);

        case '%':
            if (lex_cur(lx) == ':' && lex_peek1(lx) == '=') {
                lex_advance(lx); lex_advance(lx);
                return make_tok(TK_AUGMOD, line, col);
            }
            return make_tok(TK_MOD, line, col);

        case '^': return make_tok(TK_CARET, line, col);

        case '<':
            if (lex_cur(lx) == '<') {
                lex_advance(lx);
                if (lex_cur(lx) == '=') { lex_advance(lx); return make_tok(TK_SLE, line, col); }
                return make_tok(TK_SLT, line, col);
            }
            if (lex_cur(lx) == '=') { lex_advance(lx); return make_tok(TK_LE, line, col); }
            if (lex_cur(lx) == '-') { lex_advance(lx); return make_tok(TK_REVASSIGN, line, col); }
            return make_tok(TK_LT, line, col);

        case '>':
            if (lex_cur(lx) == '>') {
                lex_advance(lx);
                if (lex_cur(lx) == '=') { lex_advance(lx); return make_tok(TK_SGE, line, col); }
                return make_tok(TK_SGT, line, col);
            }
            if (lex_cur(lx) == '=') { lex_advance(lx); return make_tok(TK_GE, line, col); }
            return make_tok(TK_GT, line, col);

        case '=':
            if (lex_cur(lx) == '=') { lex_advance(lx); return make_tok(TK_SEQ, line, col); }
            return make_tok(TK_EQ, line, col);

        case '~':
            if (lex_cur(lx) == '=') {
                lex_advance(lx);
                if (lex_cur(lx) == '=') { lex_advance(lx); return make_tok(TK_SNE, line, col); }
                return make_tok(TK_NEQ, line, col);
            }
            return make_tok(TK_TILDE, line, col);

        case '|':
            if (lex_cur(lx) == '|') {
                lex_advance(lx);
                if (lex_cur(lx) == '|') { lex_advance(lx); return make_tok(TK_LCONCAT, line, col); }
                if (lex_cur(lx) == ':' && lex_peek1(lx) == '=') {
                    lex_advance(lx); lex_advance(lx);
                    return make_tok(TK_AUGCONCAT, line, col);
                }
                return make_tok(TK_CONCAT, line, col);
            }
            return make_tok(TK_BAR, line, col);

        case ':':
            if (lex_cur(lx) == '=') {
                lex_advance(lx);
                if (lex_cur(lx) == ':') { lex_advance(lx); return make_tok(TK_SWAP, line, col); }
                return make_tok(TK_ASSIGN, line, col);
            }
            return make_tok(TK_COLON, line, col);

        case '&': return make_tok(TK_AND, line, col);
        case '\\': return make_tok(TK_BACKSLASH, line, col);
        case '!': return make_tok(TK_BANG, line, col);
        case '?': return make_tok(TK_QMARK, line, col);
        case '@': return make_tok(TK_AT, line, col);
        case '.': return make_tok(TK_DOT, line, col);

        case '(': return make_tok(TK_LPAREN, line, col);
        case ')': return make_tok(TK_RPAREN, line, col);
        case '{': return make_tok(TK_LBRACE, line, col);
        case '}': return make_tok(TK_RBRACE, line, col);
        case '[': return make_tok(TK_LBRACK, line, col);
        case ']': return make_tok(TK_RBRACK, line, col);
        case ',': return make_tok(TK_COMMA, line, col);
        case ';': return make_tok(TK_SEMICOL, line, col);

        default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "unexpected character '%c' (0x%02x)", c, (unsigned char)c);
            return make_error(lx, msg);
        }
    }
}

/* =========================================================================
 * Public API
 * ======================================================================= */

void icn_lex_init(IcnLexer *lx, const char *src) {
    memset(lx, 0, sizeof(*lx));
    lx->src     = src;
    lx->src_len = strlen(src);
    lx->pos     = 0;
    lx->line    = 1;
    lx->col     = 1;
}

IcnToken icn_lex_next(IcnLexer *lx) {
    /* If a peeked token is buffered, consume it */
    if (lx->had_error == -1) {
        /* sentinel: no peek buffered */
    }
    return lex_one(lx);
}

/* Simple peek: scan one token, back up position */
IcnToken icn_lex_peek(IcnLexer *lx) {
    size_t saved_pos  = lx->pos;
    int    saved_line = lx->line;
    int    saved_col  = lx->col;
    IcnToken t = lex_one(lx);
    /* Restore position but NOT the string allocation — caller must not
     * use sval pointers after calling icn_lex_next. */
    lx->pos  = saved_pos;
    lx->line = saved_line;
    lx->col  = saved_col;
    return t;
}

const char *icn_tk_name(IcnTkKind kind) {
    switch (kind) {
        case TK_EOF:       return "EOF";
        case TK_ERROR:     return "ERROR";
        case TK_INT:       return "INT";
        case TK_REAL:      return "REAL";
        case TK_STRING:    return "STRING";
        case TK_CSET:      return "CSET";
        case TK_IDENT:     return "IDENT";
        case TK_PLUS:      return "+";
        case TK_MINUS:     return "-";
        case TK_STAR:      return "*";
        case TK_SLASH:     return "/";
        case TK_MOD:       return "%";
        case TK_CARET:     return "^";
        case TK_LT:        return "<";
        case TK_LE:        return "<=";
        case TK_GT:        return ">";
        case TK_GE:        return ">=";
        case TK_EQ:        return "=";
        case TK_NEQ:       return "~=";
        case TK_SLT:       return "<<";
        case TK_SLE:       return "<<=";
        case TK_SGT:       return ">>";
        case TK_SGE:       return ">>=";
        case TK_SEQ:       return "==";
        case TK_SNE:       return "~==";
        case TK_CONCAT:    return "||";
        case TK_LCONCAT:   return "|||";
        case TK_ASSIGN:    return ":=";
        case TK_SWAP:      return ":=:";
        case TK_REVASSIGN: return "<-";
        case TK_AUGPLUS:   return "+:=";
        case TK_AUGMINUS:  return "-:=";
        case TK_AUGSTAR:   return "*:=";
        case TK_AUGSLASH:  return "/:=";
        case TK_AUGMOD:    return "%:=";
        case TK_AUGCONCAT: return "||:=";
        case TK_AND:       return "&";
        case TK_BAR:       return "|";
        case TK_BACKSLASH: return "\\";
        case TK_BANG:      return "!";
        case TK_QMARK:     return "?";
        case TK_AT:        return "@";
        case TK_TILDE:     return "~";
        case TK_DOT:       return ".";
        case TK_TO:        return "to";
        case TK_BY:        return "by";
        case TK_EVERY:     return "every";
        case TK_DO:        return "do";
        case TK_IF:        return "if";
        case TK_THEN:      return "then";
        case TK_ELSE:      return "else";
        case TK_WHILE:     return "while";
        case TK_UNTIL:     return "until";
        case TK_REPEAT:    return "repeat";
        case TK_RETURN:    return "return";
        case TK_SUSPEND:   return "suspend";
        case TK_FAIL:      return "fail";
        case TK_BREAK:     return "break";
        case TK_NEXT:      return "next";
        case TK_NOT:       return "not";
        case TK_PROCEDURE: return "procedure";
        case TK_END:       return "end";
        case TK_GLOBAL:    return "global";
        case TK_LOCAL:     return "local";
        case TK_STATIC:    return "static";
        case TK_RECORD:    return "record";
        case TK_LINK:      return "link";
        case TK_INVOCABLE: return "invocable";
        case TK_CASE:      return "case";
        case TK_OF:        return "of";
        case TK_DEFAULT:   return "default";
        case TK_CREATE:    return "create";
        case TK_INITIAL:   return "initial";
        case TK_LPAREN:    return "(";
        case TK_RPAREN:    return ")";
        case TK_LBRACE:    return "{";
        case TK_RBRACE:    return "}";
        case TK_LBRACK:    return "[";
        case TK_RBRACK:    return "]";
        case TK_COMMA:     return ",";
        case TK_SEMICOL:   return ";";
        case TK_COLON:     return ":";
        default:           return "???";
    }
}
