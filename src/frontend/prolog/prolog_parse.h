#ifndef PL_PARSE_H
#define PL_PARSE_H
#include "term.h"
#include "prolog_atom.h"
#include "ast.h"
#include <stdio.h>
typedef struct PlClause PlClause;
struct PlClause {
    Term     *head;
    Term    **body;
    int       nbody;
    int       lineno;
    PlClause *next;
    /* PST-PL-6b: parallel tree_t path — pure syntax tree, no slot assignment */
    tree_t   *tr;
    /* PST-PL-6e: snapshot of VarScope name→Term* mapping for pre-lower slot assignment */
    char    **var_names;
    Term    **var_terms;
    int       nvar;
};
typedef struct {
    PlClause *head;
    PlClause *tail;
    int       nclauses;
    int       nerrors;
    int       tree_mismatches;   /* PST-PL-6c: count of Term*↔tree_t shape mismatches */
} PlProgram;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
PlProgram *prolog_parse(const char *src, const char *filename);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void prolog_program_pretty(PlProgram *prog, FILE *out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void prolog_program_free(PlProgram *prog);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void term_pretty(Term *t, FILE *out);
/* PST-PL-6c: returns number of Term*↔tree_t mismatches found during parse. */
int prolog_program_tree_mismatches(PlProgram *prog);
#endif
