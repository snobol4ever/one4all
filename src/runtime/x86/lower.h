/*
 * sm_lower.h — IR → SM_Program compiler pass (M-SCRIP-U3)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-04-06
 */

#ifndef LOWER_H
#define LOWER_H

#include "sm_prog.h"
#include "../../ast/ast.h"

/*
 * lower — compile an AST_PROGRAM node into a flat SM_Program.
 *
 * prog must be an AST_PROGRAM whose children are AST_STMT / AST_END nodes
 * produced by code_to_ast() (SI-2 shim) or directly by a frontend (SI-4+).
 *
 * The caller owns the returned SM_Program and must free it with sm_prog_free().
 * AST_t trees must remain valid for the duration of this call (lower does not
 * deep-copy strings; it borrows sval pointers in the GC heap or interned).
 *
 * Returns NULL on allocation failure.
 */
SM_Program *lower(const AST_t *prog);

#endif /* LOWER_H */
