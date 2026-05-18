#include "prolog_parse.h"
#include "prolog_lex.h"
#include "prolog_atom.h"
#include "term.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define MAX_VARS 256
typedef struct {
    char *name;
    Term *term;
} VarEntry;
typedef struct {
    VarEntry entries[MAX_VARS];
    int      count;
} VarScope;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void scope_reset(VarScope *sc) { sc->count = 0; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *scope_get(VarScope *sc, const char *name) {
    for (int i = 0; i < sc->count; i++)
        if (strcmp(sc->entries[i].name, name) == 0)
            return sc->entries[i].term;
    if (sc->count >= MAX_VARS) {
        fprintf(stderr, "too many variables in one clause\n");
        return term_new_var(-1);
    }
    Term *v = term_new_var(-1);
    sc->entries[sc->count].name = strdup(name);
    sc->entries[sc->count].term = v;
    sc->count++;
    return v;
}
#define IF_STACK_MAX 32
typedef struct {
    int active;
    int taken;
    int parent_active;
    int line;
} IfFrame;
typedef struct {
    Lexer      lx;
    VarScope   sc;
    const char *filename;
    int         nerrors;
    int         in_args;
    int         tree_mismatches;   /* PST-PL-6c */
    IfFrame     ifst[IF_STACK_MAX];
    int         ifst_top;
} Parser;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int if_currently_active(const Parser *p) {
    if (p->ifst_top == 0) return 1;
    return p->ifst[p->ifst_top - 1].active;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void perror_at(Parser *p, int line, const char *msg) {
    fprintf(stderr, "%s:%d: parse error: %s\n", p->filename, line, msg);
    p->nerrors++;
}
typedef enum { ASSOC_NONE, ASSOC_LEFT, ASSOC_RIGHT } Assoc;
typedef struct { const char *name; int prec; Assoc assoc; } OpEntry;
static const OpEntry BIN_OPS[] = {
    { ":-",   1200, ASSOC_NONE  },
    { "-->",  1200, ASSOC_NONE  },
    { ",",    1000, ASSOC_RIGHT },
    { ";",    1100, ASSOC_RIGHT },
    { "->",   1050, ASSOC_RIGHT },
    { "*->",  1050, ASSOC_RIGHT },
    { "@",     900, ASSOC_NONE  },
    { "=",     700, ASSOC_NONE  },
    { "\\=",   700, ASSOC_NONE  },
    { "==",    700, ASSOC_NONE  },
    { "\\==",  700, ASSOC_NONE  },
    { "is",    700, ASSOC_NONE  },
    { "<",     700, ASSOC_NONE  },
    { ">",     700, ASSOC_NONE  },
    { "=<",    700, ASSOC_NONE  },
    { ">=",    700, ASSOC_NONE  },
    { "=:=",   700, ASSOC_NONE  },
    { "=\\=",  700, ASSOC_NONE  },
    { "=..",   700, ASSOC_NONE  },
    { "=@=",   700, ASSOC_NONE  },
    { "\\=@=", 700, ASSOC_NONE  },
    { "?=",    700, ASSOC_NONE  },
    { "@<",    700, ASSOC_NONE  },
    { "@>",    700, ASSOC_NONE  },
    { "@=<",   700, ASSOC_NONE  },
    { "@>=",   700, ASSOC_NONE  },
    { "+",     500, ASSOC_LEFT  },
    { "-",     500, ASSOC_LEFT  },
    { "*",     400, ASSOC_LEFT  },
    { "/",     400, ASSOC_LEFT  },
    { "//",   400, ASSOC_LEFT  },
    { "mod",   400, ASSOC_LEFT  },
    { "div",   400, ASSOC_LEFT  },
    { "rem",   400, ASSOC_LEFT  },
    { "rdiv",  400, ASSOC_LEFT  },
    { ">>",    400, ASSOC_LEFT  },
    { "<<",    400, ASSOC_LEFT  },
    { "xor",   400, ASSOC_LEFT  },
    { "/\\",   500, ASSOC_LEFT  },
    { "\\/",   500, ASSOC_LEFT  },
    { "**",    200, ASSOC_RIGHT },
    { "^",     200, ASSOC_RIGHT },
    { ":",     200, ASSOC_RIGHT },
    { NULL,    0,   ASSOC_NONE  }
};
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static const OpEntry *find_binop(const char *name) {
    for (const OpEntry *op = BIN_OPS; op->name; op++)
        if (strcmp(op->name, name) == 0) return op;
    return NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *parse_term(Parser *p, int max_prec);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *parse_primary(Parser *p);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *parse_list(Parser *p) {
    Token tk = lexer_peek(&p->lx);
    if (tk.kind == TK_RBRACKET) {
        lexer_next(&p->lx);
        return term_new_atom(ATOM_NIL);
    }
    p->in_args++;
    Term *head = parse_term(p, 1200);
    p->in_args--;
    if (!head) return term_new_atom(ATOM_NIL);
    tk = lexer_peek(&p->lx);
    Term *tail = NULL;
    if (tk.kind == TK_COMMA) {
        lexer_next(&p->lx);
        tail = parse_list(p);
    } else if (tk.kind == TK_PIPE) {
        lexer_next(&p->lx);
        p->in_args++;
        tail = parse_term(p, 1200);
        p->in_args--;
        tk = lexer_peek(&p->lx);
        if (tk.kind == TK_RBRACKET) lexer_next(&p->lx);
        else perror_at(p, tk.line, "expected ] after list tail");
    } else if (tk.kind == TK_RBRACKET) {
        lexer_next(&p->lx);
        tail = term_new_atom(ATOM_NIL);
    } else {
        perror_at(p, tk.line, "expected , | ] in list");
        tail = term_new_atom(ATOM_NIL);
    }
    Term *args[2] = { head, tail };
    return term_new_compound(ATOM_DOT, 2, args);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int parse_args(Parser *p, Term ***args_out) {
    int cap = 8, n = 0;
    Term **args = malloc(cap * sizeof(Term *));
    Token pk0 = lexer_peek(&p->lx);
    if (pk0.kind == TK_RPAREN) {
        *args_out = args;
        return 0;
    }
    p->in_args++;
    for (;;) {
        if (n >= cap) { cap *= 2; args = realloc(args, cap * sizeof(Term *)); }
        Term *t = parse_term(p, 1200);
        if (!t) break;
        args[n++] = t;
        Token tk = lexer_peek(&p->lx);
        if (tk.kind != TK_COMMA) break;
        lexer_next(&p->lx);
    }
    p->in_args--;
    *args_out = args;
    return n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *parse_primary(Parser *p) {
    Token tk = lexer_next(&p->lx);
    switch (tk.kind) {
        case TK_VAR:
            return scope_get(&p->sc, tk.text);
        case TK_ANON:
            return term_new_var(-1);
        case TK_INT: {
            Term *t = term_new_int(tk.ival);
            return t;
        }
        case TK_FLOAT: {
            Term *t = term_new_float(tk.fval);
            return t;
        }
        case TK_STRING: {
            int id = prolog_atom_intern(tk.text);
            return term_new_atom(id);
        }
        case TK_ATOM: {
            Token pk = lexer_peek(&p->lx);
            if (pk.kind == TK_LPAREN) {
                lexer_next(&p->lx);
                Term **args = NULL;
                int nargs = parse_args(p, &args);
                Token rp = lexer_peek(&p->lx);
                if (rp.kind == TK_RPAREN) lexer_next(&p->lx);
                else perror_at(p, rp.line, "expected ) after args");
                int fid = prolog_atom_intern(tk.text);
                Term *t = term_new_compound(fid, nargs, args);
                free(args);
                return t;
            }
            if (strcmp(tk.text, "dynamic") == 0 ||
                    strcmp(tk.text, "discontiguous") == 0 ||
                    strcmp(tk.text, "multifile") == 0 ||
                    strcmp(tk.text, "module_transparent") == 0 ||
                    strcmp(tk.text, "meta_predicate") == 0 ||
                    strcmp(tk.text, "use_module") == 0 ||
                    strcmp(tk.text, "ensure_loaded") == 0 ||
                    strcmp(tk.text, "mode") == 0) {
                if (pk.kind == TK_ATOM || pk.kind == TK_VAR || pk.kind == TK_INT ||
                    pk.kind == TK_FLOAT || pk.kind == TK_LPAREN || pk.kind == TK_LBRACKET ||
                    pk.kind == TK_OP) {
                    int fid = prolog_atom_intern(tk.text);
                    Term *arg = parse_term(p, 1150);
                    Term *args1[1] = { arg };
                    return term_new_compound(fid, 1, args1);
                }
            }
            int id = prolog_atom_intern(tk.text);
            return term_new_atom(id);
        }
        case TK_CUT:
            return term_new_atom(ATOM_CUT);
        case TK_LPAREN: {
            int saved_in_args = p->in_args;
            p->in_args = 0;
            Term *t = parse_term(p, 1200);
            p->in_args = saved_in_args;
            Token rp = lexer_peek(&p->lx);
            if (rp.kind == TK_RPAREN) lexer_next(&p->lx);
            else perror_at(p, rp.line, "expected )");
            return t;
        }
        case TK_LBRACKET:
            return parse_list(p);
        case TK_COMMA:
        case TK_SEMI: {
            const char *opname = (tk.kind == TK_COMMA) ? "," : ";";
            Token pk = lexer_peek(&p->lx);
            if (pk.kind == TK_LPAREN) {
                lexer_next(&p->lx);
                Term **args = NULL;
                int nargs = parse_args(p, &args);
                Token rp = lexer_peek(&p->lx);
                if (rp.kind == TK_RPAREN) lexer_next(&p->lx);
                else perror_at(p, rp.line, "expected ) after args");
                int fid = prolog_atom_intern(opname);
                Term *t = term_new_compound(fid, nargs, args);
                free(args);
                return t;
            }
            perror_at(p, tk.line, "unexpected token in term");
            return NULL;
        }
        case TK_OP: {
            if (strcmp(tk.text, "\\+") == 0 || strcmp(tk.text, "not") == 0) {
                Term *arg = parse_term(p, 900);
                int fid = prolog_atom_intern(tk.text);
                Term *args[1] = { arg };
                return term_new_compound(fid, 1, args);
            }
            if (strcmp(tk.text, "\\") == 0) {
                Term *arg = parse_term(p, 200);
                int fid = prolog_atom_intern("\\");
                Term *args[1] = { arg };
                return term_new_compound(fid, 1, args);
            }
            if (strcmp(tk.text, "-") == 0) {
                Token pk = lexer_peek(&p->lx);
                if (pk.kind == TK_INT || pk.kind == TK_FLOAT) {
                    Token num = lexer_next(&p->lx);
                    if (num.kind == TK_INT)   return term_new_int(-num.ival);
                    if (num.kind == TK_FLOAT) return term_new_float(-num.fval);
                }
                if (pk.kind == TK_ATOM || pk.kind == TK_OP || pk.kind == TK_VAR || pk.kind == TK_LPAREN) {
                    Term *arg = parse_term(p, 200);
                    int fid = prolog_atom_intern("-");
                    Term *args[1] = { arg };
                    return term_new_compound(fid, 1, args);
                }
            }
            if (strcmp(tk.text, "+") == 0) {
                Token pk = lexer_peek(&p->lx);
                if (pk.kind == TK_ATOM || pk.kind == TK_OP || pk.kind == TK_VAR ||
                    pk.kind == TK_LPAREN || pk.kind == TK_INT || pk.kind == TK_FLOAT) {
                    Term *arg = parse_term(p, 200);
                    int fid = prolog_atom_intern("+");
                    Term *args[1] = { arg };
                    return term_new_compound(fid, 1, args);
                }
            }
            {
                int id = prolog_atom_intern(tk.text);
                Token pk2 = lexer_peek(&p->lx);
                if (pk2.kind == TK_LPAREN) {
                    lexer_next(&p->lx);
                    Term **args = NULL;
                    int nargs = parse_args(p, &args);
                    Token rp = lexer_peek(&p->lx);
                    if (rp.kind == TK_RPAREN) lexer_next(&p->lx);
                    else perror_at(p, rp.line, "expected ) after args");
                    Term *t = term_new_compound(id, nargs, args);
                    free(args);
                    return t;
                }
                return term_new_atom(id);
            }
        }
        case TK_NECK:
        case TK_QUERY: {
            perror_at(p, tk.line, "unexpected :- in term position");
            return NULL;
        }
        case TK_LBRACE: {
            Token pk2 = lexer_peek(&p->lx);
            if (pk2.kind == TK_RBRACE) {
                lexer_next(&p->lx);
                return term_new_atom(prolog_atom_intern("{}"));
            }
            Term *inner = parse_term(p, 1200);
            Token rb = lexer_next(&p->lx);
            if (rb.kind != TK_RBRACE)
                perror_at(p, rb.line, "expected } after term");
            int curl_id = prolog_atom_intern("{}");
            Term *args[1] = { inner };
            return term_new_compound(curl_id, 1, args);
        }
        default:
            perror_at(p, tk.line, "unexpected token in term");
            return NULL;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *parse_term(Parser *p, int max_prec) {
    Term *lhs = parse_primary(p);
    if (!lhs) return NULL;
    for (;;) {
        Token pk = lexer_peek(&p->lx);
        const char *optext = NULL;
        if (pk.kind == TK_OP)   optext = pk.text;
        else if (pk.kind == TK_ATOM) optext = pk.text;
        else if (pk.kind == TK_COMMA && p->in_args > 0) break;
        else if (pk.kind == TK_COMMA && max_prec >= 1000) optext = ",";
        else if (pk.kind == TK_SEMI  && max_prec >= 1100) optext = ";";
        else if (pk.kind == TK_NECK  && max_prec >= 1200) optext = ":-";
        else break;
        const OpEntry *op = optext ? find_binop(optext) : NULL;
        if (!op || op->prec > max_prec) break;
        lexer_next(&p->lx);
        int rprec = (op->assoc == ASSOC_LEFT) ? op->prec - 1 : op->prec;
        Term *rhs = parse_term(p, rprec);
        if (!rhs) break;
        int fid = prolog_atom_intern(op->name);
        Term *args[2] = { lhs, rhs };
        lhs = term_new_compound(fid, 2, args);
    }
    return lhs;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int count_conj(Term *t) {
    t = term_deref(t);
    if (!t) return 0;
    int comma_id = prolog_atom_intern(",");
    if (t->tag == TERM_COMPOUND && t->compound.functor == comma_id && t->compound.arity == 2)
        return count_conj(t->compound.args[0]) + count_conj(t->compound.args[1]);
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int flatten_conj(Term *t, Term **buf, int idx) {
    t = term_deref(t);
    if (!t) return idx;
    int comma_id = prolog_atom_intern(",");
    if (t->tag == TERM_COMPOUND && t->compound.functor == comma_id && t->compound.arity == 2) {
        idx = flatten_conj(t->compound.args[0], buf, idx);
        idx = flatten_conj(t->compound.args[1], buf, idx);
        return idx;
    }
    buf[idx++] = t;
    return idx;
}
static int dcg_var_counter = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-PL-6b — Parallel tree_t-building path.
 * Runs alongside the Term* path. Both active simultaneously.
 * No variable-slot assignment here — that moves to prolog_lower.c in 6e.
 * Variable identity is tracked by name only (TreeScope).
 */
#define TS_MAX_VARS 256
typedef struct { char *name; int idx; } TSEntry;
typedef struct { TSEntry e[TS_MAX_VARS]; int n; } TreeScope;

static void ts_reset(TreeScope *ts) { ts->n = 0; }

/* Return a TT_VAR node for this name. Each unique name in the clause
 * gets a unique v.sval. No slot number — lower will assign in 6e. */
static tree_t *ts_get(TreeScope *ts, const char *name) {
    for (int i = 0; i < ts->n; i++)
        if (strcmp(ts->e[i].name, name) == 0) {
            /* Return a fresh TT_VAR node aliased to the same name;
             * structural equivalence checked by name in 6c. */
            tree_t *v = ast_node_new(TT_VAR);
            v->v.sval = ts->e[i].name;   /* shared interned pointer */
            return v;
        }
    if (ts->n >= TS_MAX_VARS) {
        tree_t *v = ast_node_new(TT_VAR);
        v->v.sval = strdup("_OVERFLOW");
        return v;
    }
    char *interned = strdup(name);
    ts->e[ts->n].name = interned;
    ts->e[ts->n].idx  = ts->n;
    ts->n++;
    tree_t *v = ast_node_new(TT_VAR);
    v->v.sval = interned;
    return v;
}

/* Forward declarations for the parallel path */
static tree_t *pt_term(Parser *p, TreeScope *ts, int max_prec);
static tree_t *pt_primary(Parser *p, TreeScope *ts);

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *pt_list(Parser *p, TreeScope *ts) {
    /* Build TT_MAKELIST with flat children [e1..en, optional_tail].
     * v.ival=1 means explicit | tail present (last child is the tail).
     * v.ival=0 means proper list (no tail child).
     * Lowerer will reconstruct the '.'(H,T) cons chain. */
    Token tk = lexer_peek(&p->lx);
    if (tk.kind == TK_RBRACKET) {
        lexer_next(&p->lx);
        tree_t *n = ast_node_new(TT_MAKELIST);   /* empty list, v.ival=0 */
        return n;
    }
    tree_t *lst = ast_node_new(TT_MAKELIST);
    lst->v.ival = 0;   /* proper list by default */
    p->in_args++;
    for (;;) {
        tree_t *elem = pt_term(p, ts, 1200);
        if (elem) ast_push(lst, elem);
        Token pk = lexer_peek(&p->lx);
        if (pk.kind == TK_COMMA) { lexer_next(&p->lx); continue; }
        if (pk.kind == TK_PIPE) {
            lexer_next(&p->lx);
            tree_t *tail = pt_term(p, ts, 1200);
            if (tail) ast_push(lst, tail);
            lst->v.ival = 1;   /* explicit tail */
            pk = lexer_peek(&p->lx);
            if (pk.kind == TK_RBRACKET) lexer_next(&p->lx);
        } else if (pk.kind == TK_RBRACKET) {
            lexer_next(&p->lx);
        }
        break;
    }
    p->in_args--;
    return lst;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int pt_args(Parser *p, TreeScope *ts, tree_t *parent) {
    /* Parse comma-separated args, push each as child of parent.
     * Returns number of args added. */
    Token pk0 = lexer_peek(&p->lx);
    if (pk0.kind == TK_RPAREN) return 0;
    int n = 0;
    p->in_args++;
    for (;;) {
        tree_t *a = pt_term(p, ts, 1200);
        if (!a) break;
        ast_push(parent, a);
        n++;
        Token tk = lexer_peek(&p->lx);
        if (tk.kind != TK_COMMA) break;
        lexer_next(&p->lx);
    }
    p->in_args--;
    return n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Pure reduce action: wrap lhs and rhs as children of TT_FNC(op).
 * No structural reasoning — just node(kind, children-in-source-order). */
static tree_t *pt_binop(const char *op, tree_t *lhs, tree_t *rhs) {
    tree_t *n = ast_node_new(TT_FNC);
    n->v.sval = strdup(op);
    ast_push(n, lhs);
    ast_push(n, rhs);
    return n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Pure reduce action: wrap head and body into TT_CLAUSE(head, body).
 * Children in source order. NO conjunction flattening, NO if-then-else
 * detection — those are lower's job. The tree is a geometric record of
 * the token stream; lower interprets it. */
/* Flatten a tree_t conjunction (TT_FNC(",")) into flat TT_PROGRAM children.
 * This is n-ary flattening — a reduce action on left-recursive grammar.
 * Mirrors flatten_conj() for the Term* path. Stays in parser per design. */
static void pt_flatten_conj(tree_t *t, tree_t *prog) {
    if (!t) return;
    if (t->t == TT_FNC && t->v.sval && strcmp(t->v.sval, ",") == 0) {
        for (int i = 0; i < t->n; i++)
            pt_flatten_conj(t->c[i], prog);
        return;
    }
    ast_push(prog, t);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Detect ;(->(Cond,Then),Else) and collapse to TT_IF(cond,then,else).
 * This is a grammar-level reduce: the ';'/'->' combination is a
 * syntactic idiom that maps to a dedicated node kind. */
static tree_t *pt_maybe_ifthenelse(tree_t *semi_node) {
    if (semi_node->n != 2) return semi_node;
    tree_t *left  = semi_node->c[0];
    tree_t *right = semi_node->c[1];
    if (!left || left->t != TT_FNC || !left->v.sval) return semi_node;
    if (strcmp(left->v.sval, "->") != 0 || left->n < 2) return semi_node;
    /* Wrap then/else in TT_PROGRAM if they're conjunctions, mirroring
     * flatten_conj on the Term* side. Cond is always a single goal. */
    tree_t *then_prog = ast_node_new(TT_PROGRAM);
    pt_flatten_conj(left->c[1], then_prog);
    tree_t *else_prog = ast_node_new(TT_PROGRAM);
    pt_flatten_conj(right, else_prog);
    tree_t *iff = ast_node_new(TT_IF);
    ast_push(iff, left->c[0]);   /* cond — single goal */
    ast_push(iff, then_prog->n == 1 ? then_prog->c[0] : then_prog);  /* then */
    ast_push(iff, else_prog->n == 1 ? else_prog->c[0] : else_prog);  /* else */
    return iff;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *pt_make_clause(tree_t *head_tr, tree_t *body_tr) {
    tree_t *cl = ast_node_new(TT_CLAUSE);
    if (head_tr) {
        ast_push(cl, head_tr);
    } else {
        ast_push(cl, ast_node_new(TT_NUL));   /* directive: head is TT_NUL */
    }
    /* body: flatten conjunction into TT_PROGRAM (n-ary reduce) */
    tree_t *prog = ast_node_new(TT_PROGRAM);
    if (body_tr) pt_flatten_conj(body_tr, prog);
    ast_push(cl, prog);
    return cl;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *pt_primary(Parser *p, TreeScope *ts) {
    Token tk = lexer_next(&p->lx);
    switch (tk.kind) {
        case TK_VAR:
            return ts_get(ts, tk.text);
        case TK_ANON: {
            tree_t *v = ast_node_new(TT_VAR);
            v->v.sval = strdup("_");
            return v;
        }
        case TK_INT: {
            tree_t *n = ast_node_new(TT_ILIT);
            n->v.ival = tk.ival;
            return n;
        }
        case TK_FLOAT: {
            tree_t *n = ast_node_new(TT_FLIT);
            n->v.dval = tk.fval;
            return n;
        }
        case TK_STRING: {
            tree_t *n = ast_node_new(TT_QLIT);
            n->v.sval = strdup(tk.text);
            return n;
        }
        case TK_ATOM: {
            Token pk = lexer_peek(&p->lx);
            if (pk.kind == TK_LPAREN) {
                lexer_next(&p->lx);
                tree_t *fnc = ast_node_new(TT_FNC);
                fnc->v.sval = strdup(tk.text);
                pt_args(p, ts, fnc);
                Token rp = lexer_peek(&p->lx);
                if (rp.kind == TK_RPAREN) lexer_next(&p->lx);
                return fnc;
            }
            /* Special directive atoms that consume one argument */
            if (strcmp(tk.text, "dynamic") == 0 ||
                strcmp(tk.text, "discontiguous") == 0 ||
                strcmp(tk.text, "multifile") == 0 ||
                strcmp(tk.text, "module_transparent") == 0 ||
                strcmp(tk.text, "meta_predicate") == 0 ||
                strcmp(tk.text, "use_module") == 0 ||
                strcmp(tk.text, "ensure_loaded") == 0 ||
                strcmp(tk.text, "mode") == 0) {
                if (pk.kind == TK_ATOM || pk.kind == TK_VAR || pk.kind == TK_INT ||
                    pk.kind == TK_FLOAT || pk.kind == TK_LPAREN || pk.kind == TK_LBRACKET ||
                    pk.kind == TK_OP) {
                    tree_t *fnc = ast_node_new(TT_FNC);
                    fnc->v.sval = strdup(tk.text);
                    tree_t *arg = pt_term(p, ts, 1150);
                    if (arg) ast_push(fnc, arg);
                    return fnc;
                }
            }
            /* [] is the empty list atom — mirror Term* ATOM_NIL → (list) */
            if (strcmp(tk.text, "[]") == 0)
                return ast_node_new(TT_MAKELIST);
            tree_t *n = ast_node_new(TT_QLIT);
            n->v.sval = strdup(tk.text);
            return n;
        }
        case TK_CUT: {
            return ast_node_new(TT_CUT);
        }
        case TK_LPAREN: {
            int saved = p->in_args;
            p->in_args = 0;
            tree_t *inner = pt_term(p, ts, 1200);
            p->in_args = saved;
            Token rp = lexer_peek(&p->lx);
            if (rp.kind == TK_RPAREN) lexer_next(&p->lx);
            return inner;
        }
        case TK_LBRACKET:
            return pt_list(p, ts);
        case TK_COMMA:
        case TK_SEMI: {
            const char *opname = (tk.kind == TK_COMMA) ? "," : ";";
            Token pk2 = lexer_peek(&p->lx);
            if (pk2.kind == TK_LPAREN) {
                lexer_next(&p->lx);
                tree_t *fnc = ast_node_new(TT_FNC);
                fnc->v.sval = strdup(opname);
                pt_args(p, ts, fnc);
                Token rp = lexer_peek(&p->lx);
                if (rp.kind == TK_RPAREN) lexer_next(&p->lx);
                return fnc;
            }
            return NULL;
        }
        case TK_OP: {
            if (strcmp(tk.text, "\\+") == 0 || strcmp(tk.text, "not") == 0) {
                tree_t *arg = pt_term(p, ts, 900);
                tree_t *fnc = ast_node_new(TT_FNC);
                fnc->v.sval = strdup(tk.text);
                if (arg) ast_push(fnc, arg);
                return fnc;
            }
            if (strcmp(tk.text, "\\") == 0) {
                tree_t *arg = pt_term(p, ts, 200);
                tree_t *fnc = ast_node_new(TT_FNC);
                fnc->v.sval = strdup("\\");
                if (arg) ast_push(fnc, arg);
                return fnc;
            }
            if (strcmp(tk.text, "-") == 0) {
                Token pk3 = lexer_peek(&p->lx);
                if (pk3.kind == TK_INT) {
                    Token num = lexer_next(&p->lx);
                    tree_t *n = ast_node_new(TT_ILIT);
                    n->v.ival = -num.ival;
                    return n;
                }
                if (pk3.kind == TK_FLOAT) {
                    Token num = lexer_next(&p->lx);
                    tree_t *n = ast_node_new(TT_FLIT);
                    n->v.dval = -num.fval;
                    return n;
                }
                if (pk3.kind == TK_ATOM || pk3.kind == TK_OP || pk3.kind == TK_VAR || pk3.kind == TK_LPAREN) {
                    tree_t *arg = pt_term(p, ts, 200);
                    tree_t *fnc = ast_node_new(TT_FNC);
                    fnc->v.sval = strdup("-");
                    if (arg) ast_push(fnc, arg);
                    return fnc;
                }
            }
            if (strcmp(tk.text, "+") == 0) {
                Token pk3 = lexer_peek(&p->lx);
                if (pk3.kind == TK_ATOM || pk3.kind == TK_OP || pk3.kind == TK_VAR ||
                    pk3.kind == TK_LPAREN || pk3.kind == TK_INT || pk3.kind == TK_FLOAT) {
                    tree_t *arg = pt_term(p, ts, 200);
                    tree_t *fnc = ast_node_new(TT_FNC);
                    fnc->v.sval = strdup("+");
                    if (arg) ast_push(fnc, arg);
                    return fnc;
                }
            }
            {
                Token pk3 = lexer_peek(&p->lx);
                if (pk3.kind == TK_LPAREN) {
                    lexer_next(&p->lx);
                    tree_t *fnc = ast_node_new(TT_FNC);
                    fnc->v.sval = strdup(tk.text);
                    pt_args(p, ts, fnc);
                    Token rp = lexer_peek(&p->lx);
                    if (rp.kind == TK_RPAREN) lexer_next(&p->lx);
                    return fnc;
                }
                tree_t *n = ast_node_new(TT_QLIT);
                n->v.sval = strdup(tk.text);
                return n;
            }
        }
        case TK_LBRACE: {
            Token pk2 = lexer_peek(&p->lx);
            if (pk2.kind == TK_RBRACE) {
                lexer_next(&p->lx);
                tree_t *n = ast_node_new(TT_FNC);
                n->v.sval = strdup("{}");
                return n;
            }
            tree_t *inner = pt_term(p, ts, 1200);
            Token rb = lexer_next(&p->lx);
            (void)rb;
            tree_t *fnc = ast_node_new(TT_FNC);
            fnc->v.sval = strdup("{}");
            if (inner) ast_push(fnc, inner);
            return fnc;
        }
        default:
            return NULL;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static tree_t *pt_term(Parser *p, TreeScope *ts, int max_prec) {
    /* Save and restore lexer position: pt_primary consumes tokens from p->lx
     * but the Term* path already consumed them. We CANNOT double-consume.
     * Strategy: pt_term is called on a SECOND parse pass of a saved token
     * stream, OR we drive pt_term from the same single pass as the Term* path
     * by running each sub-parse in lockstep.
     *
     * For PST-PL-6b the approach is: run the parallel path on a CLONED
     * lexer snapshot taken before the Term* parse. See pt_parse_clause().
     */
    tree_t *lhs = pt_primary(p, ts);
    if (!lhs) return NULL;
    for (;;) {
        Token pk = lexer_peek(&p->lx);
        const char *optext = NULL;
        if      (pk.kind == TK_OP)                              optext = pk.text;
        else if (pk.kind == TK_ATOM)                            optext = pk.text;
        else if (pk.kind == TK_COMMA && p->in_args > 0)         break;
        else if (pk.kind == TK_COMMA && max_prec >= 1000)       optext = ",";
        else if (pk.kind == TK_SEMI  && max_prec >= 1100)       optext = ";";
        else if (pk.kind == TK_NECK  && max_prec >= 1200)       optext = ":-";
        else break;
        const OpEntry *op = optext ? find_binop(optext) : NULL;
        if (!op || op->prec > max_prec) break;
        lexer_next(&p->lx);
        int rprec = (op->assoc == ASSOC_LEFT) ? op->prec - 1 : op->prec;
        tree_t *rhs = pt_term(p, ts, rprec);
        if (!rhs) break;
        tree_t *node = pt_binop(op->name, lhs, rhs);
        /* Collapse ;(->(C,T),E) → TT_IF(c,t,e) at parse time (n-ary reduce). */
        if (strcmp(op->name, ";") == 0)
            node = pt_maybe_ifthenelse(node);
        lhs = node;
    }
    return lhs;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *dcg_fresh_var(VarScope *sc) {
    char name[32];
    snprintf(name, sizeof(name), "_S%d", dcg_var_counter++);
    return scope_get(sc, name);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *dcg_append_tail(Term *list, Term *tail) {
    list = term_deref(list);
    if (!list) return tail;
    if (list->tag == TERM_ATOM && list->atom_id == ATOM_NIL)
        return tail;
    if (list->tag == TERM_COMPOUND && list->compound.arity == 2) {
        Term *new_tail = dcg_append_tail(list->compound.args[1], tail);
        Term **args = malloc(2 * sizeof(Term *));
        args[0] = list->compound.args[0];
        args[1] = new_tail;
        return term_new_compound(list->compound.functor, 2, args);
    }
    return tail;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *dcg_make_unify(Term *a, Term *b) {
    int eq_id = prolog_atom_intern("=");
    Term **args = malloc(2 * sizeof(Term *));
    args[0] = a; args[1] = b;
    return term_new_compound(eq_id, 2, args);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static Term *dcg_call_nt(Term *nt, Term *s_in, Term *s_out) {
    nt = term_deref(nt);
    if (nt->tag == TERM_ATOM) {
        Term **args = malloc(2 * sizeof(Term *));
        args[0] = s_in; args[1] = s_out;
        return term_new_compound(nt->atom_id, 2, args);
    } else if (nt->tag == TERM_COMPOUND) {
        int new_arity = nt->compound.arity + 2;
        Term **args = malloc(new_arity * sizeof(Term *));
        for (int i = 0; i < nt->compound.arity; i++)
            args[i] = nt->compound.args[i];
        args[new_arity-2] = s_in;
        args[new_arity-1] = s_out;
        return term_new_compound(nt->compound.functor, new_arity, args);
    }
    return term_new_atom(prolog_atom_intern("true"));
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int dcg_expand_body(Term *body, Term *s_in, Term *s_out,
                           VarScope *sc, Term **buf, int idx);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int dcg_expand_body(Term *body, Term *s_in, Term *s_out,
                           VarScope *sc, Term **buf, int idx) {
    body = term_deref(body);
    if (!body) {
        buf[idx++] = dcg_make_unify(s_in, s_out);
        return idx;
    }
    int comma_id = prolog_atom_intern(",");
    int semi_id  = prolog_atom_intern(";");
    int curl_id  = prolog_atom_intern("{}");
    if (body->tag == TERM_COMPOUND && body->compound.functor == curl_id
            && body->compound.arity == 1) {
        int n = count_conj(body->compound.args[0]);
        int old = idx;
        Term **tmp = malloc((n+1) * sizeof(Term *));
        int nn = flatten_conj(body->compound.args[0], tmp, 0);
        for (int i = 0; i < nn; i++) buf[idx++] = tmp[i];
        free(tmp);
        (void)old;
        buf[idx++] = dcg_make_unify(s_in, s_out);
        return idx;
    }
    if (body->tag == TERM_ATOM && body->atom_id == ATOM_NIL) {
        buf[idx++] = dcg_make_unify(s_in, s_out);
        return idx;
    }
    if (body->tag == TERM_COMPOUND && body->compound.functor == ATOM_DOT) {
        Term *list_with_tail = dcg_append_tail(body, s_out);
        buf[idx++] = dcg_make_unify(s_in, list_with_tail);
        return idx;
    }
    if (body->tag == TERM_COMPOUND && body->compound.functor == comma_id
            && body->compound.arity == 2) {
        Term *s_mid = dcg_fresh_var(sc);
        idx = dcg_expand_body(body->compound.args[0], s_in,  s_mid, sc, buf, idx);
        idx = dcg_expand_body(body->compound.args[1], s_mid, s_out, sc, buf, idx);
        return idx;
    }
    if (body->tag == TERM_COMPOUND && body->compound.functor == semi_id
            && body->compound.arity == 2) {
        Term *buf_a[256]; int na = 0;
        Term *buf_b[256]; int nb = 0;
        na = dcg_expand_body(body->compound.args[0], s_in, s_out, sc, buf_a, 0);
        nb = dcg_expand_body(body->compound.args[1], s_in, s_out, sc, buf_b, 0);
        Term *conj_a = buf_a[0];
        for (int i = 1; i < na; i++) {
            Term **ca = malloc(2 * sizeof(Term *));
            ca[0] = conj_a; ca[1] = buf_a[i];
            conj_a = term_new_compound(comma_id, 2, ca);
        }
        Term *conj_b = buf_b[0];
        for (int i = 1; i < nb; i++) {
            Term **cb = malloc(2 * sizeof(Term *));
            cb[0] = conj_b; cb[1] = buf_b[i];
            conj_b = term_new_compound(comma_id, 2, cb);
        }
        Term **sargs = malloc(2 * sizeof(Term *));
        sargs[0] = conj_a; sargs[1] = conj_b;
        buf[idx++] = term_new_compound(semi_id, 2, sargs);
        return idx;
    }
    if (body->tag == TERM_ATOM && body->atom_id == prolog_atom_intern("!")) {
        buf[idx++] = body;
        buf[idx++] = dcg_make_unify(s_in, s_out);
        return idx;
    }
    if (body->tag == TERM_ATOM && body->atom_id == prolog_atom_intern("true")) {
        buf[idx++] = dcg_make_unify(s_in, s_out);
        return idx;
    }
    buf[idx++] = dcg_call_nt(body, s_in, s_out);
    return idx;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void dcg_expand_clause(PlClause *cl, Term *dcg_body, Term *pushback, VarScope *sc) {
    dcg_var_counter = 0;
    Term *s0 = dcg_fresh_var(sc);
    Term *s  = dcg_fresh_var(sc);
    Term *head = term_deref(cl->head);
    if (head->tag == TERM_ATOM) {
        Term **args = malloc(2 * sizeof(Term *));
        args[0] = s0; args[1] = s;
        cl->head = term_new_compound(head->atom_id, 2, args);
    } else if (head->tag == TERM_COMPOUND) {
        int new_arity = head->compound.arity + 2;
        Term **args = malloc(new_arity * sizeof(Term *));
        for (int i = 0; i < head->compound.arity; i++)
            args[i] = head->compound.args[i];
        args[new_arity-2] = s0;
        args[new_arity-1] = s;
        cl->head = term_new_compound(head->compound.functor, new_arity, args);
    }
    Term *buf[1024];
    int n;
    if (pushback) {
        Term *s_mid = dcg_fresh_var(sc);
        n = dcg_expand_body(dcg_body, s0, s_mid, sc, buf, 0);
        Term *pushback_with_tail = dcg_append_tail(pushback, s);
        buf[n++] = dcg_make_unify(s_mid, pushback_with_tail);
    } else {
        n = dcg_expand_body(dcg_body, s0, s, sc, buf, 0);
    }
    cl->body  = malloc(n * sizeof(Term *));
    cl->nbody = n;
    for (int i = 0; i < n; i++) cl->body[i] = buf[i];
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int eval_if_condition(Term *cond) {
    cond = term_deref(cond);
    if (!cond) return -1;
    if (cond->tag == TERM_ATOM) {
        const char *a = prolog_atom_name(cond->atom_id);
        if (strcmp(a, "true") == 0) return 1;
        if (strcmp(a, "fail") == 0 || strcmp(a, "false") == 0) return 0;
        return -1;
    }
    if (cond->tag != TERM_COMPOUND) return -1;
    const char *fn = prolog_atom_name(cond->compound.functor);
    int arity = cond->compound.arity;
    if ((strcmp(fn, "\\+") == 0 || strcmp(fn, "not") == 0) && arity == 1) {
        int v = eval_if_condition(cond->compound.args[0]);
        if (v < 0) return -1;
        return v ? 0 : 1;
    }
    if (strcmp(fn, "current_prolog_flag") == 0 && arity == 2) {
        Term *flag_t = term_deref(cond->compound.args[0]);
        Term *val_t  = term_deref(cond->compound.args[1]);
        if (!flag_t || !val_t || flag_t->tag != TERM_ATOM || val_t->tag != TERM_ATOM)
            return -1;
        const char *flag = prolog_atom_name(flag_t->atom_id);
        const char *val  = prolog_atom_name(val_t->atom_id);
        if (strcmp(flag, "bounded") == 0) {
            if (strcmp(val, "true")  == 0) return 1;
            if (strcmp(val, "false") == 0) return 0;
            return -1;
        }
        if (strcmp(flag, "prefer_rationals") == 0) {
            if (strcmp(val, "true")  == 0) return 0;
            if (strcmp(val, "false") == 0) return 1;
            return -1;
        }
        return -1;
    }
    return -1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int try_handle_if_directive(Parser *p, Term *goal, int lineno) {
    goal = term_deref(goal);
    if (!goal) return 0;
    const char *fn = NULL;
    int arity = 0;
    Term *arg0 = NULL;
    if (goal->tag == TERM_ATOM) {
        fn = prolog_atom_name(goal->atom_id);
        arity = 0;
    } else if (goal->tag == TERM_COMPOUND) {
        fn = prolog_atom_name(goal->compound.functor);
        arity = goal->compound.arity;
        if (arity > 0) arg0 = goal->compound.args[0];
    } else {
        return 0;
    }
    if (!fn) return 0;
    if (strcmp(fn, "if") == 0 && arity == 1) {
        if (p->ifst_top >= IF_STACK_MAX) {
            perror_at(p, lineno, ":- if/1 nesting too deep");
            return 1;
        }
        int parent_active = if_currently_active(p);
        int verdict = parent_active ? eval_if_condition(arg0) : 0;
        int active = parent_active && (verdict != 0);
        IfFrame *f = &p->ifst[p->ifst_top++];
        f->active = active;
        f->taken  = active;
        f->parent_active = parent_active;
        f->line   = lineno;
        return 1;
    }
    if (strcmp(fn, "elif") == 0 && arity == 1) {
        if (p->ifst_top == 0) {
            perror_at(p, lineno, ":- elif without matching :- if");
            return 1;
        }
        IfFrame *f = &p->ifst[p->ifst_top - 1];
        if (!f->parent_active || f->taken) {
            f->active = 0;
        } else {
            int verdict = eval_if_condition(arg0);
            int active  = (verdict != 0);
            f->active = active;
            if (active) f->taken = 1;
        }
        return 1;
    }
    if (strcmp(fn, "else") == 0 && arity == 0) {
        if (p->ifst_top == 0) {
            perror_at(p, lineno, ":- else without matching :- if");
            return 1;
        }
        IfFrame *f = &p->ifst[p->ifst_top - 1];
        if (!f->parent_active || f->taken) {
            f->active = 0;
        } else {
            f->active = 1;
            f->taken  = 1;
        }
        return 1;
    }
    if (strcmp(fn, "endif") == 0 && arity == 0) {
        if (p->ifst_top == 0) {
            perror_at(p, lineno, ":- endif without matching :- if");
            return 1;
        }
        p->ifst_top--;
        return 1;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* PST-PL-6c — Structural equivalence verifier: Term* ↔ tree_t
 * Both trees are serialized to a canonical S-expression string and compared.
 * Called from parse_clause for every non-directive clause.
 *
 * Normalization rules (same for both sides):
 *   atom          → (atom "name")
 *   integer       → (int N)
 *   float         → (flt F)
 *   variable      → (var "name")  — name comes from VarScope for Term*
 *   cut           → (cut)
 *   list [H|T]    → (list H ... T?)  — flattened, tail only if non-nil
 *   compound f/n  → (fnc "f" c1 c2 ... cn)
 *   if-then-else  → (if C T E)       — from both ';'/'->' and TT_IF
 *   conjunction   → (seq g1 g2 ... gn)  — flattened from ',' chains / TT_PROGRAM
 */

#define PL_SER_MAX (1<<17)   /* 128 KB should be more than enough per clause */

typedef struct { char *buf; int len; int cap; int err; } PLS;

static void pls_init(PLS *s) {
    s->cap = 256; s->buf = malloc(s->cap); s->len = 0; s->err = 0; s->buf[0] = 0;
}
static void pls_free(PLS *s) { free(s->buf); }
static void pls_char(PLS *s, char c) {
    if (s->err) return;
    if (s->len + 2 >= PL_SER_MAX) { s->err = 1; return; }
    if (s->len + 2 > s->cap) {
        s->cap *= 2; s->buf = realloc(s->buf, s->cap);
    }
    s->buf[s->len++] = c; s->buf[s->len] = 0;
}
static void pls_str(PLS *s, const char *t) { for (; t && *t; t++) pls_char(s, *t); }
static void pls_int(PLS *s, long v) { char tmp[32]; snprintf(tmp,32,"%ld",v); pls_str(s,tmp); }
static void pls_flt(PLS *s, double v) { char tmp[32]; snprintf(tmp,32,"%.17g",v); pls_str(s,tmp); }

/* Forward declarations */
static void ser_term(PLS *s, Term *t, VarScope *sc);
static void ser_tree(PLS *s, tree_t *t);

/* Serialize Term* conjunction as flat (seq g1 g2 ...) */
static void ser_term_conj_flat(PLS *s, Term *t, VarScope *sc) {
    t = term_deref(t);
    if (!t) return;
    int comma_id = prolog_atom_intern(",");
    if (t->tag == TERM_COMPOUND && t->compound.functor == comma_id && t->compound.arity == 2) {
        ser_term_conj_flat(s, t->compound.args[0], sc);
        pls_char(s, ' ');
        ser_term_conj_flat(s, t->compound.args[1], sc);
    } else {
        ser_term(s, t, sc);
    }
}

/* Serialize Term* list as flat (list e1 e2 ... [tail]) */
static void ser_term_list(PLS *s, Term *t, VarScope *sc) {
    /* t is the cons chain after '[' — i.e. '.'(H,T) or nil */
    pls_str(s, "(list");
    for (;;) {
        t = term_deref(t);
        if (!t) break;
        if (t->tag == TERM_ATOM && t->atom_id == ATOM_NIL) break;
        if (t->tag == TERM_COMPOUND && t->compound.functor == ATOM_DOT && t->compound.arity == 2) {
            pls_char(s, ' ');
            ser_term(s, t->compound.args[0], sc);
            t = t->compound.args[1];
        } else {
            /* tail variable or non-nil atom */
            pls_str(s, " |");
            ser_term(s, t, sc);
            break;
        }
    }
    pls_char(s, ')');
}

/* Reverse-lookup variable name from VarScope by Term* pointer */
static const char *sc_name_for(VarScope *sc, Term *t) {
    t = term_deref(t);
    for (int i = 0; i < sc->count; i++)
        if (sc->entries[i].term == t) return sc->entries[i].name;
    return "_";
}

static void ser_term(PLS *s, Term *t, VarScope *sc) {
    t = term_deref(t);
    if (!t) { pls_str(s, "(null)"); return; }
    switch (t->tag) {
        case TERM_VAR: {
            const char *nm = sc_name_for(sc, t);
            pls_str(s, "(var \""); pls_str(s, nm); pls_str(s, "\")");
            return;
        }
        case TERM_INT:
            pls_str(s, "(int "); pls_int(s, t->ival); pls_char(s, ')');
            return;
        case TERM_FLOAT:
            pls_str(s, "(flt "); pls_flt(s, t->fval); pls_char(s, ')');
            return;
        case TERM_ATOM: {
            const char *n = prolog_atom_name(t->atom_id);
            if (!n) n = "?";
            if (t->atom_id == ATOM_NIL)  { pls_str(s, "(list)"); return; }
            if (t->atom_id == ATOM_CUT)  { pls_str(s, "(cut)");  return; }
            pls_str(s, "(atom \""); pls_str(s, n); pls_str(s, "\")");
            return;
        }
        case TERM_COMPOUND: {
            const char *fn = prolog_atom_name(t->compound.functor);
            if (!fn) fn = "?";
            int arity = t->compound.arity;
            /* list */
            if (t->compound.functor == ATOM_DOT && arity == 2) {
                ser_term_list(s, t, sc);
                return;
            }
            /* cut atom (shouldn't reach here as compound, but guard) */
            if (t->compound.functor == ATOM_CUT && arity == 0) {
                pls_str(s, "(cut)"); return;
            }
            int comma_id  = prolog_atom_intern(",");
            int semi_id   = prolog_atom_intern(";");
            int arrow_id  = prolog_atom_intern("->");
            /* if-then-else: ;(->(C,T),E) */
            if (t->compound.functor == semi_id && arity == 2) {
                Term *left = term_deref(t->compound.args[0]);
                if (left && left->tag == TERM_COMPOUND &&
                    left->compound.functor == arrow_id && left->compound.arity == 2) {
                    pls_str(s, "(if ");
                    ser_term(s, left->compound.args[0], sc);
                    pls_char(s, ' ');
                    ser_term(s, left->compound.args[1], sc);
                    pls_char(s, ' ');
                    ser_term(s, t->compound.args[1], sc);
                    pls_char(s, ')');
                    return;
                }
            }
            /* conjunction as (seq ...) */
            if (t->compound.functor == comma_id && arity == 2) {
                pls_str(s, "(seq ");
                ser_term_conj_flat(s, t, sc);
                pls_char(s, ')');
                return;
            }
            /* general compound */
            pls_str(s, "(fnc \""); pls_str(s, fn); pls_str(s, "\"");
            for (int i = 0; i < arity; i++) {
                pls_char(s, ' ');
                ser_term(s, t->compound.args[i], sc);
            }
            pls_char(s, ')');
            return;
        }
        case TERM_REF:
            ser_term(s, t->ref, sc);
            return;
    }
}

/* Serialize tree_t */
static void ser_tree_list(PLS *s, tree_t *t);

/* Flatten TT_FNC(",") chains into individual items for (seq ...) emission. */
static void ser_tree_conj_flat(PLS *s, tree_t *t) {
    if (!t) return;
    if (t->t == TT_FNC && t->v.sval && strcmp(t->v.sval, ",") == 0) {
        for (int i = 0; i < t->n; i++) ser_tree_conj_flat(s, t->c[i]);
        return;
    }
    pls_char(s, ' '); ser_tree(s, t);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void ser_tree(PLS *s, tree_t *t) {
    if (!t) { pls_str(s, "(null)"); return; }
    switch (t->t) {
        case TT_VAR:
            pls_str(s, "(var \"");
            pls_str(s, t->v.sval ? t->v.sval : "_");
            pls_str(s, "\")");
            return;
        case TT_ILIT:
            pls_str(s, "(int "); pls_int(s, t->v.ival); pls_char(s, ')');
            return;
        case TT_FLIT:
            pls_str(s, "(flt "); pls_flt(s, t->v.dval); pls_char(s, ')');
            return;
        case TT_QLIT:
            pls_str(s, "(atom \"");
            pls_str(s, t->v.sval ? t->v.sval : "");
            pls_str(s, "\")");
            return;
        case TT_CUT:
            pls_str(s, "(cut)");
            return;
        case TT_NUL:
            pls_str(s, "(nul)");
            return;
        case TT_MAKELIST:
            ser_tree_list(s, t);
            return;
        case TT_IF:
            pls_str(s, "(if ");
            for (int i = 0; i < t->n; i++) {
                if (i) pls_char(s, ' ');
                ser_tree(s, t->c[i]);
            }
            pls_char(s, ')');
            return;
        case TT_PROGRAM:
            /* body conjunction — emit as (seq ...) */
            pls_str(s, "(seq");
            for (int i = 0; i < t->n; i++) {
                pls_char(s, ' ');
                ser_tree(s, t->c[i]);
            }
            pls_char(s, ')');
            return;
        case TT_FNC: {
            const char *fn = t->v.sval ? t->v.sval : "?";
            /* {} with no children is the empty-brace atom — match Term* (atom "{}") */
            if (strcmp(fn, "{}") == 0 && t->n == 0) {
                pls_str(s, "(atom \"{}\")");
                return;
            }
            /* TT_FNC(",") is binary conjunction in body — serialize as (seq ...).
             * But ','(X) used as a unary functor (e.g. call(','(A),B)) stays as (fnc ",").
             * Only flatten when it has exactly 2 children (the standard binary form). */
            if (strcmp(fn, ",") == 0 && t->n == 2) {
                pls_str(s, "(seq");
                ser_tree_conj_flat(s, t);
                pls_char(s, ')');
                return;
            }
            pls_str(s, "(fnc \""); pls_str(s, fn); pls_str(s, "\"");
            for (int i = 0; i < t->n; i++) {
                pls_char(s, ' ');
                ser_tree(s, t->c[i]);
            }
            pls_char(s, ')');
            return;
        }
        default:
            pls_str(s, "(unk)");
            return;
    }
}

static void ser_tree_list(PLS *s, tree_t *t) {
    /* TT_MAKELIST: v.ival=1 means last child is explicit tail (| Tail).
     * v.ival=0 means proper list — all children are elements. */
    int has_tail = (int)t->v.ival;
    int nelems = has_tail ? t->n - 1 : t->n;
    pls_str(s, "(list");
    for (int i = 0; i < nelems; i++) {
        pls_char(s, ' ');
        ser_tree(s, t->c[i]);
    }
    if (has_tail && t->n > 0) {
        pls_str(s, " |");
        ser_tree(s, t->c[t->n - 1]);
    }
    pls_char(s, ')');
}

/* Serialize full clause: Term* side */
static void ser_clause_term(PLS *s, PlClause *cl, VarScope *sc) {
    pls_str(s, "(clause ");
    if (cl->head) ser_term(s, cl->head, sc);
    else          pls_str(s, "(nul)");
    /* Use conj-flat for each body goal: directives store body[0] as a raw
     * conjunction term (not pre-flattened), so flatten here to match tree_t. */
    pls_str(s, " (seq");
    for (int i = 0; i < cl->nbody; i++) {
        pls_char(s, ' ');
        ser_term_conj_flat(s, cl->body[i], sc);
    }
    pls_str(s, "))");
}

/* Serialize full clause: tree_t side (TT_CLAUSE node) */
static void ser_clause_tree(PLS *s, tree_t *tr) {
    if (!tr || tr->t != TT_CLAUSE) { pls_str(s, "(bad-clause)"); return; }
    pls_str(s, "(clause ");
    ser_tree(s, tr->n > 0 ? tr->c[0] : NULL);
    pls_char(s, ' ');
    ser_tree(s, tr->n > 1 ? tr->c[1] : NULL);
    pls_char(s, ')');
}

/* The public verifier: compare Term* clause against its tree_t.
 * Returns 1 if equivalent, 0 if mismatch (prints diff to stderr). */
static int pl_verify_clause_tree(PlClause *cl, VarScope *sc, const char *filename) {
    if (!cl->tr) return 1;   /* directives skipped */
    PLS sa, sb;
    pls_init(&sa); pls_init(&sb);
    ser_clause_term(&sa, cl, sc);
    ser_clause_tree(&sb, cl->tr);
    int ok = (sa.err == 0 && sb.err == 0 && strcmp(sa.buf, sb.buf) == 0);
    if (!ok) {
        fprintf(stderr, "PST-PL-6c MISMATCH in %s line %d:\n  TERM: %s\n  TREE: %s\n",
                filename ? filename : "?", cl->lineno, sa.buf, sb.buf);
    }
    pls_free(&sa); pls_free(&sb);
    return ok;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static PlClause *parse_clause(Parser *p) {
    scope_reset(&p->sc);
    Token pk = lexer_peek(&p->lx);
    if (pk.kind == TK_EOF) return NULL;
    /* PST-PL-6b: save lexer + ifst_top before Term* parse so we can replay. */
    Lexer saved_lx  = p->lx;
    int   saved_ifst_top = p->ifst_top;
    int   is_dcg = 0;   /* PST-PL-6c: DCG clauses skip verification */
    PlClause *cl = calloc(1, sizeof(PlClause));
    cl->lineno = pk.line;
    if (pk.kind == TK_NECK) {
        lexer_next(&p->lx);
        Term *goal = parse_term(p, 1200);
        Token dot = lexer_next(&p->lx);
        if (dot.kind != TK_DOT)
            perror_at(p, dot.line, "expected . after directive");
        if (try_handle_if_directive(p, goal, cl->lineno)) {
            cl->head  = NULL;
            cl->body  = NULL;
            cl->nbody = 0;
            cl->tr    = NULL;   /* directives: no tree */
            return cl;
        }
        cl->head  = NULL;
        cl->body  = malloc(sizeof(Term *));
        cl->body[0] = goal;
        cl->nbody = 1;
        /* PST-PL-6b: replay directive through parallel path */
        {
            Parser p2 = *p;
            p2.lx = saved_lx;
            p2.ifst_top = saved_ifst_top;
            p2.in_args  = 0;
            TreeScope ts2; ts_reset(&ts2);
            lexer_next(&p2.lx);  /* consume TK_NECK */
            tree_t *body_tr = pt_term(&p2, &ts2, 1200);
            cl->tr = pt_make_clause(NULL, body_tr);
        }
        /* PST-PL-6c: verify */
        if (!pl_verify_clause_tree(cl, &p->sc, p->filename))
            p->tree_mismatches++;
        return cl;
    }
    Term *head = parse_term(p, 1199);
    cl->head = head;
    pk = lexer_peek(&p->lx);
    if (pk.kind == TK_NECK) {
        lexer_next(&p->lx);
        Term *body_term = parse_term(p, 1200);
        int n = count_conj(body_term);
        cl->body  = malloc((n ? n : 1) * sizeof(Term *));
        cl->nbody = flatten_conj(body_term, cl->body, 0);
    } else if (pk.kind == TK_OP && strcmp(pk.text, "-->") == 0) {
        lexer_next(&p->lx);
        Term *dcg_body = parse_term(p, 1200);
        Term *pushback = NULL;
        Term *hd = term_deref(cl->head);
        int comma_id = prolog_atom_intern(",");
        if (hd->tag == TERM_COMPOUND && hd->compound.functor == comma_id && hd->compound.arity == 2) {
            cl->head = hd->compound.args[0];
            pushback = hd->compound.args[1];
        }
        dcg_expand_clause(cl, dcg_body, pushback, &p->sc);
        /* DCG: Term* has been expanded; tree_t carries raw pre-expansion form.
         * Shapes legitimately differ — skip 6c verification for DCG clauses. */
        cl->tr = NULL;  /* NULL tr → verifier skips it */
        is_dcg = 1;
    } else {
        cl->body  = NULL;
        cl->nbody = 0;
    }
    Token dot = lexer_next(&p->lx);
    if (dot.kind != TK_DOT)
        perror_at(p, dot.line, "expected . at end of clause");
    /* PST-PL-6b: replay clause through parallel path (skip DCG) */
    if (!is_dcg) {
        Parser p2 = *p;
        p2.lx = saved_lx;
        p2.ifst_top = saved_ifst_top;
        p2.in_args  = 0;
        TreeScope ts2; ts_reset(&ts2);
        tree_t *head_tr = pt_term(&p2, &ts2, 1199);
        Token pk2 = lexer_peek(&p2.lx);
        tree_t *body_tr = NULL;
        if (pk2.kind == TK_NECK) {
            lexer_next(&p2.lx);
            body_tr = pt_term(&p2, &ts2, 1200);
        }
        cl->tr = pt_make_clause(head_tr, body_tr);
    }
    /* PST-PL-6c: verify (skipped when is_dcg, since cl->tr == NULL) */
    if (!pl_verify_clause_tree(cl, &p->sc, p->filename))
        p->tree_mismatches++;
    /* PST-PL-6e: snapshot named-var name→Term* mapping for pre-lower slot pass in lower_clause */
    if (p->sc.count > 0) {
        cl->nvar      = p->sc.count;
        cl->var_names = malloc((size_t)p->sc.count * sizeof(char *));
        cl->var_terms = malloc((size_t)p->sc.count * sizeof(Term *));
        for (int _vi = 0; _vi < p->sc.count; _vi++) {
            cl->var_names[_vi] = p->sc.entries[_vi].name;
            cl->var_terms[_vi] = p->sc.entries[_vi].term;
        }
    }
    return cl;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
PlProgram *prolog_parse(const char *src, const char *filename) {
    prolog_atom_init();
    Parser p;
    lexer_init(&p.lx, src);
    p.filename = filename ? filename : "<input>";
    p.nerrors  = 0;
    p.ifst_top = 0;
    p.in_args  = 0;
    p.tree_mismatches = 0;
    scope_reset(&p.sc);
    PlProgram *prog = calloc(1, sizeof(PlProgram));
    for (;;) {
        Token pk = lexer_peek(&p.lx);
        if (pk.kind == TK_EOF) break;
        if (pk.kind == TK_ERROR) {
            fprintf(stderr, "%s:%d: lex error: %s\n",
                    p.filename, pk.line, pk.text);
            p.nerrors++;
            lexer_next(&p.lx);
            continue;
        }
        PlClause *cl = parse_clause(&p);
        if (!cl) break;
        if (cl->head == NULL && cl->body == NULL && cl->nbody == 0) {
            free(cl);
            continue;
        }
        if (!if_currently_active(&p)) {
            if (cl->body) free(cl->body);
            free(cl);
            continue;
        }
        if (!prog->head) prog->head = cl;
        else             prog->tail->next = cl;
        prog->tail = cl;
        prog->nclauses++;
    }
    if (p.ifst_top != 0) {
        fprintf(stderr, "%s: parse error: unmatched :- if (opened at line %d)\n",
                p.filename, p.ifst[p.ifst_top - 1].line);
        p.nerrors++;
    }
    prog->nerrors = p.nerrors;
    prog->tree_mismatches = p.tree_mismatches;
    return prog;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void term_pretty(Term *t, FILE *out) {
    t = term_deref(t);
    if (!t) { fprintf(out, "<null>"); return; }
    switch (t->tag) {
        case TERM_ATOM: {
            const char *n = prolog_atom_name(t->atom_id);
            if (!n) n = "?";
            int needs_quote = !islower((unsigned char)n[0]) && n[0] != '[';
            for (const char *c = n; *c && !needs_quote; c++)
                if (!isalnum((unsigned char)*c) && *c != '_') needs_quote = 1;
            if (needs_quote && strcmp(n,"[]") != 0 && strcmp(n,"!")!=0 &&
                strcmp(n,",")!=0 && strcmp(n,".")!=0)
                fprintf(out, "'%s'", n);
            else
                fprintf(out, "%s", n);
            break;
        }
        case TERM_VAR:
            fprintf(out, "_V%d", t->var_slot < 0 ? 0 : t->var_slot);
            break;
        case TERM_INT:
            fprintf(out, "%ld", t->ival);
            break;
        case TERM_FLOAT:
            fprintf(out, "%g", t->fval);
            break;
        case TERM_COMPOUND: {
            const char *fn = prolog_atom_name(t->compound.functor);
            if (!fn) fn = "?";
            if (t->compound.functor == ATOM_DOT && t->compound.arity == 2) {
                fprintf(out, "[");
                term_pretty(t->compound.args[0], out);
                Term *tail = term_deref(t->compound.args[1]);
                while (tail && tail->tag == TERM_COMPOUND &&
                       tail->compound.functor == ATOM_DOT && tail->compound.arity == 2) {
                    fprintf(out, ",");
                    term_pretty(tail->compound.args[0], out);
                    tail = term_deref(tail->compound.args[1]);
                }
                if (tail && !(tail->tag == TERM_ATOM && tail->atom_id == ATOM_NIL)) {
                    fprintf(out, "|");
                    term_pretty(tail, out);
                }
                fprintf(out, "]");
                break;
            }
            if (t->compound.arity == 2 && find_binop(fn)) {
                fprintf(out, "(");
                term_pretty(t->compound.args[0], out);
                fprintf(out, " %s ", fn);
                term_pretty(t->compound.args[1], out);
                fprintf(out, ")");
                break;
            }
            fprintf(out, "%s(", fn);
            for (int i = 0; i < t->compound.arity; i++) {
                if (i) fprintf(out, ",");
                term_pretty(t->compound.args[i], out);
            }
            fprintf(out, ")");
            break;
        }
        case TERM_REF:
            term_pretty(t->ref, out);
            break;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void prolog_program_pretty(PlProgram *prog, FILE *out) {
    for (PlClause *cl = prog->head; cl; cl = cl->next) {
        if (!cl->head) {
            fprintf(out, ":- ");
            if (cl->nbody > 0) term_pretty(cl->body[0], out);
            fprintf(out, ".\n");
            continue;
        }
        term_pretty(cl->head, out);
        if (cl->nbody > 0) {
            fprintf(out, " :-\n");
            for (int i = 0; i < cl->nbody; i++) {
                fprintf(out, "    ");
                term_pretty(cl->body[i], out);
                if (i + 1 < cl->nbody) fprintf(out, ",\n");
            }
        }
        fprintf(out, ".\n");
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void prolog_program_free(PlProgram *prog) {
    PlClause *cl = prog->head;
    while (cl) {
        PlClause *next = cl->next;
        free(cl->body);
        free(cl);
        cl = next;
    }
    free(prog);
}
