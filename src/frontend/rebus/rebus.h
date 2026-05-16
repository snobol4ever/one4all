#ifndef REBUS_H
#define REBUS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../ast/ast.h"
typedef struct RCase RCase;
struct RCase {
    int       is_default;
    tree_t   *guard_tree;
    tree_t   *body_tree;
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
    tree_t   *initial_tree;
    tree_t   *body_tree;
    char    **fields;
    int       nfields;
    RDecl    *next;
};
typedef struct {
    RDecl  *decls;
    int     ndecls;
} RProgram;
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
