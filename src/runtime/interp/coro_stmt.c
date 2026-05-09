/*============================================================================================================================
 * coro_stmt.c — RS-17b/RS-21: pure-BB statement-context evaluator for Icon Byrd boxes.
 *
 * See coro_stmt.h for the contract.
 *
 * RS-21 (2026-05-03): native dispatch for Icon statement-level kinds.
 *   AST_WHILE, AST_UNTIL, AST_REPEAT, AST_IF, AST_SEQ, AST_SEQ_EXPR, AST_LOOP_NEXT,
 *   AST_LOOP_BREAK, AST_RETURN, AST_PROC_FAIL, AST_SUSPEND.
 *
 * These kinds previously fell through to `interp_eval` (the IR-mode tree
 * walker).  RS-21 lifts each one into an explicit case here, with internal
 * value-context recursions going through `bb_eval_value` and internal
 * statement-context body recursions going back through `bb_exec_stmt`.
 *
 * After RS-21, the only kinds reaching the trailing fallthrough should be
 * AST_FNC (Icon proc/builtin call as statement), AST_ASSIGN (slot store), and
 * a handful of expression-kind escapees that mode-1 was happy to swallow
 * with its DESCR_t-discarding contract.  RS-22 absorbs AST_FNC + AST_ASSIGN.
 *
 * The 13 statement-context call sites in coro_runtime.c (proc bodies,
 * loop bodies, do-clauses, every-bodies, do-clause re-entry after suspend)
 * route here via bb_exec_stmt(...).
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet
 * SPRINT:  RS-17b (2026-05-03), extended by RS-21 (2026-05-03)
 *==========================================================================================================================*/

#include "coro_stmt.h"
#include "coro_value.h"
#include "coro_runtime.h"   /* FRAME, frame_depth, IcnFrame */
#include "snobol4.h"        /* DESCR_t, IS_FAIL_fn, FAILDESCR, NULVCL */

/* RS-23e (closes RS-23 arc): the `interp_eval` extern is gone.  Diag
 * verified zero IR fallthrough from any BB-adapter ancestor across smoke
 * + unified_broker + full Icon corpus 263.  Any kind not handled by an
 * explicit case below is a four-mode isolation violation and aborts via
 * `bb_exec_stmt`'s diagnostic at the end of the function. */

/*------------------------------------------------------------------------------------------------------------------------------
 * bb_exec_stmt — execute e in statement context.
 *
 * Statement context means: side effects only.  Control-flow effects propagate
 * through IcnFrame state (FRAME.returning, FRAME.loop_break, FRAME.loop_next,
 * FRAME.suspending) which the caller observes after this returns.
 *
 * The dispatch mirrors the icon-frame switch in interp_eval.c (lines ~2300-
 * 2416) and the shared switch (lines ~3475-3567), with two changes:
 *  (a) value-context children are evaluated via `bb_eval_value` instead of
 *      `interp_eval`, keeping the call graph IR-free.
 *  (b) statement-context body children recurse via `bb_exec_stmt`, again
 *      avoiding interp_eval.
 *----------------------------------------------------------------------------------------------------------------------------*/
void bb_exec_stmt(AST_t *e)
{
    if (!e) return;

    switch (e->kind) {

    /*========================================================================
     * Trivial control-flow markers — set FRAME state, no children.
     *======================================================================*/
    case AST_LOOP_NEXT: {
        /* `next` — abort body, ask enclosing loop to advance. */
        FRAME.loop_next = 1;
        return;
    }
    case AST_LOOP_BREAK: {
        /* `break` — exit enclosing loop. */
        FRAME.loop_break = 1;
        return;
    }
    case AST_PROC_FAIL: {
        /* `fail` — procedure-level fail return. */
        FRAME.returning  = 1;
        FRAME.return_val = FAILDESCR;
        return;
    }
    case AST_RETURN: {
        DESCR_t rv = (e->nchildren > 0) ? bb_eval_value(e->children[0]) : NULVCL;
        FRAME.returning  = 1;
        FRAME.return_val = rv;
        return;
    }

    /*========================================================================
     * AST_SUSPEND — yield a value to coro_drive_fnc loop.
     *======================================================================*/
    case AST_SUSPEND: {
        DESCR_t val = (e->nchildren > 0) ? bb_eval_value(e->children[0]) : NULVCL;
        if (!IS_FAIL_fn(val)) {
            FRAME.suspending  = 1;
            FRAME.suspend_val = val;
            FRAME.suspend_do  = (e->nchildren > 1) ? e->children[1] : NULL;
        }
        return;
    }

    /*========================================================================
     * Conditional — AST_IF.
     * Goal-directed test (IC-8): if the condition is suspendable, pump it
     * via coro_eval; first non-fail value fires then-branch, exhaustion fires
     * else-branch.  Otherwise classic single-shot evaluation.
     *======================================================================*/
    case AST_IF: {
        if (e->nchildren < 1) return;
        AST_t *test = e->children[0];
        if (is_suspendable(test)) {
            bb_node_t box = coro_eval(test);
            DESCR_t v = box.fn(box.ζ, α);
            if (!IS_FAIL_fn(v) && !FRAME.returning && !FRAME.loop_break) {
                if (e->nchildren > 1) bb_exec_stmt(e->children[1]);
            } else {
                if (e->nchildren > 2) bb_exec_stmt(e->children[2]);
            }
            return;
        }
        DESCR_t cv = bb_eval_value(test);
        if (!IS_FAIL_fn(cv)) {
            if (e->nchildren > 1) bb_exec_stmt(e->children[1]);
        } else {
            if (e->nchildren > 2) bb_exec_stmt(e->children[2]);
        }
        return;
    }

    /*========================================================================
     * Loops — AST_WHILE, AST_UNTIL, AST_REPEAT.
     * Each saves/restores loop_break and loop_next around the loop, exits
     * on returning / loop_break / suspending, and re-runs the body via
     * bb_exec_stmt.
     *======================================================================*/
    case AST_WHILE: {
        int saved_brk = FRAME.loop_break; FRAME.loop_break = 0;
        int saved_nxt = FRAME.loop_next;  FRAME.loop_next  = 0;
        while (!FRAME.returning && !FRAME.loop_break && !FRAME.suspending) {
            DESCR_t cv = (e->nchildren > 0) ? bb_eval_value(e->children[0]) : FAILDESCR;
            if (IS_FAIL_fn(cv)) break;
            FRAME.loop_next = 0;
            if (e->nchildren > 1) bb_exec_stmt(e->children[1]);
            if (FRAME.suspending) break;
        }
        FRAME.loop_break = saved_brk;
        FRAME.loop_next  = saved_nxt;
        return;
    }
    case AST_UNTIL: {
        int saved_brk = FRAME.loop_break; FRAME.loop_break = 0;
        int saved_nxt = FRAME.loop_next;  FRAME.loop_next  = 0;
        while (!FRAME.returning && !FRAME.loop_break && !FRAME.suspending) {
            DESCR_t cv = (e->nchildren > 0) ? bb_eval_value(e->children[0]) : FAILDESCR;
            if (!IS_FAIL_fn(cv)) break;
            FRAME.loop_next = 0;
            if (e->nchildren > 1) bb_exec_stmt(e->children[1]);
            if (FRAME.suspending) break;
        }
        FRAME.loop_break = saved_brk;
        FRAME.loop_next  = saved_nxt;
        return;
    }
    case AST_REPEAT: {
        int saved_brk = FRAME.loop_break; FRAME.loop_break = 0;
        int saved_nxt = FRAME.loop_next;  FRAME.loop_next  = 0;
        while (!FRAME.returning && !FRAME.loop_break && !FRAME.suspending) {
            FRAME.loop_next = 0;
            if (e->nchildren > 0) {
                bb_exec_stmt(e->children[0]);
                if (FRAME.suspending) break;
            }
        }
        FRAME.loop_break = saved_brk;
        FRAME.loop_next  = saved_nxt;
        return;
    }

    /*========================================================================
     * Sequences — AST_SEQ, AST_SEQ_EXPR.
     * Statement-context: discard each child's value.  Honour returning /
     * loop_break / loop_next as exit conditions.
     * AST_SEQ honours the IC-9 "& conjunction" semantics (fail on any
     * sub-failure), but in statement context the caller doesn't observe the
     * fail — the child stmt's side effects already happened.
     *======================================================================*/
    case AST_SEQ:
    case AST_SEQ_EXPR: {
        for (int i = 0; i < e->nchildren; i++) {
            bb_exec_stmt(e->children[i]);
            if (FRAME.returning || FRAME.loop_break || FRAME.loop_next ||
                FRAME.suspending) break;
        }
        return;
    }

    /*========================================================================
     * RS-23a-route: high-volume expression kinds in statement context.
     * Contract: evaluate for side effects, discard the result.
     * bb_eval_value already handles AST_FNC (including raku_try_call_builtin
     * at the top, via RS-23a-raku), AST_ASSIGN, and AST_AUGOP natively.
     *======================================================================*/
    case AST_FNC:
    case AST_ASSIGN:
    case AST_AUGOP: {
        (void)bb_eval_value(e);
        return;
    }

    /*========================================================================
     * RS-23b: expression kinds that arrive in statement context from
     * Icon/Prolog procedure bodies (caller=coro_call) and recursive
     * statement-context evaluation (caller=bb_exec_stmt).
     *
     *   AST_ILIT, AST_NUL — pure literals, zero side effects → no-op.
     *   AST_NOT         — evaluates child for side effects, discards boolean.
     *   AST_ALTERNATE   — generator combinator; first-success branch runs its
     *                   side effects via bb_eval_value, result discarded.
     *   AST_SCAN        — `subj ? body`; bb_eval_value handles scan-stack
     *                   push/pop and body evaluation.
     *   AST_CASE        — bb_eval_value evaluates topic, dispatches, runs
     *                   matching body for side effects.
     *
     * bb_eval_value already handles all five non-trivial kinds (AST_NOT,
     * AST_ALTERNATE, AST_SCAN, AST_CASE, plus AST_ILIT/AST_NUL via eval_node).
     *======================================================================*/
    case AST_ILIT:
    case AST_NUL:
        return;

    case AST_NOT:
    case AST_ALTERNATE:
    case AST_SCAN:
    case AST_CASE: {
        (void)bb_eval_value(e);
        return;
    }

    /*========================================================================
     * RS-23c: AST_EVERY, AST_INITIAL, AST_SWAP — missing from both adapters.
     * Statement context: evaluate for side effects, discard result.
     * bb_eval_value carries the full native implementation for all three
     * kinds (added in this rung to coro_value.c).
     *======================================================================*/
    case AST_EVERY:
    case AST_INITIAL:
    case AST_SWAP: {
        (void)bb_eval_value(e);
        return;
    }

    /*========================================================================
     * RS-23-extra (session 2026-05-05): AST_REVASSIGN stmt-context.
     * Statement context: perform the reversible assign and discard the
     * returned value.  bb_eval_value carries the full native implementation
     * (added in this rung to coro_value.c).  The revert semantics for
     * every/alt-driven contexts are unaffected — those reach coro_bb_revassign
     * via coro_eval, not via this path.
     *======================================================================*/
    case AST_REVASSIGN: {
        (void)bb_eval_value(e);
        return;
    }

    default: break;
    }

    /* RS-23e (closes the RS-23 arc, session 2026-05-05).  Diag verified
     * zero `interp_eval` calls reach this fallthrough from any BB-adapter
     * ancestor across smoke + unified_broker + full Icon corpus 263.
     * Every kind that exercises the test surface is now handled by an
     * explicit case above (or delegates to bb_eval_value).  Anything
     * reaching this point is a four-mode isolation violation: a kind
     * arrived in BB-adapter stmt context without a native handler.
     * Abort with a diagnostic naming the kind. */
    fprintf(stderr,
            "FATAL bb_exec_stmt: unhandled kind %d (RS-23e isolation breach)\n",
            (int)e->kind);
    abort();
}
