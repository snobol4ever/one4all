#pragma once
/* ast_clone.h — IR tree cloning into GC memory, and CODE_t freeing (RS-9b)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (RS-9b, 2026-05-02)
 *
 * ast_gc_clone: deep-copies an AST_t subtree allocating with GC_malloc so
 *   the clone survives after the original calloc'd IR tree is freed.
 *   Used by sm_lower before storing AST_t* into SM_PUSH_EXPR a[0].ptr.
 *
 * code_free: frees a CODE_t (STMT_t list + AST_t trees) allocated via
 *   calloc/realloc in the parser. Safe to call after sm_lower returns.
 */

#include "../ir/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"

/* Deep-clone expr tree e into GC-managed memory.  Returns NULL if e==NULL. */
AST_t *ast_gc_clone(const AST_t *e);

/* Free a CODE_t and all its STMT_t / AST_t nodes (calloc-allocated). */
void code_free(CODE_t *prog);
