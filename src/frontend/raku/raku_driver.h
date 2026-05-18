#ifndef RAKU_DRIVER_H
#define RAKU_DRIVER_H
#include "../../ast/ast.h"
/* SUB_TAG_ID — non-zero sentinel stamped on tree_t._id of TT_FNC nodes that represent Raku `sub` */
/* definitions (proc-defs), distinguishing them from regular TT_FNC call expressions of identical */
/* AST shape. Consumed by lower.c::lower_stmt LANG_RAKU branch to lower proc bodies directly      */
/* instead of emitting a spurious SM_CALL_FN <name> wrapper. (IJ-HELLO-2b, 2026-05-18.)           */
#define SUB_TAG_ID 1
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void raku_compile(const char *src, const char *filename, tree_t **out_ast);
#endif
