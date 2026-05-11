/*
 * snocone_lex.c -- Snocone threaded-code FSM lexer
 *                  GOAL-SNOCONE-LANG-SPACE LS-3.f.1, session 2026-04-30
 *
 * Replaces snocone.l / snocone.lex.c (flex 2.6.4 hangs on the {W}OP{W}
 * envelopes when comments are folded into {W} -- empirically verified).
 * Replaces also the two-pass hand-written snocone_lex.c per Lon
 * directive session 2026-04-30 #1: "use clean emission technique
 * just like SNOBOL4, use one pass when ever possible."
 *
 * THE FSM IS THE CODE
 * -------------------
 * sc_lex_next(ctx) returns one token kind per call.  The function
 * body is a graph of labelled blocks linked by goto statements.
 * Each line is exactly:
 *
 *     Label:   [if (cond)]   action;          goto NEXT;
 *
 * No switch, for, while, or do/until appears anywhere in the FSM
 * body.  Loops are formed by backward-pointing gotos -- they are
 * visible as cycles in the label graph, not hidden in language
 * constructs.  Comments-as-whitespace falls out for free: S_LCOMMENT
 * and S_BCOMMENT both transition back into S_WS on completion.
 *
 * STATE BETWEEN CALLS (in LexCtx):
 *   p           cursor into the source buffer (advances per call)
 *   line        1-based line number
 *   last_kind   kind of the most recently emitted token; consulted
 *               by the next call for CONCAT-trigger and bin/unary
 *               disambiguation.  Initialised to 0.
 *   text        payload buffer for the most recent value token.
 *   strbuf      intermediate buffer for string-literal accumulation.
 *   strpos      cursor into strbuf.
 *
 * PER-CALL LOCAL STATE:
 *   had_ws      set to 1 by the leading whitespace loop if any
 *               whitespace or comment was consumed before the token.
 *   tok_start   pointer to the first byte of the current token's
 *               payload (used by EMIT_V).
 *   last_value  cache of sc_kind_is_value(ctx->last_kind), used for
 *               CONCAT trigger and the {W}OP{W} bin-vs-unary test.
 *
 * THE TWO RULES, SAID PLAINLY
 * ---------------------------
 *   1. A binary operator requires whitespace on BOTH sides.  If
 *      either side is missing, the operator is unary.  Special
 *      case: '=' accepts left-only '{W}=' too, for the DYN-63
 *      end-of-line idiom 'x ='.
 *   2. T_CONCAT is emitted between two value-yielding tokens that
 *      have whitespace between them and no operator.
 *
 * Token kinds (T_*) match snobol4.tab.h -- same name everywhere
 * for any concept equivalence (RULES.md / Lon's session-#9).
 *
 * Commit identity: LCherryholmes / lcherryh@yahoo.com  (RULES.md)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "snocone_lex.h"
#include "snocone_parse.tab.h"          /* T_* token enum (single source of truth) */
/*--------------------------------------------------------------------------------------------------------------------*/
static inline int is_alpha(int c)        { return ((c | 32) >= 'a' && (c | 32) <= 'z') || c == '_'; }
static inline int is_digit(int c)        { return c >= '0' && c <= '9'; }
static inline int is_idcont(int c)       { return is_alpha(c) || is_digit(c); }
/* is_value_starter -- a char that begins a fresh value token, used
 * by the CONCAT-trigger check in S_DISPATCH after a whitespace gap.
 *
 * Unary-prefix operator chars (+, -, *, &, etc.) are NOT here.
 * Each S_OP_* label decides binary-vs-unary based on right-ws, and
 * when it picks unary AND there was a leading-ws value-token, that
 * label itself emits CONCAT (cursor stays put for the next call).
 *
 * '{' is not here: '{' opens a block, no CONCAT before block.
 * '[' is not here: '[' is subscript only, regardless of whitespace. */
static inline int is_value_starter(int c) {
    return is_alpha(c) || is_digit(c) || c == '\'' || c == '"' ||
           c == '(';
}
/* is_rws -- "is right-side whitespace": the char `c` (peeked AFTER an
 * operator candidate) counts as the right-ws of the {W}OP{W} envelope
 * if it is a literal whitespace character, end-of-input, or the start
 * of a comment (// or slash-star).  The two-char comment lookahead
 * is folded in via the predicate `is_rws_at(p, n)` which knows the
 * full source.
 *
 * The dual-role unary/binary operators (& | ? $ . + - * / ^ ! @ # % ~)
 * are binary only when {W}OP{W} -- both sides whitespace.  Without
 * right-ws they are unary.  This is the SPITBOL/SNOBOL4 rule and is
 * what makes `x && y` lex as IDENT CONCAT UN_AMP UN_AMP IDENT (the
 * first '&' has no right-ws because '&' follows it, so it's unary).
 */
static inline int is_rws_char(int c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' ||
           c == '\n' || c == '\0';
}
static inline int is_rws_at(const char *p, int n) {
    int c0 = (unsigned char)p[n];
    if (is_rws_char(c0)) return 1;
    if (c0 == '/') {
        int c1 = (unsigned char)p[n + 1];
        if (c1 == '/' || c1 == '*') return 1;
    }
    return 0;
}
/* ---------------------------------------------------------------- */
/* sc_kind_is_value -- table-driven; no branches at call site.      */
/* Filled at first call (idempotent).                               */
/* ---------------------------------------------------------------- */
/* Tables are indexed by token-kind value.  Bison's enum sc_tokentype
 * (snocone_parse.tab.h) places user tokens starting at 258 and counts
 * up to roughly 340 — so 512 is comfortably above the range. */
static signed char sc_value_table[512];
static signed char sc_payload_table[512];
static int         sc_value_table_built = 0;
static void sc_value_table_build(void) {
    /* sc_value_table — "is this a value-ender" for the lexer's own
     * CONCAT-injection / binary-vs-unary dispatch decisions. */
    sc_value_table[T_IDENT]    = 1;
    /* T_CALL is NOT a value-ender: it atomically consumes IDENT + `(`,
     * so the next token sits inside the arg list (post-LPAREN state).
     * Treating T_CALL as a value would make `*` after `f(` lex as
     * binary multiplication instead of unary defer. */
    sc_value_table[T_INT]      = 1;
    sc_value_table[T_REAL]     = 1;
    sc_value_table[T_STR]      = 1;
    sc_value_table[T_KEYWORD]  = 1;
    sc_value_table[T_RPAREN]   = 1;
    sc_value_table[T_RBRACK]   = 1;

    /* sc_payload_table — "does this token carry a text payload in
     * ctx->text" for the parser thunk to strdup into yylval->str.
     * T_CALL belongs here (the identifier name) even though it is
     * not a value-ender. */
    sc_payload_table[T_IDENT]   = 1;
    sc_payload_table[T_CALL]    = 1;
    sc_payload_table[T_INT]     = 1;
    sc_payload_table[T_REAL]    = 1;
    sc_payload_table[T_STR]     = 1;
    sc_payload_table[T_KEYWORD] = 1;

    sc_value_table_built       = 1;
}
int sc_kind_is_value(int kind) {
    if (!sc_value_table_built) sc_value_table_build();
    return (kind >= 0 && kind < 512) ? sc_value_table[kind] : 0;
}
int sc_kind_has_payload(int kind) {
    if (!sc_value_table_built) sc_value_table_build();
    return (kind >= 0 && kind < 512) ? sc_payload_table[kind] : 0;
}
/* ---------------------------------------------------------------- */
/* Keyword classifier -- tail-recursive table walk.                 */
/* GCC -O2 turns the recursion into a loop; no source-level loop.   */
/* ---------------------------------------------------------------- */
typedef struct { const char *word; int kind; } KwEntry;
static const KwEntry KW_TABLE[] = {
    { "if",       T_IF       },
    { "else",     T_ELSE     },
    { "while",    T_WHILE    },
    { "do",       T_DO       },
    { "for",      T_FOR      },
    { "switch",   T_SWITCH   },
    { "case",     T_CASE     },
    { "default",  T_DEFAULT  },
    { "break",    T_BREAK    },
    { "continue", T_CONTINUE },
    { "goto",     T_GOTO     },
    { "function",   T_DEFINE },
    /* `procedure` synonym removed session 2026-05-01 #7 — corpus
     * migrated to `function` (61 files, 297 replacements) via
     * util_migrate_snocone_procedure_to_function.py.  Bare
     * `procedure` is now an IDENT (almost certainly a syntax error
     * in any post-migration .sc file). */
    { "return",   T_RETURN   },
    { "freturn",  T_FRETURN  },
    { "nreturn",  T_NRETURN  },
    { "struct",   T_STRUCT   },
    { NULL,       T_IDENT       }
};
static int kw_lookup_at(const char *s, int idx) {
    if (KW_TABLE[idx].word == NULL)              return T_IDENT;
    if (strcmp(s, KW_TABLE[idx].word) == 0)      return KW_TABLE[idx].kind;
    return kw_lookup_at(s, idx + 1);
}
static int classify_keyword_range(const char *start, const char *end) {
    int n = (int)(end - start);
    if (n <= 0 || n > 32) return T_IDENT;
    char buf[64];
    memcpy(buf, start, n);
    buf[n] = '\0';
    return kw_lookup_at(buf, 0);
}
/* ---------------------------------------------------------------- */
/* Emit helpers                                                     */
/* ---------------------------------------------------------------- */
/* ADV(n)     advance cursor by n
 * PEEK(n)    read p[n] without advancing
 * EMIT(k)    return non-value token; updates last_kind and ctx->p
 * EMIT_V(k)  return value token; copies tok_start..p into ctx->text
 *
 * The standard `do { ... } while (0)` macro idiom is avoided here
 * to keep the file free of loop syntax.  These are written with
 * comma operators or as separate statements in the per-label code.
 */
#define ADV(n)    (p += (n))
#define PEEK(n)   ((unsigned char)p[n])
static inline int emit_kind(LexCtx *ctx, SC_STYPE *yylval, const char *p, int kind) {
    yylval->str = NULL;
    ctx->p = p;
    ctx->last_kind = kind;
    return kind;
}
static inline int emit_value(LexCtx *ctx, SC_STYPE *yylval, const char *p, const char *tok_start, int kind) {
    int n = (int)(p - tok_start);
    if (n >= (int)sizeof(ctx->text)) n = (int)sizeof(ctx->text) - 1;
    memcpy(ctx->text, tok_start, n);
    ctx->text[n] = '\0';
    yylval->str = strdup(ctx->text);
    ctx->p = p;
    ctx->last_kind = kind;
    return kind;
}
#define EMIT(k)    return emit_kind(ctx, yylval, p, (k))
#define EMIT_V(k)  return emit_value(ctx, yylval, p, tok_start, (k))
int sc_lex(SC_STYPE *yylval, ScParseState *st) {
    LexCtx     *ctx        = st->ctx;
    const char *p          = ctx->p;
    const char *tok_start  = NULL;
    int         had_ws     = 0;
    int         last_value = sc_kind_is_value(ctx->last_kind);
    /* | Label        | if                                         | action                                    | goto             | */
    /* |--------------|--------------------------------------------|-------------------------------------------|------------------| */
S_WS:
    if (PEEK(0) == ' '  )                                          {  had_ws = 1; ADV(1);                                  goto S_WS;        }
    if (PEEK(0) == '\t' )                                          {  had_ws = 1; ADV(1);                                  goto S_WS;        }
    if (PEEK(0) == '\r' )                                          {  had_ws = 1; ADV(1);                                  goto S_WS;        }
    if (PEEK(0) == '\f' )                                          {  had_ws = 1; ADV(1);                                  goto S_WS;        }
    if (PEEK(0) == '\n' )                                          {  ctx->line++; had_ws = 1; ADV(1);                     goto S_CONT;      }
    if (PEEK(0) == '/'  && PEEK(1) == '/'  )                       {  had_ws = 1; ADV(2);                                  goto S_LCOMMENT;  }
    if (PEEK(0) == '/'  && PEEK(1) == '*'  )                       {  had_ws = 1; ADV(2);                                  goto S_BCOMMENT;  }
                                                                                                                           goto S_DISPATCH;
/*--------------------------------------------------------------------------------------------------------------------*/
S_CONT:
    /* After a newline: SNOBOL4 column-1 continuation marker '+' or '.' */
    if (PEEK(0) == '+'  )                                          {  ADV(1);                                              goto S_WS;        }
    if (PEEK(0) == '.'  )                                          {  ADV(1);                                              goto S_WS;        }
                                                                                                                           goto S_WS;
/*--------------------------------------------------------------------------------------------------------------------*/
S_LCOMMENT:
    if (PEEK(0) == '\0' )                                                                                                  goto S_DISPATCH;
    if (PEEK(0) == '\n' )                                                                                                  goto S_WS;
                                                                   {  ADV(1);                                              goto S_LCOMMENT;  }
/*--------------------------------------------------------------------------------------------------------------------*/
S_BCOMMENT:
    if (PEEK(0) == '\0' )                                                                                                  goto S_DISPATCH;
    if (PEEK(0) == '*'  )                                          {  ADV(1);                                              goto S_BC_STAR;   }
    if (PEEK(0) == '\n' )                                          {  ctx->line++; ADV(1);                                 goto S_BCOMMENT;  }
                                                                   {  ADV(1);                                              goto S_BCOMMENT;  }
/*--------------------------------------------------------------------------------------------------------------------*/
S_BC_STAR:
    if (PEEK(0) == '/'  )                                          {  ADV(1);                                              goto S_WS;        }
    if (PEEK(0) == '*'  )                                          {  ADV(1);                                              goto S_BC_STAR;   }
    if (PEEK(0) == '\0' )                                                                                                  goto S_DISPATCH;
                                                                   {  ADV(1);                                              goto S_BCOMMENT;  }
/*--------------------------------------------------------------------------------------------------------------------*/
S_DISPATCH:
    /* CONCAT trigger: prev was a value, gap had whitespace, next
     * char unambiguously starts a fresh value-token.  '.DIGIT' is
     * also a value-starter (real number, e.g. .5 after `x`). */
    if (had_ws && last_value && is_value_starter(PEEK(0)))         {  EMIT(T_CONCAT);                 }
    if (had_ws && last_value && PEEK(0) == '.' && is_digit(PEEK(1))) { EMIT(T_CONCAT);                }
    /* CONCAT trigger for &IDENT keyword reference: `&` followed by an
     * alpha forms a keyword token (e.g. `&UCASE`), which is a value.
     * Without this, `KEYWORD KEYWORD` (e.g. inside ANY(&UCASE &LCASE))
     * has no T_CONCAT injected and the parser sees two adjacent value
     * tokens with no operator between, causing a syntax error. */
    if (had_ws && last_value && PEEK(0) == '&' && is_alpha(PEEK(1))) { EMIT(T_CONCAT);               }
    if (PEEK(0) == '\0' )                                                                                                  goto AST_EOF;
    if (PEEK(0) == '\'' )                                          {  ctx->strpos = 0; ADV(1);                             goto S_STR1;      }
    if (PEEK(0) == '"'  )                                          {  ctx->strpos = 0; ADV(1);                             goto S_STR2;      }
    if (is_alpha(PEEK(0)))                                         {  tok_start = p; ADV(1);                               goto S_IDENT;     }
    if (is_digit(PEEK(0)))                                         {  tok_start = p; ADV(1);                               goto S_INT;       }
    if (PEEK(0) == '.'  && is_digit(PEEK(1)))                      {  tok_start = p; ADV(1);                                goto S_FRAC;      }
    if (PEEK(0) == '.'  )                                                                                                  goto S_OP_DOT;
    if (PEEK(0) == '&'  && is_alpha(PEEK(1)))                      {  ADV(1); tok_start = p;                               goto S_KEYWORD;   }
    if (PEEK(0) == '('  )                                          {  ADV(1);                                              goto AST_LPAREN;    }
    if (PEEK(0) == ')'  )                                          {  ADV(1);                                              goto AST_RPAREN;    }
    if (PEEK(0) == '['  )                                          {  ADV(1);                                              goto AST_LBRACK;    }
    if (PEEK(0) == ']'  )                                          {  ADV(1);                                              goto AST_RBRACK;    }
    if (PEEK(0) == '{'  )                                          {  ADV(1);                                              goto AST_LBRACE;    }
    if (PEEK(0) == '}'  )                                          {  ADV(1);                                              goto AST_RBRACE;    }
    if (PEEK(0) == ','  )                                          {  ADV(1);                                              goto AST_COMMA;     }
    if (PEEK(0) == ';'  )                                          {  ADV(1);                                              goto AST_SEMICOLON; }
    if (PEEK(0) == ':'  )                                                                                                  goto S_OP_COLON;
    if (PEEK(0) == '='  )                                                                                                  goto S_OP_EQ;
    if (PEEK(0) == '!'  )                                                                                                  goto S_OP_BANG;
    if (PEEK(0) == '<'  )                                                                                                  goto S_OP_LT;
    if (PEEK(0) == '>'  )                                                                                                  goto S_OP_GT;
    if (PEEK(0) == '+'  )                                                                                                  goto S_OP_PLUS;
    if (PEEK(0) == '-'  )                                                                                                  goto S_OP_MINUS;
    if (PEEK(0) == '*'  )                                                                                                  goto S_OP_STAR;
    if (PEEK(0) == '/'  )                                                                                                  goto S_OP_SLASH;
    if (PEEK(0) == '^'  )                                                                                                  goto S_OP_CARET;
    if (PEEK(0) == '|'  )                                                                                                  goto S_OP_PIPE;
    if (PEEK(0) == '?'  )                                                                                                  goto S_OP_QUEST;
    if (PEEK(0) == '$'  )                                                                                                  goto S_OP_DOLLAR;
    if (PEEK(0) == '&'  )                                                                                                  goto S_OP_AMP;
    if (PEEK(0) == '@'  )                                                                                                  goto S_OP_AT;
    if (PEEK(0) == '#'  )                                                                                                  goto S_OP_POUND;
    if (PEEK(0) == '%'  )                                                                                                  goto S_OP_PERCENT;
    if (PEEK(0) == '~'  )                                                                                                  goto S_OP_TILDE;
                                                                   {  tok_start = p; ADV(1);                               goto TT_UNKNOWN;   }
/*--------------------------------------------------------------------------------------------------------------------*/
S_IDENT:
    if (is_idcont(PEEK(0)))                                        {  ADV(1);                                              goto S_IDENT;     }
    if (PEEK(0) == '('  )                                                                                                  goto AST_CALL;
                                                                   {                                                       goto AST_IDENT;     }
/*--------------------------------------------------------------------------------------------------------------------*/
S_KEYWORD:
    if (is_idcont(PEEK(0)))                                        {  ADV(1);                                              goto S_KEYWORD;   }
                                                                                                                           goto TT_KEYWORD;
/*--------------------------------------------------------------------------------------------------------------------*/
S_INT:
    if (is_digit(PEEK(0)))                                         {  ADV(1);                                              goto S_INT;       }
    if (PEEK(0) == '.' && is_digit(PEEK(1)))                       {  ADV(1);                                              goto S_FRAC;      }
    if (PEEK(0) == '.' )                                           {  ADV(1);                                              goto AST_REAL;      }
    if (PEEK(0) == 'e' || PEEK(0) == 'E')                          {  ADV(1);                                              goto S_EXP_SIGN;  }
    if (PEEK(0) == 'd' || PEEK(0) == 'D')                          {  ADV(1);                                              goto S_EXP_SIGN;  }
                                                                                                                           goto AST_INT;
/*--------------------------------------------------------------------------------------------------------------------*/
S_FRAC:
    if (is_digit(PEEK(0)))                                         {  ADV(1);                                              goto S_FRAC;      }
    if (PEEK(0) == 'e' || PEEK(0) == 'E')                          {  ADV(1);                                              goto S_EXP_SIGN;  }
    if (PEEK(0) == 'd' || PEEK(0) == 'D')                          {  ADV(1);                                              goto S_EXP_SIGN;  }
                                                                                                                           goto AST_REAL;
/*--------------------------------------------------------------------------------------------------------------------*/
S_EXP_SIGN:
    if (PEEK(0) == '+' || PEEK(0) == '-')                          {  ADV(1);                                              goto S_EXP_DIG;   }
                                                                                                                           goto S_EXP_DIG;
/*--------------------------------------------------------------------------------------------------------------------*/
S_EXP_DIG:
    if (is_digit(PEEK(0)))                                         {  ADV(1);                                              goto S_EXP_DIG;   }
                                                                                                                           goto AST_REAL;
/*--------------------------------------------------------------------------------------------------------------------*/
S_STR1:
    if (PEEK(0) == '\0' )                                                                                                  goto AST_STR;
    if (PEEK(0) == '\'' && PEEK(1) == '\'' )                       {  ctx->strbuf[ctx->strpos++] = '\''; ADV(2);           goto S_STR1;      }
    if (PEEK(0) == '\'' )                                          {  ADV(1);                                              goto AST_STR;       }
    if (PEEK(0) == '\n' )                                          {  ctx->line++; ctx->strbuf[ctx->strpos++] = '\n'; ADV(1); goto S_STR1;   }
                                                                   {  ctx->strbuf[ctx->strpos++] = *p; ADV(1);              goto S_STR1;     }
/*--------------------------------------------------------------------------------------------------------------------*/
S_STR2:
    if (PEEK(0) == '\0' )                                                                                                  goto AST_STR;
    if (PEEK(0) == '"'  && PEEK(1) == '"'  )                       {  ctx->strbuf[ctx->strpos++] = '"';  ADV(2);           goto S_STR2;      }
    if (PEEK(0) == '"'  )                                          {  ADV(1);                                              goto AST_STR;       }
    if (PEEK(0) == '\n' )                                          {  ctx->line++; ctx->strbuf[ctx->strpos++] = '\n'; ADV(1); goto S_STR2;   }
                                                                   {  ctx->strbuf[ctx->strpos++] = *p; ADV(1);              goto S_STR2;     }
    /* Each S_OP_* label disambiguates multi-char forms (++, +=,    */
    /* etc.), then chooses BINARY vs UNARY based on:                */
    /*    BINARY needs left-ws + last-was-value (had_ws+last_value).*/
    /*    UNARY  is the default.                                    */
    /* The {W}OP{W} right-ws check is implicit: with no right-ws,   */
    /* the left-side condition still fires correctly because the    */
    /* parser will see ADD/etc and the absence of right-ws is fine. */
    /* (Snobol4 / SPITBOL convention: x+ is x followed by unary +.) */
    /* ============================================================ */
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_COLON:
    if (PEEK(1) == ':' )                                           {  ADV(2);                                              goto AST_IDENT_OP;  }
    if (PEEK(1) == '!' && PEEK(2) == ':' )                         {  ADV(3);                                              goto AST_DIFFER;    }
    if (PEEK(1) == '<' && PEEK(2) == '=' && PEEK(3) == ':' )       {  ADV(4);                                              goto TT_LLE;       }
    if (PEEK(1) == '>' && PEEK(2) == '=' && PEEK(3) == ':' )       {  ADV(4);                                              goto TT_LGE;       }
    if (PEEK(1) == '=' && PEEK(2) == '=' && PEEK(3) == ':' )       {  ADV(4);                                              goto TT_LEQ;       }
    if (PEEK(1) == '!' && PEEK(2) == '=' && PEEK(3) == ':' )       {  ADV(4);                                              goto TT_LNE;       }
    if (PEEK(1) == '<' && PEEK(2) == ':' )                         {  ADV(3);                                              goto TT_LLT;       }
    if (PEEK(1) == '>' && PEEK(2) == ':' )                         {  ADV(3);                                              goto TT_LGT;       }
                                                                   {  ADV(1);                                              goto AST_COLON;     }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_EQ:
    if (PEEK(1) == '=' )                                           {  ADV(2);                                              goto TT_EQ;        }
    if (last_value)                                                {  ADV(1);                                              goto TT_ASSIGN;    }
                                                                   {  ADV(1);                                              goto AST_UN_EQUAL;  }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_BANG:
    if (PEEK(1) == '=' )                                           {  ADV(2);                                              goto TT_NE;        }
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto AST_EXP;       }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_BANG;   }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_LT:
    if (PEEK(1) == '=' )                                           {  ADV(2);                                              goto TT_LE;        }
                                                                   {  ADV(1);                                              goto TT_LT;        }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_GT:
    if (PEEK(1) == '=' )                                           {  ADV(2);                                              goto TT_GE;        }
                                                                   {  ADV(1);                                              goto TT_GT;        }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_PLUS:
    if (PEEK(1) == '=' )                                           {  ADV(2);                                              goto AST_PLUS_ASSIGN;  }
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto TT_ADD;       }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_PLUS;   }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_MINUS:
    if (PEEK(1) == '=' )                                           {  ADV(2);                                              goto AST_MINUS_ASSIGN; }
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto TT_SUB;       }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_MINUS;  }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_STAR:
    if (PEEK(1) == '*' && is_rws_at(p, 2))                         {  ADV(2);                                              goto AST_EXP;       }
    if (PEEK(1) == '=' )                                           {  ADV(2);                                              goto AST_STAR_ASSIGN;  }
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto TT_MUL;       }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_STAR;   }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_SLASH:
    if (PEEK(1) == '=' )                                           {  ADV(2);                                              goto AST_SLASH_ASSIGN; }
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto TT_DIV;       }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_SLASH;  }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_CARET:
    if (PEEK(1) == '=' )                                           {  ADV(2);                                              goto AST_CARET_ASSIGN; }
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto AST_EXP;       }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  tok_start = p; ADV(1);                               goto TT_UNKNOWN;   }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_PIPE:
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto TT_ALT;       }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_PIPE;   }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_QUEST:
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto AST_MATCH;     }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_QUEST;  }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_DOLLAR:
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto AST_IMM_ASSIGN;}
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_DOLLAR; }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_DOT:
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto AST_COND_ASSIGN;  }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_DOT;    }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_AMP:
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto AST_AMP;       }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_AMP;    }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_AT:
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto AST_AT;        }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_AT;     }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_POUND:
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto AST_POUND;     }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_POUND;  }
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_PERCENT:
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto AST_PERCENT;   }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_PERCENT;}
/*--------------------------------------------------------------------------------------------------------------------*/
S_OP_TILDE:
    if (had_ws && last_value && is_rws_at(p, 1))                   {  ADV(1);                                              goto AST_TILDE;     }
    if (had_ws && last_value)                                      {  EMIT(T_CONCAT);                 }
                                                                   {  ADV(1);                                              goto AST_UN_TILDE;  }
/*--------------------------------------------------------------------------------------------------------------------*/
AST_EOF:           return 0;        /* Bison end-of-input sentinel */
/*--------------------------------------------------------------------------------------------------------------------*/
AST_LPAREN:        EMIT(T_LPAREN);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_RPAREN:        EMIT(T_RPAREN);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_LBRACK:        EMIT(T_LBRACK);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_RBRACK:        EMIT(T_RBRACK);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_LBRACE:        EMIT(T_LBRACE);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_RBRACE:        EMIT(T_RBRACE);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_COMMA:         EMIT(T_COMMA);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_SEMICOLON:     EMIT(T_SEMICOLON);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_COLON:         EMIT(T_COLON);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_ASSIGN:        EMIT(T_2EQUAL);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_MATCH:         EMIT(T_2QUEST);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_ALT:           EMIT(T_2PIPE);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_LEQ:           EMIT(T_LEQ);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_LNE:           EMIT(T_LNE);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_LLE:           EMIT(T_LLE);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_LGE:           EMIT(T_LGE);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_LLT:           EMIT(T_LLT);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_LGT:           EMIT(T_LGT);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_DIFFER:        EMIT(T_DIFFER);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_IDENT_OP:      EMIT(T_IDENT_OP);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_EQ:            EMIT(T_EQ);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_NE:            EMIT(T_NE);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_LE:            EMIT(T_LE);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_GE:            EMIT(T_GE);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_LT:            EMIT(T_LT);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_GT:            EMIT(T_GT);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_ADD:           EMIT(T_2PLUS);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_SUB:           EMIT(T_2MINUS);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_MUL:           EMIT(T_2STAR);
/*--------------------------------------------------------------------------------------------------------------------*/
TT_DIV:           EMIT(T_2SLASH);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_EXP:           EMIT(T_2CARET);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_IMM_ASSIGN:    EMIT(T_2DOLLAR);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_COND_ASSIGN:   EMIT(T_2DOT);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_AMP:           EMIT(T_2AMP);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_AT:            EMIT(T_2AT);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_POUND:         EMIT(T_2POUND);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_PERCENT:       EMIT(T_2PERCENT);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_TILDE:         EMIT(T_2TILDE);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_PLUS_ASSIGN:   EMIT(T_PLUS_ASSIGN);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_MINUS_ASSIGN:  EMIT(T_MINUS_ASSIGN);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_STAR_ASSIGN:   EMIT(T_STAR_ASSIGN);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_SLASH_ASSIGN:  EMIT(T_SLASH_ASSIGN);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_CARET_ASSIGN:  EMIT(T_CARET_ASSIGN);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_PLUS:       EMIT(T_1PLUS);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_MINUS:      EMIT(T_1MINUS);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_STAR:       EMIT(T_1STAR);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_SLASH:      EMIT(T_1SLASH);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_PERCENT:    EMIT(T_1PERCENT);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_AT:         EMIT(T_1AT);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_TILDE:      EMIT(T_1TILDE);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_DOLLAR:     EMIT(T_1DOLLAR);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_DOT:        EMIT(T_1DOT);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_POUND:      EMIT(T_1POUND);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_PIPE:       EMIT(T_1PIPE);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_EQUAL:      EMIT(T_1EQUAL);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_QUEST:      EMIT(T_1QUEST);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_AMP:        EMIT(T_1AMP);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_UN_BANG:       EMIT(T_1BANG);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_INT:           EMIT_V(T_INT);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_REAL:          EMIT_V(T_REAL);
/*--------------------------------------------------------------------------------------------------------------------*/
AST_CALL:
    /* T_CALL is the call-form token: IDENT immediately followed by `(`,
     * emitted as a single atomic token (the `(` is consumed here).  The
     * grammar form is `T_CALL exprlist T_RPAREN` — no separate T_LPAREN.
     *
     * Two cases must NOT produce T_CALL:
     *   (a) the IDENT is a keyword (e.g. `if(cond)`, `while(cond)`) —
     *       the keyword retains its keyword token; `(` stays in stream.
     *   (b) the prev token is T_DEFINE (e.g. `function name(args)`) —
     *       this is a definition, not a call; the name is plain T_IDENT
     *       and the `(` stays in stream as T_LPAREN.
     * In both cases redirect to AST_IDENT, which emits the proper token
     * without consuming `(`. */
    if (classify_keyword_range(tok_start, p) != T_IDENT) goto AST_IDENT;
    if (ctx->last_kind == T_DEFINE)                      goto AST_IDENT;
    {
        /* Capture identifier name (range [tok_start, p)) into ctx->text,
         * then advance p past the `(` so it is consumed atomically. */
        int n = (int)(p - tok_start);
        if (n >= (int)sizeof(ctx->text)) n = (int)sizeof(ctx->text) - 1;
        memcpy(ctx->text, tok_start, n);
        ctx->text[n] = '\0';
        yylval->str = strdup(ctx->text);
        ADV(1);  /* consume the `(` */
        ctx->p = p;
        ctx->last_kind = T_CALL;
        return T_CALL;
    }
/*--------------------------------------------------------------------------------------------------------------------*/
AST_IDENT:
    {
        int kind = classify_keyword_range(tok_start, p);
        int n    = (int)(p - tok_start);
        if (n >= (int)sizeof(ctx->text)) n = (int)sizeof(ctx->text) - 1;
        memcpy(ctx->text, tok_start, n);
        ctx->text[n] = '\0';
        yylval->str = strdup(ctx->text);
        ctx->p = p; ctx->last_kind = kind; return kind;
    }
/*--------------------------------------------------------------------------------------------------------------------*/
TT_KEYWORD:
    {
        int n = (int)(p - tok_start);
        if (n >= (int)sizeof(ctx->text)) n = (int)sizeof(ctx->text) - 1;
        memcpy(ctx->text, tok_start, n);
        ctx->text[n] = '\0';
        yylval->str = strdup(ctx->text);
        ctx->p = p; ctx->last_kind = T_KEYWORD; return T_KEYWORD;
    }
/*--------------------------------------------------------------------------------------------------------------------*/
AST_STR:
    {
        ctx->strbuf[ctx->strpos] = '\0';
        int n = ctx->strpos;
        if (n >= (int)sizeof(ctx->text)) n = (int)sizeof(ctx->text) - 1;
        memcpy(ctx->text, ctx->strbuf, n);
        ctx->text[n] = '\0';
        yylval->str = strdup(ctx->text);
        ctx->p = p; ctx->last_kind = T_STR; return T_STR;
    }
/*--------------------------------------------------------------------------------------------------------------------*/
TT_UNKNOWN:       EMIT_V(T_UNKNOWN);
}
/*--------------------------------------------------------------------------------------------------------------------*/
static const char *sc_name_table[512];
static int         sc_name_table_built = 0;
static void sc_name_table_build(void) {
    sc_name_table[T_INT]              = "T_INT";
    sc_name_table[T_REAL]             = "T_REAL";
    sc_name_table[T_STR]              = "T_STR";
    sc_name_table[T_IDENT]            = "T_IDENT";
    sc_name_table[T_CALL]         = "T_CALL";
    sc_name_table[T_KEYWORD]          = "T_KEYWORD";
    sc_name_table[T_CONCAT]           = "T_CONCAT";
    sc_name_table[T_2EQUAL]       = "T_2EQUAL";
    sc_name_table[T_2QUEST]            = "T_2QUEST";
    sc_name_table[T_2PIPE]      = "T_2PIPE";
    sc_name_table[T_2PLUS]         = "T_2PLUS";
    sc_name_table[T_2MINUS]      = "T_2MINUS";
    sc_name_table[T_2STAR]   = "T_2STAR";
    sc_name_table[T_2SLASH]         = "T_2SLASH";
    sc_name_table[T_2CARET]   = "T_2CARET";
    sc_name_table[T_2DOLLAR] = "T_2DOLLAR";
    sc_name_table[T_2DOT]      = "T_2DOT";
    sc_name_table[T_2AMP]        = "T_2AMP";
    sc_name_table[T_2AT]          = "T_2AT";
    sc_name_table[T_2POUND]            = "T_2POUND";
    sc_name_table[T_2PERCENT]          = "T_2PERCENT";
    sc_name_table[T_2TILDE]            = "T_2TILDE";
    sc_name_table[T_EQ]               = "T_EQ";
    sc_name_table[T_NE]               = "T_NE";
    sc_name_table[T_LT]               = "T_LT";
    sc_name_table[T_GT]               = "T_GT";
    sc_name_table[T_LE]               = "T_LE";
    sc_name_table[T_GE]               = "T_GE";
    sc_name_table[T_LEQ]              = "T_LEQ";
    sc_name_table[T_LNE]              = "T_LNE";
    sc_name_table[T_LLT]              = "T_LLT";
    sc_name_table[T_LGT]              = "T_LGT";
    sc_name_table[T_LLE]              = "T_LLE";
    sc_name_table[T_LGE]              = "T_LGE";
    sc_name_table[T_IDENT_OP]         = "T_IDENT_OP";
    sc_name_table[T_DIFFER]           = "T_DIFFER";
    sc_name_table[T_PLUS_ASSIGN]      = "T_PLUS_ASSIGN";
    sc_name_table[T_MINUS_ASSIGN]     = "T_MINUS_ASSIGN";
    sc_name_table[T_STAR_ASSIGN]      = "T_STAR_ASSIGN";
    sc_name_table[T_SLASH_ASSIGN]     = "T_SLASH_ASSIGN";
    sc_name_table[T_CARET_ASSIGN]     = "T_CARET_ASSIGN";
    sc_name_table[T_1PLUS]          = "T_1PLUS";
    sc_name_table[T_1MINUS]         = "T_1MINUS";
    sc_name_table[T_1STAR]      = "T_1STAR";
    sc_name_table[T_1SLASH]         = "T_1SLASH";
    sc_name_table[T_1PERCENT]       = "T_1PERCENT";
    sc_name_table[T_1AT]       = "T_1AT";
    sc_name_table[T_1TILDE]         = "T_1TILDE";
    sc_name_table[T_1DOLLAR]   = "T_1DOLLAR";
    sc_name_table[T_1DOT]        = "T_1DOT";
    sc_name_table[T_1POUND]         = "T_1POUND";
    sc_name_table[T_1PIPE]  = "T_1PIPE";
    sc_name_table[T_1EQUAL]         = "T_1EQUAL";
    sc_name_table[T_1QUEST] = "T_1QUEST";
    sc_name_table[T_1AMP]     = "T_1AMP";
    sc_name_table[T_1BANG]    = "T_1BANG";
    sc_name_table[T_LPAREN]           = "T_LPAREN";
    sc_name_table[T_RPAREN]           = "T_RPAREN";
    sc_name_table[T_LBRACE]           = "T_LBRACE";
    sc_name_table[T_RBRACE]           = "T_RBRACE";
    sc_name_table[T_LBRACK]           = "T_LBRACK";
    sc_name_table[T_RBRACK]           = "T_RBRACK";
    sc_name_table[T_COMMA]            = "T_COMMA";
    sc_name_table[T_SEMICOLON]        = "T_SEMICOLON";
    sc_name_table[T_COLON]            = "T_COLON";
    sc_name_table[T_IF]            = "T_IF";
    sc_name_table[T_ELSE]          = "T_ELSE";
    sc_name_table[T_WHILE]         = "T_WHILE";
    sc_name_table[T_DO]            = "T_DO";
    sc_name_table[T_FOR]           = "T_FOR";
    sc_name_table[T_SWITCH]        = "T_SWITCH";
    sc_name_table[T_CASE]          = "T_CASE";
    sc_name_table[T_DEFAULT]       = "T_DEFAULT";
    sc_name_table[T_BREAK]         = "T_BREAK";
    sc_name_table[T_CONTINUE]      = "T_CONTINUE";
    sc_name_table[T_GOTO]          = "T_GOTO";
    sc_name_table[T_DEFINE]      = "T_DEFINE";
    sc_name_table[T_RETURN]        = "T_RETURN";
    sc_name_table[T_FRETURN]       = "T_FRETURN";
    sc_name_table[T_NRETURN]       = "T_NRETURN";
    sc_name_table[T_STRUCT]        = "T_STRUCT";
    sc_name_table[T_UNKNOWN]          = "T_UNKNOWN";
    sc_name_table_built               = 1;
}
const char *sc2_kind_name(int kind) {
    if (!sc_name_table_built) sc_name_table_build();
    if (kind < 0 || kind >= 512) return "T_???";
    const char *s = sc_name_table[kind];
    return s ? s : "T_???";
}
