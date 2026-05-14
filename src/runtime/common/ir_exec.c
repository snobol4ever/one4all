/*
 * ir_exec.c — DCG graph-walk executor: ir_exec_once, ir_exec_pump (LR-2)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (LR-2, 2026-05-14)
 */
#include "ir_exec.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
/*------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_exec_eval_node — evaluate nd in its current state; return next port.
 * Self-evaluating scalar kinds set nd->value and return port_succ or port_fail.
 * Generative kinds consult nd->state / nd->counter and update them.
 * Unimplemented kinds return port_fail (explicit, safe, detectable in tests). */
ir_node_t * ir_exec_eval_node(ir_node_t * nd) {
    switch (nd->kind) {
    /*-- Literals: always succeed, value is the literal. ----------------------------------------------------------------*/
    case IR_LIT_I:
        nd->value = INTVAL(nd->ival);
        return nd->port_succ;
    case IR_LIT_F:
        nd->value = REALVAL(nd->dval);
        return nd->port_succ;
    case IR_LIT_S:
        nd->value = STRVAL(nd->sval ? nd->sval : "");
        return nd->port_succ;
    case IR_LIT_NUL:
    case IR_SUCCEED:
        nd->value = NULVCL;
        return nd->port_succ;
    /*-- FAIL: always fails. --------------------------------------------------------------------------------------------*/
    case IR_FAIL:
        nd->value = FAILDESCR;
        return nd->port_fail;
    /*-- TO_BY: integer range generator.
     * state 0 = fresh (init from children c[0]=from, c[1]=to, c[2]=by or NULL).
     * state 1 = running; nd->counter = current value; nd->value = INTVAL(counter).
     * state 2 = exhausted → port_fail. -------------------------------------------------------------------------------*/
    case IR_TO_BY: {
        if (nd->state == 0) {
            /* evaluate children to get from/to/by -- do not rely on pre-set values */
            int64_t from = 0, by = 1;
            if (nd->n > 0 && nd->c[0]) { ir_exec_eval_node(nd->c[0]); from = nd->c[0]->value.i; }
            if (nd->n > 2 && nd->c[2]) { ir_exec_eval_node(nd->c[2]); by   = nd->c[2]->value.i; }
            if (by == 0) by = 1;
            nd->counter = from;
            nd->ival    = by;   /* reuse ival for step */
            nd->state   = 1;
        }
        if (nd->state == 2) {
            nd->value = FAILDESCR;
            return nd->port_fail;
        }
        /* evaluate to-child (may be dynamic) */
        int64_t to_val = 0;
        if (nd->n > 1 && nd->c[1]) { ir_exec_eval_node(nd->c[1]); to_val = nd->c[1]->value.i; }
        int64_t by = nd->ival;
        if (by >= 0 ? nd->counter > to_val : nd->counter < to_val) {
            nd->state = 2;
            nd->value = FAILDESCR;
            return nd->port_fail;
        }
        nd->value    = INTVAL(nd->counter);
        nd->counter += by;
        return nd->port_succ;
    }
    /*-- ALTERNATE(A,B): try A; on A-fail try B. Wired by lower:
     * port_start→A.start, A.succ→self.port_succ, A.fail→B.start,
     * B.succ→self.port_succ, B.fail→self.port_fail.
     * ir_exec_eval_node is not called for ALTERNATE in the walker —
     * the port wiring handles routing.  But if somehow called, route to fail. */
    case IR_ALTERNATE:
        nd->value = FAILDESCR;
        return nd->port_fail;
    /*-- All other kinds: not yet implemented — return port_fail explicitly. ----------------------------------------*/
    default:
        nd->value = FAILDESCR;
        return nd->port_fail;
    }
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_exec_once — drive cfg from entry to first succ or fail.
 * Graph walker: pointer-chase from entry through port_succ/port_fail.
 * Back-edges (cycles) are traversed by the pointer chain; exhaustion is
 * when a node routes to port_fail with no further resume path. */
DESCR_t ir_exec_once(ir_graph_t * cfg) {
    if (!cfg || !cfg->entry) return FAILDESCR;
    ir_graph_reset(cfg);
    ir_node_t * cur = cfg->entry;
    int safety = cfg->n * 64 + 256;   /* cycle-breaker: max steps before abort */
    while (cur && safety-- > 0) {
        ir_node_t * next = ir_exec_eval_node(cur);
        if (!next) {
            /* terminal: succ path returned NULL → value is in cur->value */
            return IS_FAIL_fn(cur->value) ? FAILDESCR : cur->value;
        }
        /* If next == cur we have an infinite self-loop on a non-generative node.
         * Treat as fail to avoid spinning forever. */
        if (next == cur) return FAILDESCR;
        cur = next;
    }
    return FAILDESCR;
}
/*------------------------------------------------------------------------------------------------------------------------------------*/
/* ir_exec_pump — drive cfg to exhaustion, calling body_fn per value.
 * After ir_exec_once returns a value, resume by following port_resume of the
 * deepest node that has one.  Implementation: simple retry loop using
 * ir_exec_once_resume which starts from port_resume of the last-succ node. */
int ir_exec_pump(ir_graph_t * cfg, ir_exec_body_fn body_fn, void * ctx) {
    if (!cfg || !cfg->entry) return 0;
    ir_graph_reset(cfg);
    int ticks  = 0;
    int safety = cfg->n * 256 + 1024;
    ir_node_t * cur = cfg->entry;
    while (cur && safety-- > 0) {
        ir_node_t * next = ir_exec_eval_node(cur);
        if (!next) {
            /* terminal node: check value */
            if (!IS_FAIL_fn(cur->value)) {
                ticks++;
                if (body_fn && body_fn(cur->value, ctx)) break;
                /* resume from this node's port_resume — do NOT reset state */
                next = cur->port_resume;
                if (!next) break;
            } else {
                break;
            }
        } else if (next == cur) {
            /* self-loop on a generative node: eval again without resetting */
            continue;
        }
        cur = next;
    }
    return ticks;
}
