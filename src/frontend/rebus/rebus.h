#ifndef REBUS_H
#define REBUS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
typedef enum {
    RE_STR,
    RE_INT,
    RE_REAL,
    RE_NULL,
    RE_VAR,
    RE_KEYWORD,
    RE_NEG,
    RE_POS,
    RE_NOT,
    RE_VALUE,
    RE_BANG,
    RE_ADD,
    RE_SUB,
    RE_MUL,         /* *  */
    RE_DIV,
    RE_MOD,
    RE_POW,
    RE_STRCAT,
    RE_PATCAT,
    RE_ALT,
    RE_EQ,
    RE_NE,
    RE_LT,
    RE_LE,
    RE_GT,
    RE_GE,
    RE_SEQ,
    RE_SNE,
    RE_SLT,
    RE_SLE,
    RE_SGT,
    RE_SGE,
    RE_ASSIGN,
    RE_EXCHANGE,
    RE_ADDASSIGN,
    RE_SUBASSIGN,
    RE_CATASSIGN,
    RE_CALL,
    RE_SUB_IDX,
    RE_RANGE,
    RE_COND,
    RE_IMM,
    RE_CURSOR,
    RE_DEREF,       /* *var        deferred pattern reference */
    RE_PATOPT,
    RE_AUG,
} REKind;
typedef struct RExpr RExpr;
struct RExpr {
    REKind    kind;
    int       lineno;
    char     *sval;
    long      ival;
    double    dval;
    RExpr    *left;
    RExpr    *right;
    RExpr   **args;
    int       nargs;
    REKind    augop;
};
typedef enum {
    RS_EXPR,
    RS_ASSIGN,
    RS_IF,
    RS_UNLESS,
    RS_WHILE,
    RS_UNTIL,
    RS_REPEAT,
    RS_FOR,
    RS_CASE,
    RS_EXIT,
    RS_NEXT,
    RS_FAIL,
    RS_RETURN,
    RS_STOP,
    RS_MATCH,
    RS_REPLACE,
    RS_REPLN,
    RS_COMPOUND,
} RSKind;
typedef struct RStmt RStmt;
typedef struct RCase RCase;
struct RStmt {
    RSKind    kind;
    int       lineno;
    RExpr    *expr;
    RExpr    *pat;
    RExpr    *repl;
    RStmt    *body;
    RStmt    *alt;
    char     *for_var;
    RExpr    *for_from;
    RExpr    *for_to;
    RExpr    *for_by;
    RExpr    *case_expr;
    RCase    *cases;
    RExpr    *retval;
    RStmt   **stmts;
    int       nstmts;
    RStmt    *next;
};
struct RCase {
    int       is_default;
    RExpr    *guard;
    RStmt    *body;
    RCase    *next;
};
typedef enum {
    RD_FUNCTION,
    RD_RECORD,
} RDKind;
typedef struct RDecl RDecl;
struct RDecl {
    RDKind    kind;
    int       lineno;
    char     *name;
    char    **params;
    int       nparams;
    char    **locals;
    int       nlocals;
    RStmt    *initial;
    RStmt    *body;
    char    **fields;
    int       nfields;
    RDecl    *next;
};
typedef struct {
    RDecl  *decls;
    int     ndecls;
} RProgram;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline RExpr *rexpr_new(REKind k, int lineno) {
    RExpr *e = calloc(1, sizeof *e);
    e->kind   = k;
    e->lineno = lineno;
    return e;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline RStmt *rstmt_new(RSKind k, int lineno) {
    RStmt *s = calloc(1, sizeof *s);
    s->kind   = k;
    s->lineno = lineno;
    return s;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline RDecl *rdecl_new(RDKind k, int lineno) {
    RDecl *d = calloc(1, sizeof *d);
    d->kind   = k;
    d->lineno = lineno;
    return d;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline RCase *rcase_new(int lineno) {
    RCase *c = calloc(1, sizeof *c);
    (void)lineno;
    return c;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline char *rebus_intern(const char *s) {
    return s ? strdup(s) : NULL;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline char *rebus_intern_n(const char *s, int n) {
    char *p = malloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
RProgram *rebus_parse(FILE *f, const char *filename);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void rebus_print(RProgram *prog, FILE *out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void rebus_error(int lineno, const char *fmt, ...);
extern int   rebus_nerrors;
extern char *rebus_filename;
#endif
