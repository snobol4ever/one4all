/*============================================================================================================================
 * bb_broker.c — Unified Byrd Box Broker (GOAL-UNIFIED-BROKER U-3)
 *
 * One broker for all five languages. Three drive modes:
 *
 *   BB_SCAN (SNOBOL4): scan cursor positions Δ=0..Ω; call root.fn(ζ,α) at each position;
 *                      on γ: call body_fn(val, arg) and stop (first match wins).
 *                      Mirrors Phase 3 of stmt_exec.c exactly.
 *
 *   BB_PUMP (Icon):    call root.fn(ζ,α); on γ: body_fn(val,arg); β-loop until ω.
 *
 *   BB_ONCE (Prolog):  call root.fn(ζ,α) once; on γ: body_fn(val,arg) once; done.
 *                      OR-box handles retry internally. pl_exec_goal removed U-11.
 *
 * Value type: DESCR_t throughout.
 * During migration (before U-5), SNOBOL4 callers use descr_from_spec() to wrap spec_t
 * results before passing boxes to bb_broker. After U-5 all boxes return DESCR_t natively.
 *
 * Returns: number of values/matches produced (tick count). 0 = failure (ω on first call).
 *============================================================================================================================*/

#include "bb_broker.h"

/*============================================================================================================================
 * bb_broker — unified entry point
 *============================================================================================================================*/
int bb_broker(bb_node_t root, BrokerMode mode,
              void (*body_fn)(DESCR_t val, void *arg), void *arg) {
    if (!root.fn) return 0;

    univ_box_fn fn = (univ_box_fn)root.fn;   /* cast: safe once U-5 done; during migration caller ensures DESCR_t box */
    int ticks = 0;

    switch (mode) {

    /*--------------------------------------------------------------------------------------------------------------------------
     * BB_SCAN — SNOBOL4 pattern match: try each cursor position 0..Ω
     * Mirrors exec_stmt Phase 3. kw_anchor and NAM save/restore are caller's responsibility
     * (stmt_exec.c handles those in its own Phase 3 wrapper until U-9 wires this in fully).
     *--------------------------------------------------------------------------------------------------------------------------*/
    case BB_SCAN: {
        for (int scan = 0; scan <= Ω; scan++) {
            Δ = scan;
            DESCR_t val = fn(root.ζ, α);
            if (!IS_FAIL_fn(val)) {
                if (body_fn) body_fn(val, arg);
                ticks++;
                return ticks;   /* first match wins — SNOBOL4 semantics */
            }
        }
        return 0;
    }

    /*--------------------------------------------------------------------------------------------------------------------------
     * BB_PUMP — Icon generator: pump all values until ω
     * body_fn called once per value.
     *--------------------------------------------------------------------------------------------------------------------------*/
    case BB_PUMP: {
        DESCR_t val = fn(root.ζ, α);
        if (IS_FAIL_fn(val)) return 0;
        if (body_fn) body_fn(val, arg);
        ticks++;
        for (;;) {
            val = fn(root.ζ, β);
            if (IS_FAIL_fn(val)) break;
            if (body_fn) body_fn(val, arg);
            ticks++;
        }
        return ticks;
    }

    /*--------------------------------------------------------------------------------------------------------------------------
     * BB_ONCE — Prolog goal: α once, OR-box handles retry internally.
     * U-11: pl_exec_goal removed; callers use bb_broker(root, BB_ONCE, NULL, NULL) directly.
     * body_fn called at most once.
     *--------------------------------------------------------------------------------------------------------------------------*/
    case BB_ONCE: {
        DESCR_t val = fn(root.ζ, α);
        if (IS_FAIL_fn(val)) return 0;
        if (body_fn) body_fn(val, arg);
        return 1;
    }

    } /* switch */

    return 0;   /* unreachable */
}
