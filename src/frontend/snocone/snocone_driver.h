/*
 * snocone_driver.h — Snocone frontend public API
 *
 * snocone_compile: parse source, set *out_ast to TT_PROGRAM.
 * Returns void; caller owns *out_ast (GC-managed).
 */
#pragma once
#include "../../ast/ast.h"

void snocone_compile(const char *source, const char *filename, tree_t **out_ast);
