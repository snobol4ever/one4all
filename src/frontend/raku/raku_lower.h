/*
 * raku_lower.h — Tiny-Raku AST → unified IR lowering pass API
 *
 * Converts RakuNode trees (produced by raku_parse.c) to EXPR_t trees
 * (using canonical EKind from ir/ir.h).  After this pass the RakuNode tree
 * is frontend-private; emitters operate only on EXPR_t.
 *
 * Key mappings (RK-3, 2026-04-14):
 *   gather { body }      → E_ITERATE(body)
 *   take expr            → E_SUSPEND(expr)
 *   for RANGE -> $v body → E_EVERY(E_TO(lo,hi), body)
 *   say expr             → E_FNC("write", [expr])
 *   my $x = expr / $x=e → E_ASSIGN(E_VAR("x"), rhs)
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */
#ifndef RAKU_LOWER_H
#define RAKU_LOWER_H

#include "raku_ast.h"
#include "../../ir/ir.h"

/* Lower one top-level RakuNode statement/sub → EXPR_t.
 * Returns an E_FNC node (name="main" for top-level block) or
 * an E_FNC node (name=sub name) for sub definitions. */
EXPR_t *raku_lower_stmt(const RakuNode *n);

/* Lower a top-level RK_BLOCK program → array of EXPR_t* procedures.
 * Sub definitions become individual E_FNC entries.
 * Top-level statements are wrapped in a synthetic "main" E_FNC.
 * Returns malloc'd EXPR_t** of length *out_count.  Caller owns result. */
EXPR_t **raku_lower_file(RakuNode **stmts, int count, int *out_count);

#endif /* RAKU_LOWER_H */
