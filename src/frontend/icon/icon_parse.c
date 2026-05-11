/*
 * icon_parse.c — Tiny-ICON recursive-descent parser → IR direct
 *
 * Grammar (explicit-semicolon Icon, Tier 0 + procedure shell):
 *
 *   file       := (proc | record | global)* EOF
 *   proc       := 'procedure' IDENT '(' params ')' stmt* 'end'
 *   params     := (IDENT (',' IDENT)*)?
 *   stmt       := expr ';'
 *              |  'every' expr [do_clause] ';'
 *              |  'if' expr 'then' expr ['else' expr] ';'
 *              |  'while' expr [do_clause] ';'
 *              |  'until' expr [do_clause] ';'
 *              |  'repeat' expr ';'
 *              |  'return' [expr] ';'
 *              |  'suspend' expr [do_clause] ';'
 *              |  'fail' ';'
 *              |  'break' [expr] ';'
 *              |  'next' ';'
 *              |  'local' IDENT (',' IDENT)* ';'
 *   do_clause  := 'do' expr
 *
 *   expr       := and_expr
 *   and_expr   := assign_expr ('&' assign_expr)*
 *   assign_expr:= alt_expr (':=' | '<-' | ':=:' | augop) assign_expr
 *              |  alt_expr
 *   alt_expr   := to_expr ('|' to_expr)*
 *   to_expr    := rel_expr ('to' rel_expr ('by' rel_expr)?)?
 *   rel_expr   := concat_expr (relop concat_expr)*
 *   concat_expr:= add_expr ('||' add_expr | '|||' add_expr)*
 *   add_expr   := mul_expr (('+' | '-') mul_expr)*
 *   mul_expr   := unary_expr (('*' | '/' | '%') unary_expr)*
 *   unary_expr := ('-' | '!' | '\\' | '~' | 'not') unary_expr
 *              |  limit_expr
 *   limit_expr := postfix_expr ('\\' unary_expr)?
 *   postfix_expr:= primary ('[' expr ']' | '.' IDENT | '(' args ')' )*
 *   primary    := INT | REAL | STRING | CSET | IDENT
 *              |  '(' expr ')'
 *              |  '&' IDENT   (keyword)
 *
 * FI-2: Produces tree_t / STMT_t directly — IcnNode/icon_ast eliminated.
 * Authors: Claude Sonnet 4.6 (FI-2, 2026-04-14)
 */

#include "icon_parse.h"
#include "../ast/ast.h"
#include "../snobol4/scrip_cc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* =========================================================================
 * Internal helpers — mirrors icon_lower.c helpers, now inline in parser
 * ======================================================================= */

static tree_t *e_leaf_sval(AST_e k, const char *s, int len) {
    tree_t *e = expr_new(k);
    if (len >= 0) e->v.sval = intern_n(s, len);
    else          e->v.sval = intern(s);
    return e;
}

static tree_t *e_unary(AST_e k, tree_t *child) {
    return expr_unary(k, child);
}

static tree_t *e_binary(AST_e k, tree_t *left, tree_t *right) {
    return expr_binary(k, left, right);
}

/* =========================================================================
 * Parser machinery
 * ======================================================================= */

static void parser_error(IcnParser *p, const char *msg) {
    if (!p->had_error) {
        snprintf(p->errmsg, sizeof(p->errmsg),
                 "line %d: %s (got %s)", p->cur.line, msg, icn_tk_name(p->cur.t));
        p->had_error = 1;
    }
}

static IcnToken advance(IcnParser *p) {
    p->prev_kind = p->cur.t;
    p->cur  = p->peek;
    p->peek = icn_lex_next(p->lex);
    return p->cur;
}

static int check(IcnParser *p, IcnTkKind kind) { return p->cur.t == kind; }

static int match(IcnParser *p, IcnTkKind kind) {
    if (p->cur.t == kind) { advance(p); return 1; }
    return 0;
}

static int expect(IcnParser *p, IcnTkKind kind, const char *ctx) {
    if (p->cur.t == kind) { advance(p); return 1; }
    char msg[128];
    snprintf(msg, sizeof(msg), "%s: expected %s", ctx, icn_tk_name(kind));
    parser_error(p, msg);
    return 0;
}

/* =========================================================================
 * Forward declarations
 * ======================================================================= */
static tree_t *parse_expr(IcnParser *p);
static tree_t *parse_stmt(IcnParser *p);
static tree_t *parse_do_clause(IcnParser *p);
static tree_t *parse_block_or_expr(IcnParser *p);

/* =========================================================================
 * Append helper — add expr child to n-ary node
 * ======================================================================= */
static void push_child(tree_t *parent, tree_t *child) {
    expr_add_child(parent, child);
}

/* =========================================================================
 * Expression parsing (recursive descent) → tree_t direct
 * ======================================================================= */

static tree_t *parse_primary(IcnParser *p) {
    int line = p->cur.line;
    IcnToken t = p->cur;

    if (t.t == TK_INT) {
        advance(p);
        tree_t *e = expr_new(AST_ILIT);
        e->v.ival = t.val.v.ival;
        return e;
    }
    if (t.t == TK_REAL) {
        advance(p);
        tree_t *e = expr_new(AST_FLIT);
        e->v.dval = t.val.fval;
        return e;
    }
    if (t.t == TK_STRING) {
        advance(p);
        return e_leaf_sval(AST_QLIT, t.val.v.sval.data, (int)t.val.v.sval.len);
    }
    if (t.t == TK_CSET) {
        advance(p);
        return e_leaf_sval(AST_CSET, t.val.v.sval.data, (int)t.val.v.sval.len);
    }
    if (t.t == TK_IDENT) {
        advance(p);
        return e_leaf_sval(AST_VAR, t.val.v.sval.data, (int)t.val.v.sval.len);
    }
    if (t.t == TK_AND) {
        /* &keyword */
        advance(p);
        const char *kwname = NULL;
        if (p->cur.t == TK_IDENT) {
            kwname = p->cur.val.v.sval.data;
        } else {
            kwname = icn_tk_name(p->cur.t);
            int ok = 1;
            for (const char *c = kwname; *c; c++)
                if (!isalpha((unsigned char)*c) && *c != '_') { ok = 0; break; }
            if (!ok) kwname = NULL;
        }
        if (!kwname) { parser_error(p, "expected keyword name after &"); return NULL; }
        char name[256]; snprintf(name, sizeof(name), "&%s", kwname);
        advance(p);
        return e_leaf_sval(AST_VAR, name, -1);
    }
    if (t.t == TK_LPAREN) {
        advance(p);
        tree_t *first = parse_expr(p);
        if (check(p, TK_SEMICOL)) {
            /* (E1; E2; ...) — expression sequence → AST_SEQ_EXPR */
            tree_t *seq = expr_new(AST_SEQ_EXPR);
            push_child(seq, first);
            while (check(p, TK_SEMICOL)) {
                advance(p);
                if (check(p, TK_RPAREN)) break;
                push_child(seq, parse_expr(p));
            }
            expect(p, TK_RPAREN, "sequence expression");
            return seq;
        }
        expect(p, TK_RPAREN, "grouped expression");
        return first;
    }
    if (t.t == TK_LBRACK) {
        /* [e1, e2, ...] — list constructor → AST_MAKELIST */
        advance(p);
        tree_t *lst = expr_new(AST_MAKELIST);
        if (!check(p, TK_RBRACK)) {
            push_child(lst, parse_expr(p));
            while (check(p, TK_COMMA)) {
                advance(p);
                if (check(p, TK_RBRACK)) break;
                push_child(lst, parse_expr(p));
            }
        }
        expect(p, TK_RBRACK, "list literal");
        return lst;
    }
    if (t.t == TK_FAIL) {
        advance(p);
        return expr_new(AST_PROC_FAIL);
    }
    if (t.t == TK_BREAK) {
        advance(p);
        tree_t *e = expr_new(AST_LOOP_BREAK);
        if (!check(p, TK_SEMICOL) && !check(p, TK_RPAREN) && !check(p, TK_EOF))
            push_child(e, parse_expr(p));
        return e;
    }
    if (t.t == TK_NEXT) {
        advance(p);
        return expr_new(AST_LOOP_NEXT);
    }
    if (t.t == TK_CASE) {
        return parse_expr(p);
    }
    if (t.t == TK_LBRACE) {
        return parse_block_or_expr(p);
    }

    parser_error(p, "expected expression");
    advance(p);
    return NULL;
}

static tree_t *parse_postfix(IcnParser *p) {
    tree_t *n = parse_primary(p);
    if (!n) return NULL;
    for (;;) {
        int line = p->cur.line; (void)line;
        if (check(p, TK_LPAREN)) {
            advance(p);
            /* AST_FNC: child[0]=callee, child[1..]=args */
            tree_t *call = expr_new(AST_FNC);
            push_child(call, n);
            if (!check(p, TK_RPAREN)) {
                do {
                    tree_t *arg;
                    if (check(p, TK_COMMA) || check(p, TK_RPAREN))
                        arg = e_leaf_sval(AST_VAR, "&null", -1);
                    else {
                        arg = parse_expr(p);
                        if (!arg) break;
                    }
                    push_child(call, arg);
                } while (match(p, TK_COMMA));
            }
            expect(p, TK_RPAREN, "function call");
            n = call;
        } else if (check(p, TK_LBRACK)) {
            advance(p);
            tree_t *idx = parse_expr(p);
            if (check(p, TK_COLON)) {
                advance(p);
                tree_t *hi = parse_expr(p);
                expect(p, TK_RBRACK, "section");
                tree_t *sec = expr_new(AST_SECTION);
                push_child(sec, n); push_child(sec, idx); push_child(sec, hi);
                n = sec;
            } else if (check(p, TK_PLUSCOLON)) {
                advance(p);
                tree_t *len = parse_expr(p);
                expect(p, TK_RBRACK, "section+:");
                tree_t *sec = expr_new(AST_SECTION_PLUS);
                push_child(sec, n); push_child(sec, idx); push_child(sec, len);
                n = sec;
            } else if (check(p, TK_MINUSCOLON)) {
                advance(p);
                tree_t *len = parse_expr(p);
                expect(p, TK_RBRACK, "section-:");
                tree_t *sec = expr_new(AST_SECTION_MINUS);
                push_child(sec, n); push_child(sec, idx); push_child(sec, len);
                n = sec;
            } else {
                expect(p, TK_RBRACK, "subscript");
                n = e_binary(AST_IDX, n, idx);
            }
        } else if (check(p, TK_DOT)) {
            advance(p);
            if (p->cur.t != TK_IDENT) { parser_error(p, "expected field name"); break; }
            IcnToken fname = p->cur; advance(p);
            /* AST_FIELD: sval=field name, child[0]=object */
            tree_t *fe = expr_new(AST_FIELD);
            fe->v.sval = intern_n(fname.val.v.sval.data, (int)fname.val.v.sval.len);
            push_child(fe, n);
            n = fe;
        } else {
            break;
        }
    }
    return n;
}

static tree_t *parse_unary(IcnParser *p);

static tree_t *parse_limit(IcnParser *p) {
    tree_t *n = parse_postfix(p);
    if (!n) return NULL;
    if (check(p, TK_BACKSLASH)) {
        advance(p);
        tree_t *lim = parse_unary(p);
        n = e_binary(AST_LIMIT, n, lim);
    }
    return n;
}

static tree_t *parse_unary(IcnParser *p) {
    int line = p->cur.line; (void)line;
    if (check(p, TK_MINUS))     { advance(p); return e_unary(AST_MNS,        parse_unary(p)); }
    if (check(p, TK_PLUS))      { advance(p); return e_unary(AST_PLS,        parse_unary(p)); }
    if (check(p, TK_BANG))      { advance(p); return e_unary(AST_ITERATE,    parse_unary(p)); }
    if (check(p, TK_STAR))      { advance(p); return e_unary(AST_SIZE,       parse_unary(p)); }
    if (check(p, TK_BACKSLASH)) { advance(p); return e_unary(AST_NONNULL,    parse_unary(p)); }
    if (check(p, TK_SLASH))     { advance(p); return e_unary(AST_NULL,       parse_unary(p)); }
    if (check(p, TK_NOT))       { advance(p); return e_unary(AST_NOT,        parse_unary(p)); }
    if (check(p, TK_QMARK))     { advance(p); return e_unary(AST_RANDOM,     parse_unary(p)); }
    if (check(p, TK_TILDE))     { advance(p); return e_unary(AST_CSET_COMPL, parse_unary(p)); }
    if (check(p, TK_EQ)) {
        /* =E — scan match: rewrite as match(E) call */
        advance(p);
        tree_t *inner = parse_unary(p);
        tree_t *call = expr_new(AST_FNC);
        push_child(call, e_leaf_sval(AST_VAR, "match", -1));
        push_child(call, inner);
        return call;
    }
    return parse_limit(p);
}

static tree_t *parse_pow(IcnParser *p) {
    tree_t *n = parse_unary(p);
    if (!n) return NULL;
    if (check(p, TK_CARET)) {
        advance(p);
        tree_t *rhs = parse_pow(p);   /* right-associative */
        n = e_binary(AST_POW, n, rhs);
    }
    return n;
}

static tree_t *parse_mul(IcnParser *p) {
    tree_t *n = parse_pow(p);
    if (!n) return NULL;
    for (;;) {
        AST_e k;
        if      (check(p, TK_STAR))  k = AST_MUL;
        else if (check(p, TK_SLASH)) k = AST_DIV;
        else if (check(p, TK_MOD))   k = AST_MOD;
        else break;
        advance(p);
        n = e_binary(k, n, parse_pow(p));
    }
    return n;
}

static tree_t *parse_add(IcnParser *p) {
    tree_t *n = parse_mul(p);
    if (!n) return NULL;
    for (;;) {
        AST_e k;
        if      (check(p, TK_PLUS))  k = AST_ADD;
        else if (check(p, TK_MINUS)) k = AST_SUB;
        else break;
        advance(p);
        n = e_binary(k, n, parse_mul(p));
    }
    return n;
}

static tree_t *parse_cset(IcnParser *p) {
    tree_t *n = parse_add(p);
    if (!n) return NULL;
    for (;;) {
        AST_e k;
        if      (check(p, TK_PLUSPLUS))   k = AST_CSET_UNION;
        else if (check(p, TK_MINUSMINUS)) k = AST_CSET_DIFF;
        else if (check(p, TK_STARSTAR))   k = AST_CSET_INTER;
        else if (check(p, TK_BANG)) {
            /* binary !: E1 ! E2 → AST_BANG_BINARY */
            advance(p);
            n = e_binary(AST_BANG_BINARY, n, parse_add(p));
            continue;
        }
        else break;
        advance(p);
        n = e_binary(k, n, parse_add(p));
    }
    return n;
}

static tree_t *parse_concat(IcnParser *p) {
    tree_t *n = parse_cset(p);
    if (!n) return NULL;
    for (;;) {
        AST_e k;
        if      (check(p, TK_LCONCAT)) k = AST_LCONCAT;
        else if (check(p, TK_CONCAT))  k = AST_CAT;
        else break;
        advance(p);
        n = e_binary(k, n, parse_cset(p));
    }
    return n;
}

static int is_relop(IcnTkKind k) {
    return k==TK_LT || k==TK_LE || k==TK_GT || k==TK_GE ||
           k==TK_EQ || k==TK_NEQ ||
           k==TK_SLT || k==TK_SLE || k==TK_SGT || k==TK_SGE ||
           k==TK_SEQ || k==TK_SNE;
}

static AST_e relop_ekind(IcnTkKind k) {
    switch (k) {
        case TK_LT:  return AST_LT;   case TK_LE:  return AST_LE;
        case TK_GT:  return AST_GT;   case TK_GE:  return AST_GE;
        case TK_EQ:  return AST_EQ;   case TK_NEQ: return AST_NE;
        case TK_SLT: return AST_LLT;  case TK_SLE: return AST_LLE;
        case TK_SGT: return AST_LGT;  case TK_SGE: return AST_LGE;
        case TK_SEQ: return AST_LEQ;  case TK_SNE: return AST_LNE;
        default:     return AST_EQ;
    }
}

static tree_t *parse_rel(IcnParser *p) {
    tree_t *n = parse_concat(p);
    if (!n) return NULL;
    while (is_relop(p->cur.t)) {
        AST_e k = relop_ekind(p->cur.t);
        advance(p);
        n = e_binary(k, n, parse_concat(p));
    }
    return n;
}

static tree_t *parse_assign(IcnParser *p);  /* forward — parse_and calls parse_assign */

static tree_t *parse_and(IcnParser *p) {
    tree_t *n = parse_assign(p);
    if (!n) return NULL;
    if (!check(p, TK_AND)) return n;
    /* n-ary AST_SEQ (conjunction, same Byrd-box semantics as & in Icon) */
    tree_t *seq = expr_new(AST_SEQ);
    push_child(seq, n);
    while (check(p, TK_AND)) {
        advance(p);
        push_child(seq, parse_assign(p));
    }
    return seq;
}

static tree_t *parse_to(IcnParser *p) {
    tree_t *n = parse_rel(p);
    if (!n) return NULL;
    while (check(p, TK_TO)) {
        advance(p);
        tree_t *limit = parse_rel(p);
        if (check(p, TK_BY)) {
            advance(p);
            tree_t *step = parse_rel(p);
            tree_t *tby = expr_new(AST_TO_BY);
            push_child(tby, n); push_child(tby, limit); push_child(tby, step);
            n = tby;
        } else {
            n = e_binary(AST_TO, n, limit);
        }
    }
    return n;
}

static tree_t *parse_alt(IcnParser *p) {
    tree_t *n = parse_to(p);
    if (!n) return NULL;
    if (!check(p, TK_BAR)) return n;
    /* n-ary AST_ALTERNATE */
    tree_t *alt = expr_new(AST_ALTERNATE);
    push_child(alt, n);
    while (check(p, TK_BAR)) {
        advance(p);
        push_child(alt, parse_to(p));
    }
    return alt;
}

static int is_augop(IcnTkKind k) {
    return k==TK_AUGPLUS || k==TK_AUGMINUS || k==TK_AUGSTAR ||
           k==TK_AUGSLASH || k==TK_AUGMOD  || k==TK_AUGPOW  || k==TK_AUGCONCAT ||
           k==TK_AUGCSET_UNION || k==TK_AUGCSET_DIFF || k==TK_AUGCSET_INTER ||
           k==TK_AUGSCAN ||
           k==TK_AUGEQ    || k==TK_AUGSEQ  ||
           k==TK_AUGLT    || k==TK_AUGLE   || k==TK_AUGGT  || k==TK_AUGGE || k==TK_AUGNE ||
           k==TK_AUGSLT   || k==TK_AUGSLE  || k==TK_AUGSGT || k==TK_AUGSGE || k==TK_AUGSNE;
}

static tree_t *parse_assign(IcnParser *p) {
    tree_t *n = parse_alt(p);
    if (!n) return NULL;
    if (check(p, TK_ASSIGN)) {
        advance(p);
        return e_binary(AST_ASSIGN, n, parse_assign(p));
    }
    if (check(p, TK_REVASSIGN)) {
        advance(p);
        return e_binary(AST_REVASSIGN, n, parse_assign(p));
    }
    if (check(p, TK_SWAP)) {
        advance(p);
        return e_binary(AST_SWAP, n, parse_assign(p));
    }
    if (check(p, TK_VALSWAP)) {
        advance(p);
        return e_binary(AST_REVSWAP, n, parse_assign(p));
    }
    if (check(p, TK_IDENTICAL)) {
        advance(p);
        return e_binary(AST_IDENTICAL, n, parse_assign(p));
    }
    if (check(p, TK_NOTIDENT)) {
        advance(p);
        return e_unary(AST_NOT, e_binary(AST_IDENTICAL, n, parse_assign(p)));
    }
    if (is_augop(p->cur.t)) {
        IcnTkKind aug = p->cur.t; advance(p);
        tree_t *rhs = parse_assign(p);
        tree_t *op = expr_new(AST_AUGOP);
        op->v.ival = (long)aug;
        push_child(op, n); push_child(op, rhs);
        return op;
    }
    if (check(p, TK_QMARK)) {
        advance(p);
        return e_binary(AST_SCAN, n, parse_block_or_expr(p));
    }
    return n;
}

static tree_t *parse_expr(IcnParser *p) {
    int line = p->cur.line; (void)line;
    /* Control expressions valid anywhere */
    if (check(p, TK_RETURN)) {
        advance(p);
        tree_t *e = expr_new(AST_RETURN);
        if (!check(p, TK_SEMICOL) && !check(p, TK_RPAREN) &&
            !check(p, TK_EOF)  && !check(p, TK_THEN) &&
            !check(p, TK_ELSE) && !check(p, TK_DO))
            push_child(e, parse_expr(p));
        return e;
    }
    if (check(p, TK_FAIL)) {
        advance(p);
        return expr_new(AST_PROC_FAIL);
    }
    if (check(p, TK_SUSPEND)) {
        advance(p);
        tree_t *e = expr_new(AST_SUSPEND);
        push_child(e, parse_expr(p));
        tree_t *body = parse_do_clause(p);
        if (body) push_child(e, body);
        return e;
    }
    if (check(p, TK_BREAK)) { advance(p); return expr_new(AST_LOOP_BREAK); }
    if (check(p, TK_NEXT))  { advance(p); return expr_new(AST_LOOP_NEXT); }
    if (check(p, TK_IF)) {
        advance(p);
        tree_t *e = expr_new(AST_IF);
        push_child(e, parse_expr(p));
        match(p, TK_SEMICOL);
        expect(p, TK_THEN, "if/then");
        push_child(e, parse_block_or_expr(p));
        match(p, TK_SEMICOL);
        if (match(p, TK_ELSE)) push_child(e, parse_block_or_expr(p));
        return e;
    }
    if (check(p, TK_EVERY)) {
        advance(p);
        tree_t *e = expr_new(AST_EVERY);
        push_child(e, parse_expr(p));
        tree_t *body = parse_do_clause(p);
        if (body) push_child(e, body);
        return e;
    }
    if (check(p, TK_WHILE)) {
        advance(p);
        tree_t *e = expr_new(AST_WHILE);
        push_child(e, parse_expr(p));
        tree_t *body = parse_do_clause(p);
        if (body) push_child(e, body);
        return e;
    }
    if (check(p, TK_UNTIL)) {
        advance(p);
        tree_t *e = expr_new(AST_UNTIL);
        push_child(e, parse_expr(p));
        tree_t *body = parse_do_clause(p);
        if (body) push_child(e, body);
        return e;
    }
    if (check(p, TK_REPEAT)) {
        advance(p);
        tree_t *e = expr_new(AST_REPEAT);
        push_child(e, parse_block_or_expr(p));
        return e;
    }
    if (check(p, TK_CASE)) {
        advance(p);
        tree_t *e = expr_new(AST_CASE);
        push_child(e, parse_expr(p));     /* dispatch expr */
        expect(p, TK_OF, "case expression");
        expect(p, TK_LBRACE, "case body");
        while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
            if (check(p, TK_DEFAULT)) {
                advance(p);
                expect(p, TK_COLON, "case default");
                push_child(e, parse_expr(p));
                match(p, TK_SEMICOL);
                break;
            }
            push_child(e, parse_expr(p));      /* case value */
            expect(p, TK_COLON, "case clause");
            push_child(e, parse_expr(p));      /* case result */
            match(p, TK_SEMICOL);
        }
        expect(p, TK_RBRACE, "case body end");
        return e;
    }
    return parse_and(p);
}

/* =========================================================================
 * Block / do-clause helpers
 * ======================================================================= */

static tree_t *parse_block_or_expr(IcnParser *p) {
    if (!check(p, TK_LBRACE)) return parse_expr(p);
    advance(p);
    tree_t *seq = expr_new(AST_SEQ_EXPR);
    int nc = 0;
    while (!check(p, TK_RBRACE) && !check(p, TK_EOF)) {
        tree_t *s = parse_stmt(p);
        if (!s) break;
        push_child(seq, s);
        nc++;
    }
    expect(p, TK_RBRACE, "compound block");
    if (nc == 1) {
        /* unwrap single-child seq — steal child, free wrapper */
        tree_t *only = seq->c[0];
        seq->n = 0;
        /* expr_free(seq) would free children too; just free the node shell */
        free(seq);
        return only;
    }
    return seq;
}

static tree_t *parse_do_clause(IcnParser *p) {
    if (check(p, TK_DO)) { advance(p); return parse_block_or_expr(p); }
    return NULL;
}

/* =========================================================================
 * Statement parsing
 * ======================================================================= */

static tree_t *parse_stmt(IcnParser *p) {
    if (check(p, TK_EVERY)) {
        advance(p);
        tree_t *e = expr_new(AST_EVERY);
        push_child(e, parse_expr(p));
        tree_t *body = parse_do_clause(p);
        if (body) push_child(e, body);
        match(p, TK_SEMICOL);
        return e;
    }
    if (check(p, TK_IF)) {
        advance(p);
        tree_t *e = expr_new(AST_IF);
        push_child(e, parse_expr(p));
        match(p, TK_SEMICOL);
        expect(p, TK_THEN, "if/then");
        push_child(e, parse_block_or_expr(p));
        match(p, TK_SEMICOL);
        if (match(p, TK_ELSE)) push_child(e, parse_block_or_expr(p));
        match(p, TK_SEMICOL);
        return e;
    }
    if (check(p, TK_WHILE)) {
        advance(p);
        tree_t *e = expr_new(AST_WHILE);
        push_child(e, parse_expr(p));
        tree_t *body = parse_do_clause(p);
        if (body) push_child(e, body);
        match(p, TK_SEMICOL);
        return e;
    }
    if (check(p, TK_UNTIL)) {
        advance(p);
        tree_t *e = expr_new(AST_UNTIL);
        push_child(e, parse_expr(p));
        tree_t *body = parse_do_clause(p);
        if (body) push_child(e, body);
        match(p, TK_SEMICOL);
        return e;
    }
    if (check(p, TK_REPEAT)) {
        advance(p);
        tree_t *e = expr_new(AST_REPEAT);
        push_child(e, parse_block_or_expr(p));
        match(p, TK_SEMICOL);
        return e;
    }
    if (check(p, TK_RETURN)) {
        advance(p);
        tree_t *e = expr_new(AST_RETURN);
        if (!check(p, TK_SEMICOL)) push_child(e, parse_expr(p));
        expect(p, TK_SEMICOL, "return statement");
        return e;
    }
    if (check(p, TK_SUSPEND)) {
        advance(p);
        tree_t *e = expr_new(AST_SUSPEND);
        push_child(e, parse_expr(p));
        tree_t *body = parse_do_clause(p);
        if (body) push_child(e, body);
        expect(p, TK_SEMICOL, "suspend statement");
        return e;
    }
    if (check(p, TK_FAIL)) {
        advance(p);
        expect(p, TK_SEMICOL, "fail statement");
        return expr_new(AST_PROC_FAIL);
    }
    if (check(p, TK_INITIAL)) {
        advance(p);
        tree_t *e = expr_new(AST_INITIAL);
        push_child(e, parse_block_or_expr(p));
        match(p, TK_SEMICOL);
        return e;
    }
    if (check(p, TK_CASE)) {
        tree_t *e = parse_expr(p);
        match(p, TK_SEMICOL);
        return e;
    }
    if (check(p, TK_LOCAL) || check(p, TK_STATIC)) {
        int is_static = check(p, TK_STATIC);
        advance(p);
        tree_t *e = expr_new(AST_GLOBAL);
        e->v.ival = is_static ? 1 : 0;  /* ival=1 marks "static" — vars persist across calls */
        while (!check(p, TK_SEMICOL) && !check(p, TK_EOF)) {
            if (p->cur.t == TK_IDENT) {
                push_child(e, e_leaf_sval(AST_VAR, p->cur.val.v.sval.data, (int)p->cur.val.v.sval.len));
                advance(p);
            }
            if (!match(p, TK_COMMA)) break;
        }
        match(p, TK_SEMICOL);
        return e;
    }
    /* Expression statement */
    tree_t *e = parse_expr(p);
    /* IC-9 (2026-05-01): Icon allows omitting `;` between an expression statement
     * that ended with `}` (block-as-expr — `expr ? { … }`, `if … then { … } else { … }`,
     * `case … of { … }`, etc.) and the following statement.  Mirrors Icon's actual
     * lexical rule and matches the JCON test corpus. */
    if (!check(p, TK_RBRACE) && !check(p, TK_EOF) &&
        !check(p, TK_END)    && !check(p, TK_ELSE) && !check(p, TK_THEN) &&
        !check(p, TK_RETURN) && !check(p, TK_SUSPEND) &&
        p->prev_kind != TK_RBRACE)
        expect(p, TK_SEMICOL, "expression statement");
    else
        match(p, TK_SEMICOL);
    return e;
}

/* =========================================================================
 * Record declaration
 *   record Name(field1, field2, ...)
 * Produces AST_RECORD node: sval=type name, children=AST_VAR field nodes
 * ======================================================================= */

static tree_t *parse_record(IcnParser *p) {
    expect(p, TK_RECORD, "record");
    if (p->cur.t != TK_IDENT) { parser_error(p, "expected record name"); return NULL; }
    IcnToken name_tok = p->cur; advance(p);
    tree_t *e = expr_new(AST_RECORD);
    e->v.sval = intern_n(name_tok.val.v.sval.data, (int)name_tok.val.v.sval.len);
    expect(p, TK_LPAREN, "record fields");
    while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
        if (p->cur.t == TK_IDENT) {
            push_child(e, e_leaf_sval(AST_VAR, p->cur.val.v.sval.data, (int)p->cur.val.v.sval.len));
            advance(p);
        }
        if (!match(p, TK_COMMA)) break;
    }
    expect(p, TK_RPAREN, "record fields");
    match(p, TK_SEMICOL);
    return e;
}

/* =========================================================================
 * Procedure parsing → AST_FNC
 *
 * AST_FNC layout (matches icon_lower.c ICN_PROC case, expected by coro_call):
 *   e->v.sval          = proc name
 *   e->v.ival          = nparams
 *   e->c[0]   = AST_VAR (proc name)
 *   e->c[1..nparams] = AST_VAR param nodes
 *   e->c[nparams+1..] = body tree_t statements
 * ======================================================================= */

static tree_t *parse_proc(IcnParser *p) {
    expect(p, TK_PROCEDURE, "procedure");
    if (p->cur.t != TK_IDENT) { parser_error(p, "expected procedure name"); return NULL; }
    IcnToken name_tok = p->cur; advance(p);

    /* params */
    tree_t **params = NULL; int nparams = 0, pcap = 0;
    expect(p, TK_LPAREN, "procedure params");
    while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
        if (p->cur.t == TK_IDENT) {
            if (nparams+1 > pcap) { pcap = pcap ? pcap*2 : 4; params = realloc(params, pcap*sizeof(tree_t*)); }
            params[nparams++] = e_leaf_sval(AST_VAR, p->cur.val.v.sval.data, (int)p->cur.val.v.sval.len);
            advance(p);
            if (check(p, TK_LBRACK)) { advance(p); match(p, TK_RBRACK); break; }
        }
        if (!match(p, TK_COMMA)) break;
    }
    expect(p, TK_RPAREN, "procedure params");
    /* IC-9 (2026-05-01): Icon allows an optional `;` after the procedure header
     * (`procedure ws();` followed by body — a JCON-style idiom). */
    match(p, TK_SEMICOL);

    /* body stmts */
    tree_t **stmts = NULL; int nstmts = 0, scap = 0;
    while (!check(p, TK_END) && !check(p, TK_EOF) && !p->had_error) {
        tree_t *s = parse_stmt(p);
        if (s) {
            if (nstmts+1 > scap) { scap = scap ? scap*2 : 8; stmts = realloc(stmts, scap*sizeof(tree_t*)); }
            stmts[nstmts++] = s;
        }
    }
    expect(p, TK_END, "end of procedure");

    /* Build AST_FNC */
    tree_t *proc = expr_new(AST_FNC);
    proc->v.sval = intern_n(name_tok.val.v.sval.data, (int)name_tok.val.v.sval.len);
    proc->v.ival = nparams;
    /* child[0]: name node */
    push_child(proc, e_leaf_sval(AST_VAR, proc->v.sval, -1));
    /* children[1..nparams]: param nodes */
    for (int i = 0; i < nparams; i++) push_child(proc, params[i]);
    /* children[nparams+1..]: body stmts */
    for (int i = 0; i < nstmts; i++) push_child(proc, stmts[i]);
    free(params); free(stmts);
    return proc;
}

/* =========================================================================
 * Public API
 * ======================================================================= */

void icn_parse_init(IcnParser *p, IcnLexer *lex) {
    memset(p, 0, sizeof(*p));
    p->lex  = lex;
    p->cur  = icn_lex_next(lex);
    p->peek = icn_lex_next(lex);
}

CODE_t *icn_parse_file(IcnParser *p, tree_t **out_ast) {
    CODE_t *prog     = calloc(1, sizeof(CODE_t));
    /* SI-5: build AST_PROGRAM in parallel with CODE_t */
    tree_t  *ast_prog = ast_stmt_new(AST_PROGRAM);
    while (!check(p, TK_EOF) && !p->had_error) {
        tree_t *top = NULL;
        if (check(p, TK_PROCEDURE)) {
            top = parse_proc(p);
        } else if (check(p, TK_RECORD)) {
            top = parse_record(p);
        } else if (check(p, TK_GLOBAL)) {
            advance(p);
            top = expr_new(AST_GLOBAL);
            while (!check(p, TK_SEMICOL) && !check(p, TK_EOF)) {
                if (p->cur.t == TK_IDENT) {
                    push_child(top, e_leaf_sval(AST_VAR, p->cur.val.v.sval.data, (int)p->cur.val.v.sval.len));
                    advance(p);
                }
                if (!match(p, TK_COMMA)) break;
            }
            match(p, TK_SEMICOL);
        } else {
            parser_error(p, "expected 'procedure', 'record', or 'global'");
            break;
        }
        if (top) {
            STMT_t *st = calloc(1, sizeof(STMT_t));
            st->subject = top;
            st->lineno  = 0;
            st->lang    = LANG_ICN;
            if (!prog->head) prog->head = prog->tail = st;
            else           { prog->tail->next = st; prog->tail = st; }
            prog->nstmts++;
            /* SI-5: pump each top-level item as an AST_STMT into ast_prog.
             * Shape must match stmt_to_ast(): :lang, :line, :stno, :subj. */
            {
                tree_t *ast_st = ast_stmt_new(AST_STMT);
                push_child(ast_st, ast_attr_int(":lang", LANG_ICN));
                push_child(ast_st, ast_attr_int(":line", 0));
                push_child(ast_st, ast_attr_int(":stno", 0));
                push_child(ast_st, ast_attr_expr(":subj", top));
                push_child(ast_prog, ast_st);
            }
        }
    }
    /* No AST_END appended: Icon CODE_t has no is_end sentinel, so code_to_ast
     * produces no AST_END child — we match that to stay byte-identical. */
    if (out_ast) *out_ast = ast_prog;
    return prog;
}

tree_t *icn_parse_expr(IcnParser *p) {
    return parse_expr(p);
}
