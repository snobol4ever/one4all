#ifndef REBUS_LOWER_H
#define REBUS_LOWER_H
/*
 * rebus_lower.h — public API for Rebus → unified IR lowering pass
 *
 * Milestone: M-G5-LOWER-REBUS-FIX
 */

#include "rebus.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "../../ast/ast.h"

/*
 * rebus_lower(rp)
 *   Walk RProgram* produced by rebus_parse() and return a CODE_t*
 *   whose STMT_t list is ready for asm_emit / jvm_emit / net_emit.
 *   Returns NULL on error (messages printed to stderr).
 */
CODE_t *rebus_lower(RProgram *rp);

/*
 * rebus_compile(src, filename) — FI-1A
 *   Full pipeline: parse src string via rebus_parse(), lower via
 *   rebus_lower(), tag all STMT_t with LANG_REB. Mirrors icon_compile().
 *   SI-5: out_ast receives AST_PROGRAM; pass NULL to discard.
 *   Returns NULL on parse or lower error.
 */
CODE_t *rebus_compile(const char *src, const char *filename, AST_t **out_ast);

#endif /* REBUS_LOWER_H */
