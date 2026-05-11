#ifndef PROLOG_DRIVER_H
#define PROLOG_DRIVER_H
#include "../snobol4/scrip_cc.h"
#include "../../ast/ast.h"
/* SI-5: out_ast receives AST_PROGRAM; pass NULL to discard. */
CODE_t *prolog_compile(const char *source, const char *filename, AST_t **out_ast);
#endif
