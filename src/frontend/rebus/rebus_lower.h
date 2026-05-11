/*
 * rebus_lower.h — public API for Rebus → unified IR lowering pass
 */
#pragma once
#include "rebus.h"
#include "../../ast/ast.h"

/* Internal: Walk RProgram* and produce AST_PROGRAM via CODE_t shim. */
CODE_t *rebus_lower(RProgram *rp);

/* Public: full pipeline: parse src string, set *out_ast to AST_PROGRAM. */
void rebus_compile(const char *src, const char *filename, tree_t **out_ast);
