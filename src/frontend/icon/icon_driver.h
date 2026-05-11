#ifndef ICON_DRIVER_H
#define ICON_DRIVER_H
#include "../../ast/ast.h"
/* icon_compile: parse source, set *out_ast to AST_PROGRAM. */
void icon_compile(const char *source, const char *filename, tree_t **out_ast);
#endif
