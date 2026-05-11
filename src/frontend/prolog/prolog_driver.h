#ifndef PROLOG_DRIVER_H
#define PROLOG_DRIVER_H
#include "../../ast/ast.h"
/* prolog_compile: parse source, set *out_ast to TT_PROGRAM. */
void prolog_compile(const char *source, const char *filename, tree_t **out_ast);
#endif
