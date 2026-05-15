#ifndef PL_PARSE_H
#define PL_PARSE_H
#include "term.h"
#include "prolog_atom.h"
#include <stdio.h>
typedef struct PlClause PlClause;
struct PlClause {
    Term     *head;
    Term    **body;
    int       nbody;
    int       lineno;
    PlClause *next;
};
typedef struct {
    PlClause *head;
    PlClause *tail;
    int       nclauses;
    int       nerrors;
} PlProgram;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
PlProgram *prolog_parse(const char *src, const char *filename);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void prolog_program_pretty(PlProgram *prog, FILE *out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void prolog_program_free(PlProgram *prog);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void term_pretty(Term *t, FILE *out);
#endif
