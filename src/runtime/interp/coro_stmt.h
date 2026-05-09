/*============================================================================================================================
 * coro_stmt.h — Pure-BB statement-context evaluator for Icon Byrd boxes (RS-17b)
 *
 * `bb_exec_stmt` executes an AST_t in **statement context** without going
 * through `interp_eval` (the IR-mode-only driver tree-walker).  It is the
 * statement-context analog of `bb_eval_value` (which produces a single
 * DESCR_t).  Statement context means: side effects only — no caller-visible
 * return value.  Control-flow effects (FRAME.returning, FRAME.loop_break,
 * FRAME.suspending, scope env writes) are observed by the caller via the
 * IcnFrame state, exactly as they are with the `interp_eval` it replaces.
 *
 * Coverage today (RS-17b, scaffold):
 *   - All kinds : fall through to interp_eval (TEMPORARY — RS-17b-cont).
 *
 * The fallthrough is the **migration scaffold**: with all 13 statement-context
 * call sites in coro_runtime.c routed through bb_exec_stmt, the kinds that
 * actually arrive can be observed and lifted into a switch in this file
 * one-by-one in subsequent sub-rungs.  When the fallthrough is unreachable,
 * the `extern interp_eval` declaration goes away and (combined with the
 * matching closure for pl_runtime.c — RS-18) the isolation gate can promote
 * coro_runtime.c (RS-19).
 *
 * Mirrors the RS-17a coro_value.c convention exactly: a thin trampoline that
 * delegates today, with structured space to absorb the dispatched kinds as
 * follow-on rungs land them.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
 *==========================================================================================================================*/

#ifndef CORO_STMT_H
#define CORO_STMT_H

#include "../ir/ast.h"
#include "snobol4.h"      /* DESCR_t */

/* Execute e as an Icon statement.  Side effects propagate through IcnFrame
 * state (FRAME.returning, FRAME.loop_break, FRAME.suspending, etc.).  No
 * caller-visible return value; mirrors the discarded-result semantics of
 * the 13 call sites this replaces in coro_runtime.c. */
void bb_exec_stmt(AST_t *e);

#endif /* CORO_STMT_H */
