/*============================================================================================================================
 * icn_value.h — Pure-BB value-context evaluator for Icon Byrd boxes (RS-17a)
 *
 * `bb_eval_value` evaluates an tree_t in **value context** without going through
 * `interp_eval` (the IR-mode-only driver tree-walker).  It is the value-context
 * analog of `icn_bb_build` (which builds a Byrd-box generator).
 *
 * Coverage today (RS-17a, partial):
 *   - TT_ILIT, TT_FLIT, TT_QLIT, TT_NUL, TT_KEYWORD : delegate to eval_node
 *   - TERM_VAR : Icon-frame slot read when frame_depth > 0; else delegate to eval_node
 *   - All other kinds : fall through to interp_eval (TEMPORARY — RS-17a-cont)
 *
 * The fallthrough is the **migration scaffold**: as more icn_runtime.c sites
 * route through bb_eval_value, the kinds that arrive can be observed and
 * handled here directly.  When the fallthrough is unreachable, the
 * `extern interp_eval` declaration goes away and the isolation gate can
 * include icn_runtime.c.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
 *==========================================================================================================================*/

#ifndef ICN_VALUE_H
#define ICN_VALUE_H

#include "../ast/ast.h"
#include "snobol4.h"      /* DESCR_t */

DESCR_t bb_eval_value(tree_t *e);

/* GOAL-ICON-BB-COMPLETE A3-seed-fix: canonical Icon ?E LCG seed.
 * Defined in icn_value.c, referenced by sm_interp.c (SM ICN_RANDOM)
 * and interp_eval.c (TT_RANDOM fallback) so all three modes advance
 * one shared sequence. */
extern unsigned long bb_icn_rnd_seed;

#endif /* ICN_VALUE_H */
