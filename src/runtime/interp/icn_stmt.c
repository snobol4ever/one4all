#include "icn_stmt.h"
#include "icn_value.h"
#include "icn_runtime.h"
#include "snobol4.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_exec_stmt(tree_t *e)
{
    if (!e) return;
    switch (e->t) {
    case TT_LOOP_NEXT: {
        FRAME.loop_next = 1;
        return;
    }
    case TT_LOOP_BREAK: {
        FRAME.loop_break = 1;
        return;
    }
    case TT_PROC_FAIL: {
        FRAME.returning  = 1;
        FRAME.return_val = FAILDESCR;
        return;
    }
    case TT_RETURN: {
        DESCR_t rv = (e->n > 0) ? bb_eval_value(e->c[0]) : NULVCL;
        FRAME.returning  = 1;
        FRAME.return_val = rv;
        return;
    }
    case TT_SUSPEND: {
        DESCR_t val = (e->n > 0) ? bb_eval_value(e->c[0]) : NULVCL;
        if (!IS_FAIL_fn(val)) {
            FRAME.suspending  = 1;
            FRAME.suspend_val = val;
            FRAME.suspend_do  = (e->n > 1) ? e->c[1] : NULL;
        }
        return;
    }
    case TT_IF: {
        if (e->n < 1) return;
        tree_t *test = e->c[0];
        if (is_suspendable(test)) {
            bb_node_t box = icn_bb_build(test);
            DESCR_t v = box.fn(box.ζ, α);
            if (!IS_FAIL_fn(v) && !FRAME.returning && !FRAME.loop_break) {
                if (e->n > 1) bb_exec_stmt(e->c[1]);
            } else {
                if (e->n > 2) bb_exec_stmt(e->c[2]);
            }
            return;
        }
        DESCR_t cv = bb_eval_value(test);
        if (!IS_FAIL_fn(cv)) {
            if (e->n > 1) bb_exec_stmt(e->c[1]);
        } else {
            if (e->n > 2) bb_exec_stmt(e->c[2]);
        }
        return;
    }
    case TT_WHILE: {
        int saved_brk = FRAME.loop_break; FRAME.loop_break = 0;
        int saved_nxt = FRAME.loop_next;  FRAME.loop_next  = 0;
        while (!FRAME.returning && !FRAME.loop_break && !FRAME.suspending) {
            DESCR_t cv = (e->n > 0) ? bb_eval_value(e->c[0]) : FAILDESCR;
            if (IS_FAIL_fn(cv)) break;
            FRAME.loop_next = 0;
            if (e->n > 1) bb_exec_stmt(e->c[1]);
            if (FRAME.suspending) break;
        }
        FRAME.loop_break = saved_brk;
        FRAME.loop_next  = saved_nxt;
        return;
    }
    case TT_UNTIL: {
        int saved_brk = FRAME.loop_break; FRAME.loop_break = 0;
        int saved_nxt = FRAME.loop_next;  FRAME.loop_next  = 0;
        while (!FRAME.returning && !FRAME.loop_break && !FRAME.suspending) {
            DESCR_t cv = (e->n > 0) ? bb_eval_value(e->c[0]) : FAILDESCR;
            if (!IS_FAIL_fn(cv)) break;
            FRAME.loop_next = 0;
            if (e->n > 1) bb_exec_stmt(e->c[1]);
            if (FRAME.suspending) break;
        }
        FRAME.loop_break = saved_brk;
        FRAME.loop_next  = saved_nxt;
        return;
    }
    case TT_REPEAT: {
        int saved_brk = FRAME.loop_break; FRAME.loop_break = 0;
        int saved_nxt = FRAME.loop_next;  FRAME.loop_next  = 0;
        while (!FRAME.returning && !FRAME.loop_break && !FRAME.suspending) {
            FRAME.loop_next = 0;
            if (e->n > 0) {
                bb_exec_stmt(e->c[0]);
                if (FRAME.suspending) break;
            }
        }
        FRAME.loop_break = saved_brk;
        FRAME.loop_next  = saved_nxt;
        return;
    }
    case TT_SEQ: {
        for (int i = 0; i < e->n; i++) {
            DESCR_t r = bb_eval_value(e->c[i]);
            if (IS_FAIL_fn(r)) return;
            if (FRAME.returning || FRAME.loop_break || FRAME.loop_next ||
                FRAME.suspending) return;
        }
        return;
    }
    case TT_SEQ_EXPR: {
        for (int i = 0; i < e->n; i++) {
            bb_exec_stmt(e->c[i]);
            if (FRAME.returning || FRAME.loop_break || FRAME.loop_next ||
                FRAME.suspending) break;
        }
        return;
    }
    case TT_FNC:
    case TT_ASSIGN:
    case TT_AUGOP: {
        (void)bb_eval_value(e);
        return;
    }
    case TT_ILIT:
    case TT_NUL:
        return;
    case TT_NOT:
    case TT_ALTERNATE:
    case TT_SCAN:
    case TT_CASE: {
        (void)bb_eval_value(e);
        return;
    }
    case TT_EVERY:
    case TT_INITIAL:
    case TT_SWAP: {
        (void)bb_eval_value(e);
        return;
    }
    case TT_REVASSIGN: {
        (void)bb_eval_value(e);
        return;
    }
    default: {
        (void)bb_eval_value(e);
        return;
    }
    }
}
