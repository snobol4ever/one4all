/*============================================================================================================================
 * coro_value.h — Pure-BB value-context evaluator for Icon Byrd boxes (RS-17a)
 *
 * `bb_eval_value` evaluates an tree_t in **value context** without going through
 * `interp_eval` (the IR-mode-only driver tree-walker).  It is the value-context
 * analog of `coro_eval` (which builds a Byrd-box generator).
 *
 * Coverage today (RS-17a, partial):
 *   - AST_ILIT, AST_FLIT, AST_QLIT, AST_NUL, AST_KEYWORD : delegate to eval_node
 *   - AST_VAR : Icon-frame slot read when frame_depth > 0; else delegate to eval_node
 *   - All other kinds : fall through to interp_eval (TEMPORARY — RS-17a-cont)
 *
 * The fallthrough is the **migration scaffold**: as more coro_runtime.c sites
 * route through bb_eval_value, the kinds that arrive can be observed and
 * handled here directly.  When the fallthrough is unreachable, the
 * `extern interp_eval` declaration goes away and the isolation gate can
 * include coro_runtime.c.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
 *==========================================================================================================================*/

#ifndef CORO_VALUE_H
#define CORO_VALUE_H

#include "../ast/ast.h"
#include "snobol4.h"      /* DESCR_t */

DESCR_t bb_eval_value(tree_t *e);

/* GOAL-ICON-BB-COMPLETE A3-seed-fix: canonical Icon ?E LCG seed.
 * Defined in coro_value.c, referenced by sm_interp.c (SM ICN_RANDOM)
 * and interp_eval.c (AST_RANDOM fallback) so all three modes advance
 * one shared sequence. */
extern unsigned long bb_icn_rnd_seed;

#endif /* CORO_VALUE_H */
