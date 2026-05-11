#ifndef ICON_DRIVER_H
#define ICON_DRIVER_H
#include "../snobol4/scrip_cc.h"
#include "../../ast/ast.h"
/* SI-5: out_ast receives an AST_PROGRAM built in parallel with CODE_t.
 * Pass NULL to discard (pre-SI-5 callers or polyglot path). */
CODE_t *icon_compile(const char *source, const char *filename, AST_t **out_ast);
#endif
