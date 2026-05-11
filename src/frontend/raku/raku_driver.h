/*
 * raku_driver.h — Tiny-Raku compiler entry point
 */
#ifndef RAKU_DRIVER_H
#define RAKU_DRIVER_H
#include "../../ast/ast.h"
/* raku_compile: parse source, set *out_ast to TT_PROGRAM. */
void raku_compile(const char *src, const char *filename, tree_t **out_ast);
#endif /* RAKU_DRIVER_H */
