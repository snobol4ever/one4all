#ifndef PROLOG_LOWER_H
#define PROLOG_LOWER_H
/*
 * prolog_lower.h — Prolog ClauseAST -> scrip-cc IR lowering
 */

#include "scrip_cc.h"
#include "prolog_parse.h"
#include <stdio.h>

CODE_t *prolog_lower(PlProgram *pl_prog);
void prolog_lower_pretty(CODE_t *prog, FILE *out);

/*
 * pl_assert_term — build an TT_CLAUSE tree_t from a runtime Term* (for assertz/asserta).
 * head-only Term → fact; ':-'(Head,Body) → rule.
 * Sets *functor_out and *arity_out to the predicate key.
 * Returns NULL on error.
 */
tree_t *pl_assert_term(Term *t, int *functor_out, int *arity_out);

#endif /* PROLOG_LOWER_H */
