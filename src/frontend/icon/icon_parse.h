/*
 * icon_parse.h — Tiny-ICON recursive-descent parser API
 *
 * Parses explicit-semicolon Icon (no auto-insertion).
 * Produces tree_t / STMT_t IR directly — no intermediate AST.
 *
 * FI-2: IcnNode/icon_ast eliminated (2026-04-14).
 */

#ifndef ICON_PARSE_H
#define ICON_PARSE_H

#include "icon_lex.h"
#include "../snobol4/scrip_cc.h"   /* CODE_t, STMT_t, tree_t, LANG_ICN */

/* -------------------------------------------------------------------------
 * Parser state
 * -------------------------------------------------------------------------*/
typedef struct {
    IcnLexer   *lex;
    IcnToken    cur;        /* current (already consumed) token */
    IcnToken    peek;       /* one-token lookahead */
    IcnTkKind   prev_kind;  /* kind of last token consumed by advance() — used
                             * to make `;` optional after a `}` ending an
                             * expression statement (Icon "block-as-expression"
                             * convention; see parse_stmt line ~765). */
    int         had_error;
    char        errmsg[512];
} IcnParser;

/* -------------------------------------------------------------------------
 * API
 * -------------------------------------------------------------------------*/

/* Initialize parser from an already-initialized lexer */
void     icn_parse_init(IcnParser *p, IcnLexer *lex);

/* Parse a complete Icon source file directly to IR.
 * Returns CODE_t* (caller owns) or NULL on parse error. */
CODE_t *icn_parse_file(IcnParser *p, tree_t **out_ast);

/* Parse a single expression to tree_t (useful for unit tests) */
tree_t  *icn_parse_expr(IcnParser *p);

#endif /* ICON_PARSE_H */
