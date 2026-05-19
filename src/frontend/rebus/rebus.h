#ifndef REBUS_H
#define REBUS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../ast/ast.h"
/* RCase — parser-local scratch accumulator for caselist reductions only.
   No RCase* pointer may survive in any tree_t output. (PST-RB-DECL-3 Option A) */
typedef struct RCase RCase;
struct RCase {
    int       is_default;
    tree_t   *guard_tree;
    tree_t   *body_tree;
    RCase    *next;
};
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
/* PST-RB-DECL-2: parser output is now tree_t* (TT_PROGRAM of TT_FUNCTION/TT_RECORD_DECL nodes). */
tree_t *rebus_parse(FILE *f, const char *filename);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void rebus_print(tree_t *prog, FILE *out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void rebus_error(int lineno, const char *fmt, ...);
extern int   rebus_nerrors;
extern char *rebus_filename;
#endif
