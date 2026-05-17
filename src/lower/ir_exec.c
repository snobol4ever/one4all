#include "ir_exec.h"
#include "../../frontend/prolog/term.h"
#include "../../frontend/prolog/prolog_runtime.h"
#include "../../frontend/prolog/prolog_atom.h"
#include "../../runtime/interp/pl_runtime.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <gc/gc.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void icn_every_body_pre(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int  icn_every_body_broke(void);
extern const char *Σ;
extern int         Δ;
extern int         Ω;
extern int         Σlen;
#include "snobol4.h"
#include "lower_icn.h"
#include "sm_interp.h"
#include "../runtime/interp/icn_runtime.h"
#include "coerce.h"
extern int icn_try_call_builtin_by_name(const char *fn, DESCR_t *args, int nargs, DESCR_t *out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t *data_field_ptr(const char *field, DESCR_t obj);
typedef struct { char name[64]; int nfields; char fields[64][64]; } ScDatType;
extern ScDatType *sc_dat_register(const char *spec);
extern ScDatType *sc_dat_find_type(const char *name);
extern DESCR_t    sc_dat_construct(ScDatType *t, DESCR_t *args, int nargs);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern void bb_exec_stmt(void *e);
#include "bb_box.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_binop_apply(IcnBinopKind op, DESCR_t lv, DESCR_t rv, int *rel_fail);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static DESCR_t g_ir_return_val;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t IR_exec_once(IR_block_t * cfg);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Recursive classifier: returns 1 iff e is structurally guaranteed to yield exactly one value (no generator suspension). */
/* Generator IR kinds (IR_ICN_*, IR_BINOP_GEN, IR_ALT, IR_SUSPEND, IR_REPEAT, IR_TO_BY, IR_ALTERNATE, IR_LIMIT) → 0.   */
/* IR_CALL: user proc with is_generator==0 AND ir_body set AND all args single-shot → 1; known generator builtins → 0;  */
/* other builtins → single-shot iff all args single-shot.                                                                */
static int ir_is_single_shot(IR_t * e) {
    if (!e) return 1;
    switch (e->t) {
    case IR_ICN_TO: case IR_ICN_TO_BY: case IR_ICN_UPTO: case IR_ICN_ITERATE:
    case IR_ICN_ALTERNATE: case IR_ICN_LIMIT: case IR_ICN_BINOP: case IR_ICN_TO_NESTED:
    case IR_ICN_PROC_GEN: case IR_BINOP_GEN: case IR_ALT: case IR_ALTERNATE:
    case IR_SUSPEND: case IR_REPEAT: case IR_TO_BY: case IR_LIMIT: case IR_ICN_SCAN:
    case IR_ICN_LIST_BANG: case IR_ICN_KEY_GEN:
        return 0;
    case IR_CALL: {
        if (!e->sval) return 1;
        for (int _pi = 0; _pi < proc_count; _pi++) {
            if (!proc_table[_pi].name || strcmp(proc_table[_pi].name, e->sval) != 0) continue;
            if (!proc_table[_pi].ir_body) return 0;
            if (proc_table[_pi].is_generator) return 0;
            for (int _j = 0; _j < e->n; _j++) if (!ir_is_single_shot(e->c[_j])) return 0;
            return 1;
        }
        if (!strcmp(e->sval, "find") || !strcmp(e->sval, "upto") || !strcmp(e->sval, "any")
            || !strcmp(e->sval, "many") || !strcmp(e->sval, "bal") || !strcmp(e->sval, "key")
            || !strcmp(e->sval, "seq")) return 0;
        for (int _j = 0; _j < e->n; _j++) if (!ir_is_single_shot(e->c[_j])) return 0;
        return 1;
    }
    default: {
        for (int _j = 0; _j < e->n; _j++) if (e->c[_j] && !ir_is_single_shot(e->c[_j])) return 0;
        return 1;
    }
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_t * IR_exec_node(IR_t * nd) {
    switch (nd->t) {
    case IR_LIT_I:
        nd->value = INTVAL(nd->ival);
        return nd->γ;
    case IR_VAR: {
        /* Icon variable read. nd->sval = name. Resolve frame slot at exec time via FRAME.sc.        */
        if (frame_depth > 0 && nd->sval) {
            int slot = scope_get(&FRAME.sc, nd->sval);
            if (slot >= 0 && slot < FRAME.env_n) {
                DESCR_t sv = FRAME.env[slot];
                if (sv.v != 0) { nd->value = sv; return nd->γ; }
            }
        }
        if (nd->sval) {
            DESCR_t gv = NV_GET_fn(nd->sval);
            nd->value = gv;
            return IS_FAIL_fn(gv) ? nd->ω : nd->γ;
        }
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_ASSIGN: {
        /* c[0] = LHS (IR_VAR), c[1] = RHS. Evaluate RHS, store into LHS slot (resolved by name).   */
        if (nd->n < 2 || !nd->c[0] || !nd->c[1]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[1]);
        DESCR_t val = nd->c[1]->value;
        if (IS_FAIL_fn(val)) { nd->value = FAILDESCR; return nd->ω; }
        IR_t *lhs = nd->c[0];
        if (lhs->t == IR_VAR && lhs->sval) {
            if (frame_depth > 0) {
                int slot = scope_get(&FRAME.sc, lhs->sval);
                if (slot >= 0 && slot < FRAME.env_n) {
                    FRAME.env[slot] = val;
                    nd->value = val;
                    return nd->γ;
                }
            }
            NV_SET_fn(lhs->sval, val);
            nd->value = val;
            return nd->γ;
        }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_CALL: {
        /* Builtin call by name. nd->sval = function name; nd->c[0..n-1] = arg subexprs.            */
        if (!nd->sval) { nd->value = FAILDESCR; return nd->ω; }
        /* IJ-SUSPEND-IR-CALL-GEN: if this node is mid-pump of a generator proc, advance it.        */
        /* nd->opaque holds the GeneratorState; nd->state==1 means in-flight.                       */
        /* Args are not re-evaluated on resume — they were captured into the GeneratorState frame.  */
        if (nd->state == 1 && nd->opaque) {
            GeneratorState *gs = (GeneratorState *)nd->opaque;
            DESCR_t v;
            int ok = bb_broker_drive_sm_one(gs, &v);
            if (!ok) { nd->state = 0; nd->opaque = NULL; nd->value = FAILDESCR; return nd->ω; }
            nd->value = v;
            return nd->γ;
        }
        int nargs = nd->n;
        DESCR_t *args = NULL;
        if (nargs > 0) {
            args = (DESCR_t *)GC_malloc((size_t)nargs * sizeof(DESCR_t));
            for (int j = 0; j < nargs; j++) {
                if (!nd->c[j]) { nd->value = FAILDESCR; return nd->ω; }
                IR_exec_node(nd->c[j]);
                args[j] = nd->c[j]->value;
                if (IS_FAIL_fn(args[j])) { nd->value = FAILDESCR; return nd->ω; }
            }
        }
        /* IJ-SUSPEND-IR-CALL-GEN: check user-defined generator procs BEFORE builtins.              */
        /* The "upto" builtin's scan-context guard (`scan_pos > 0 || nargs >= 2`) was true by       */
        /* default (polyglot.c initializes scan_pos=1), so user-defined upto was shadowed.          */
        /* User generator procs take priority — they correspond to source-level `procedure upto`.   */
        for (int _pi0 = 0; _pi0 < proc_count; _pi0++) {
            if (!proc_table[_pi0].name || strcmp(proc_table[_pi0].name, nd->sval) != 0) continue;
            if (!proc_table[_pi0].is_generator) break;  /* fall through to builtins/standard path */
            if (proc_table[_pi0].ir_body) break;        /* let ir_body path handle it (standard) */
            if (proc_table[_pi0].entry_pc < 0 || !g_current_sm_prog) break;
            GeneratorState *pgs = generator_state_new_proc(_pi0, args, nargs);
            if (!pgs) break;
            DESCR_t v;
            int ok = bb_broker_drive_sm_one(pgs, &v);
            if (!ok) { nd->value = FAILDESCR; return nd->ω; }
            nd->opaque = pgs;
            nd->state  = 1;
            nd->value  = v;
            return nd->γ;
        }
        DESCR_t out = FAILDESCR;
        if (icn_try_call_builtin_by_name(nd->sval, args, nargs, &out)) {
            nd->value = out;
            return IS_FAIL_fn(out) ? nd->ω : nd->γ;
        }
        /* User-defined proc: look up proc_table by name; if ir_body exists, push frame and exec. Snapshot per-node state of the callee's ir_body around IR_exec_once so the caller's activation survives when the callee is the SAME proc (recursion shares the IR graph; without snapshot, the inner IR_reset wipes the caller's per-node value/counter/state, breaking e.g. IR_BINOP_GEN's read of nd->c[0]->value after a recursive-call right operand). */
        for (int _pi = 0; _pi < proc_count; _pi++) {
            if (!proc_table[_pi].name || strcmp(proc_table[_pi].name, nd->sval) != 0) continue;
            if (!proc_table[_pi].ir_body) break;
            if (frame_depth >= FRAME_STACK_MAX) break;
            IcnFrame *_f = &frame_stack[frame_depth++];
            memset(_f, 0, sizeof *_f);
            _f->sc   = proc_table[_pi].lower_sc;
            int _nsl = _f->sc.n > 0 ? _f->sc.n : 1;
            if (_nsl > FRAME_SLOT_MAX) _nsl = FRAME_SLOT_MAX;
            _f->env_n = _nsl;
            for (int _k = 0; _k < proc_table[_pi].nparams && _k < nargs && _k < FRAME_SLOT_MAX; _k++)
                _f->env[_k] = args[_k];
            IR_node_state_t * _snap = IR_snapshot_state(proc_table[_pi].ir_body);
            IR_reset(proc_table[_pi].ir_body);
            out = IR_exec_once(proc_table[_pi].ir_body);
            if (frame_depth > 0 && FRAME.returning) { out = g_ir_return_val; FRAME.returning = 0; }
            frame_depth--;
            IR_restore_state(proc_table[_pi].ir_body, _snap);
            nd->value = out;
            return IS_FAIL_fn(out) ? nd->ω : nd->γ;
        }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_SEQ: {
        /* Sequence: evaluate each child for side effects. Body fails (FAILDESCR) so bb_broker exits */
        /* after a single pump tick — matches Icon procedure-falls-off-end semantics.                */
        for (int j = 0; j < nd->n; j++) {
            if (!nd->c[j]) continue;
            IR_exec_node(nd->c[j]);
            if (frame_depth > 0 && FRAME.returning) break;
        }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_BINOP: {
        /* Plain (non-generator) binop.  c[0]=lhs, c[1]=rhs; nd->ival=IcnBinopKind, nd->ival2=is_relop. */
        if (nd->n < 2 || !nd->c[0] || !nd->c[1]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t lv = nd->c[0]->value;
        if (IS_FAIL_fn(lv)) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[1]);
        DESCR_t rv = nd->c[1]->value;
        if (IS_FAIL_fn(rv)) { nd->value = FAILDESCR; return nd->ω; }
        int rel_fail = 0;
        DESCR_t result = icn_binop_apply((IcnBinopKind)nd->ival, lv, rv, &rel_fail);
        if (IS_FAIL_fn(result)) { nd->value = FAILDESCR; return nd->ω; }
        nd->value = result;
        return nd->γ;
    }
    case IR_BINOP_GEN: {
        /* Generator-aware binop. Yields cross-product when c[0] or c[1] is a generator.            */
        /* If only one operand is a generator (e.g. const > (1 to 3)), single-shot the const side.  */
        /* state==0: fresh — seed both sides. state==1: advance.                                    */
        if (nd->n < 2 || !nd->c[0] || !nd->c[1]) { nd->value = FAILDESCR; return nd->ω; }
        /* Inline gen-kind check — must match the set of IR kinds that act as restartable generators. */
        #define IR_IS_GEN_KIND(k) ( \
            (k) == IR_ICN_TO || (k) == IR_ICN_TO_BY || (k) == IR_ICN_UPTO || \
            (k) == IR_ALT || (k) == IR_ALTERNATE || (k) == IR_BINOP_GEN || \
            (k) == IR_ICN_ITERATE || (k) == IR_ICN_LIMIT || (k) == IR_ICN_PROC_GEN || \
            (k) == IR_ICN_LIST_BANG || (k) == IR_ICN_KEY_GEN || (k) == IR_TO_BY)
        int l_gen = IR_IS_GEN_KIND(nd->c[0]->t);
        int r_gen = IR_IS_GEN_KIND(nd->c[1]->t);
        if (nd->state == 0) {
            nd->c[0]->state = 0;
            nd->c[1]->state = 0;
            IR_exec_node(nd->c[0]);
            if (IS_FAIL_fn(nd->c[0]->value)) { nd->value = FAILDESCR; return nd->ω; }
            IR_exec_node(nd->c[1]);
            if (IS_FAIL_fn(nd->c[1]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
            nd->state = 1;
        } else {
            /* β: advance — try inner generator first. If only one side is a generator, the other is */
            /* single-shot and we exhaust after one pair (relop-driven exception handled in loop).   */
            /* When the non-generator side is a pure variable read (IR_VAR / IR_ICN_KEYWORD), re-eval */
            /* it on each pump: this is what makes `every total := total + (1 to 5)` accumulate as a */
            /* user would expect (matches augop +:= semantics).  Side-effecting nodes (IR_CALL etc.)  */
            /* are NOT re-evaluated — only cheap pure reads.                                          */
            if (r_gen) {
                IR_exec_node(nd->c[1]);
                if (IS_FAIL_fn(nd->c[1]->value)) {
                    if (!l_gen) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                    IR_exec_node(nd->c[0]);
                    if (IS_FAIL_fn(nd->c[0]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                    nd->c[1]->state = 0;
                    IR_exec_node(nd->c[1]);
                    if (IS_FAIL_fn(nd->c[1]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                }
                if (!l_gen && (nd->c[0]->t == IR_VAR || nd->c[0]->t == IR_ICN_KEYWORD)) {
                    IR_exec_node(nd->c[0]);
                    if (IS_FAIL_fn(nd->c[0]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                }
            } else if (l_gen) {
                /* Only left is a generator: advance left, keep right value (right is single-shot). */
                IR_exec_node(nd->c[0]);
                if (IS_FAIL_fn(nd->c[0]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                if (nd->c[1]->t == IR_VAR || nd->c[1]->t == IR_ICN_KEYWORD) {
                    IR_exec_node(nd->c[1]);
                    if (IS_FAIL_fn(nd->c[1]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                }
            } else {
                /* Neither side a generator — single-shot, already returned the pair, exhaust now. */
                nd->state = 0; nd->value = FAILDESCR; return nd->ω;
            }
        }
        /* Now have a (lv, rv) pair. Apply the op; on rel_fail, advance and retry. */
        for (;;) {
            int rel_fail = 0;
            DESCR_t result = icn_binop_apply((IcnBinopKind)nd->ival, nd->c[0]->value, nd->c[1]->value, &rel_fail);
            if (!IS_FAIL_fn(result)) { nd->value = result; return nd->γ; }
            if (!rel_fail) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
            /* relop failed: try next pair (advance generator side(s)). */
            if (r_gen) {
                IR_exec_node(nd->c[1]);
                if (IS_FAIL_fn(nd->c[1]->value)) {
                    if (!l_gen) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                    IR_exec_node(nd->c[0]);
                    if (IS_FAIL_fn(nd->c[0]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                    nd->c[1]->state = 0;
                    IR_exec_node(nd->c[1]);
                    if (IS_FAIL_fn(nd->c[1]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                }
            } else if (l_gen) {
                IR_exec_node(nd->c[0]);
                if (IS_FAIL_fn(nd->c[0]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
            } else {
                /* Neither side a generator, relop failed → exhaust. */
                nd->state = 0; nd->value = FAILDESCR; return nd->ω;
            }
        }
        #undef IR_IS_GEN_KIND
    }
    case IR_LIT_F:
        nd->value = REALVAL(nd->dval);
        return nd->γ;
    case IR_LIT_S:
        nd->value = STRVAL(nd->sval ? nd->sval : "");
        return nd->γ;
    case IR_LIT_NUL:
    case IR_SUCCEED:
        nd->value = NULVCL;
        return nd->γ;
    case IR_RETURN: {
        /* Icon return [E]. Set return value; signal early exit via FRAME.returning.                 */
        DESCR_t rv = NULVCL;
        if (nd->n >= 1 && nd->c[0]) { IR_exec_node(nd->c[0]); rv = nd->c[0]->value; }
        g_ir_return_val = IS_FAIL_fn(rv) ? NULVCL : rv;
        if (frame_depth > 0) FRAME.returning = 1;
        nd->value = g_ir_return_val;
        return nd->ω;
    }
    case IR_FAIL:
        nd->value = FAILDESCR;
        return nd->ω;
    case IR_IF: {
        /* Icon if/then/else.  c[0]=cond, c[1]=then, c[2]=else (optional).                                                                                                                                  */
        /* Cond succeeds if it produces any non-FAIL value.  Then-branch evaluated on success; else-branch on failure.                                                                                      */
        if (nd->n < 1 || !nd->c[0]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t cv = nd->c[0]->value;
        if (!IS_FAIL_fn(cv)) {
            if (nd->n >= 2 && nd->c[1]) { IR_exec_node(nd->c[1]); nd->value = nd->c[1]->value; }
            else nd->value = NULVCL;
            return nd->γ;
        }
        if (nd->n >= 3 && nd->c[2]) {
            IR_exec_node(nd->c[2]);
            nd->value = nd->c[2]->value;
            return nd->γ;
        }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_EVERY: {
        /* Icon every E [do B].  c[0]=generator expr (mandatory), c[1]=optional body.                                                                                                                       */
        /* Statement consumer: drive c[0] to exhaustion within ONE outer IR_exec_node call.  c[0]'s state (counter, etc.) persists across the inner IR_exec_node calls because IR_reset is not invoked.   */
        /* every-loop in statement context always "succeeds" with &null after exhaustion — never propagates the generator's final FAIL upward.                                                              */
        /* Single-shot fast path: when c[0] is a structurally non-generator expression (recursive walk: generator IR kinds disqualify; IR_CALL to user proc with is_generator==0; IR_CALL to non-gen      */
        /* builtin with all-single-shot args), fire once.  Covers every fact(5) and every write(fact(5)) without naive TT_FNC blanket.  Generator IR kinds bail out (IR_ICN_*, IR_BINOP_GEN, IR_ALT,...).  */
        if (nd->n < 1 || !nd->c[0]) { nd->value = NULVCL; return nd->γ; }
        int single_shot_call = ir_is_single_shot(nd->c[0]);
        int saved_brk = frame_depth > 0 ? FRAME.loop_break : 0;
        int saved_nxt = frame_depth > 0 ? FRAME.loop_next  : 0;
        if (frame_depth > 0) { FRAME.loop_break = 0; FRAME.loop_next = 0; }
        int safety = 1000000;
        while (safety-- > 0) {
            IR_exec_node(nd->c[0]);
            DESCR_t v = nd->c[0]->value;
            if (IS_FAIL_fn(v)) break;
            if (frame_depth > 0 && FRAME.loop_break) break;
            if (nd->n >= 2 && nd->c[1]) { IR_exec_node(nd->c[1]); }
            if (frame_depth > 0 && (FRAME.loop_break || FRAME.returning)) break;
            if (frame_depth > 0) FRAME.loop_next = 0;
            if (single_shot_call) break;
        }
        if (frame_depth > 0) { FRAME.loop_break = saved_brk; FRAME.loop_next = saved_nxt; }
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_WHILE: {
        /* Icon while C [do B].  c[0]=cond (mandatory), c[1]=optional body.                                                                                                                                  */
        /* Loop: eval cond; if cond fails, break; else eval body; repeat.  Cond's state is reset before each pump via IR_exec_node — for plain scalar conds (BINOP relops) this is correct; for generator   */
        /* conds, the resume-on-next-iteration is the right semantics for while (cond drives one pump per iteration).  Always succeeds with NULVCL after termination.                                       */
        if (nd->n < 1 || !nd->c[0]) { nd->value = NULVCL; return nd->γ; }
        int saved_brk_w = frame_depth > 0 ? FRAME.loop_break : 0;
        int saved_nxt_w = frame_depth > 0 ? FRAME.loop_next  : 0;
        if (frame_depth > 0) { FRAME.loop_break = 0; FRAME.loop_next = 0; }
        int safety = 1000000;
        while (safety-- > 0) {
            nd->c[0]->state = 0;
            IR_exec_node(nd->c[0]);
            DESCR_t cv = nd->c[0]->value;
            if (IS_FAIL_fn(cv)) break;
            if (frame_depth > 0 && FRAME.loop_break) break;
            if (nd->n >= 2 && nd->c[1]) { IR_exec_node(nd->c[1]); }
            if (frame_depth > 0 && (FRAME.loop_break || FRAME.returning)) break;
            if (frame_depth > 0) FRAME.loop_next = 0;
        }
        if (frame_depth > 0) { FRAME.loop_break = saved_brk_w; FRAME.loop_next = saved_nxt_w; }
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_UNTIL: {
        /* Icon until C [do B].  c[0]=cond (mandatory), c[1]=optional body.  Inverse of WHILE: loop while cond FAILS, exit when cond succeeds.                                                                */
        if (nd->n < 1 || !nd->c[0]) { nd->value = NULVCL; return nd->γ; }
        int saved_brk_u = frame_depth > 0 ? FRAME.loop_break : 0;
        int saved_nxt_u = frame_depth > 0 ? FRAME.loop_next  : 0;
        if (frame_depth > 0) { FRAME.loop_break = 0; FRAME.loop_next = 0; }
        int safety_u = 1000000;
        while (safety_u-- > 0) {
            nd->c[0]->state = 0;
            IR_exec_node(nd->c[0]);
            DESCR_t cv = nd->c[0]->value;
            if (!IS_FAIL_fn(cv)) break;
            if (frame_depth > 0 && FRAME.loop_break) break;
            if (nd->n >= 2 && nd->c[1]) { IR_exec_node(nd->c[1]); }
            if (frame_depth > 0 && (FRAME.loop_break || FRAME.returning)) break;
            if (frame_depth > 0) FRAME.loop_next = 0;
        }
        if (frame_depth > 0) { FRAME.loop_break = saved_brk_u; FRAME.loop_next = saved_nxt_u; }
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_REPEAT: {
        /* Icon repeat B. Run body forever; exit when body produces FAIL (break semantics). */
        /* nd->c[0] = body. Safety cap prevents infinite loops in broken programs.          */
        if (nd->n < 1 || !nd->c[0]) { nd->value = NULVCL; return nd->γ; }
        int saved_brk_r = frame_depth > 0 ? FRAME.loop_break : 0;
        int saved_nxt_r = frame_depth > 0 ? FRAME.loop_next  : 0;
        if (frame_depth > 0) { FRAME.loop_break = 0; FRAME.loop_next = 0; }
        int safety_r = 1000000;
        while (safety_r-- > 0) {
            nd->c[0]->state = 0;
            IR_exec_node(nd->c[0]);
            if (IS_FAIL_fn(nd->c[0]->value)) break;
            if (frame_depth > 0 && (FRAME.loop_break || FRAME.returning)) break;
            if (frame_depth > 0) FRAME.loop_next = 0;
        }
        if (frame_depth > 0) { FRAME.loop_break = saved_brk_r; FRAME.loop_next = saved_nxt_r; }
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_LIMIT: {
        /* Icon gen \ N — limit a generator to N yields. c[0]=gen, c[1]=limit-expr.                       */
        /* state==0 fresh: evaluate c[1] once to get max (integer-coerced); seed counter=0; eval c[0] α.  */
        /* state==1 active: pump c[0] for next value (β-resume). Each successful yield bumps counter.     */
        /* When counter >= max OR c[0] fails, return ω. N<=0 → immediate fail without pumping c[0].      */
        /* IR_LIMIT is already classified as a generator kind by ir_is_single_shot (line 45) and by the   */
        /* ALT_IS_GEN / IR_IS_GEN_KIND macros — so IR_EVERY drives it correctly to exhaustion.            */
        if (nd->n < 2 || !nd->c[0] || !nd->c[1]) { nd->value = FAILDESCR; return nd->ω; }
        if (nd->state == 0) {
            IR_exec_node(nd->c[1]);
            DESCR_t mv = nd->c[1]->value;
            if (IS_FAIL_fn(mv)) { nd->value = FAILDESCR; return nd->ω; }
            int64_t mx = IS_INT_fn(mv) ? mv.i : (mv.v == DT_R ? (int64_t)mv.r : 0);
            if (mx <= 0) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
            nd->ival    = mx;
            nd->counter = 0;
            nd->c[0]->state = 0;
            nd->state   = 1;
        }
        if (nd->counter >= nd->ival) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t gv = nd->c[0]->value;
        if (IS_FAIL_fn(gv)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        nd->counter++;
        nd->value = gv;
        return nd->γ;
    }
    case IR_ALT: {
        /* Icon alternation A|B|C... n-ary. On alpha: try children left to right; on beta: */
        /* resume current child only if it is a generator kind (can yield multiple values); */
        /* single-shot children (literals, vars) are exhausted after their alpha yield and  */
        /* must NOT be re-pumped on beta — doing so would cause them to re-yield the same   */
        /* value forever (IJ-ALT-BETA-SINGLESHOT fix).                                     */
        /* nd->counter holds current child index (0..n-1). nd->state: 0=fresh, 1=active.   */
        #define ALT_IS_GEN(k) ( \
            (k) == IR_ICN_TO || (k) == IR_ICN_TO_BY || (k) == IR_ICN_UPTO || \
            (k) == IR_ALT    || (k) == IR_ALTERNATE  || (k) == IR_BINOP_GEN || \
            (k) == IR_ICN_ITERATE || (k) == IR_ICN_LIMIT || (k) == IR_ICN_PROC_GEN || \
            (k) == IR_ICN_LIST_BANG || (k) == IR_ICN_KEY_GEN || (k) == IR_TO_BY  || (k) == IR_ICN_ALTERNATE)
        if (nd->n < 1) { nd->value = FAILDESCR; return nd->ω; }
        if (nd->state == 0) {
            for (int i = 0; i < nd->n; i++) {
                if (!nd->c[i]) continue;
                nd->c[i]->state = 0;
                IR_exec_node(nd->c[i]);
                if (!IS_FAIL_fn(nd->c[i]->value)) {
                    nd->value   = nd->c[i]->value;
                    nd->counter = i;
                    nd->state   = 1;
                    return nd->γ;
                }
            }
            nd->value = FAILDESCR;
            return nd->ω;
        }
        /* beta: resume current child only if it is a generator kind */
        int ci = (int)nd->counter;
        if (ci < nd->n && nd->c[ci] && ALT_IS_GEN(nd->c[ci]->t)) {
            IR_exec_node(nd->c[ci]);
            if (!IS_FAIL_fn(nd->c[ci]->value)) { nd->value = nd->c[ci]->value; return nd->γ; }
        }
        /* current child exhausted (or single-shot): try next children from fresh */
        for (int i = ci + 1; i < nd->n; i++) {
            if (!nd->c[i]) continue;
            nd->c[i]->state = 0;
            IR_exec_node(nd->c[i]);
            if (!IS_FAIL_fn(nd->c[i]->value)) {
                nd->value   = nd->c[i]->value;
                nd->counter = i;
                return nd->γ;
            }
        }
        nd->state = 0;
        nd->value = FAILDESCR;
        #undef ALT_IS_GEN
        return nd->ω;
    }
    case IR_TO_BY: {
        /* IJ-TOBY-REAL: integer and real-typed `lo to hi by step`. Real path uses dval/ival3 storage. */
        /* nd->ival2 flag: 0=int mode, 1=real mode (set on α). nd->dval=real counter; nd->ival=int step; */
        /* nd->ival3 stores real step bits via memcpy (double in int64_t). nd->c[1]=hi, c[2]=step.      */
        if (nd->state == 0) {
            int64_t from_i = 0; double from_r = 0.0;
            int64_t by_i = 1;   double by_r = 1.0;
            int is_real = 0;
            if (nd->n > 0 && nd->c[0]) {
                IR_exec_node(nd->c[0]);
                DESCR_t lv = nd->c[0]->value;
                if (lv.v == DT_R) { is_real = 1; from_r = lv.r; }
                else               from_i = IS_INT_fn(lv) ? lv.i : 0;
            }
            if (nd->n > 2 && nd->c[2]) {
                IR_exec_node(nd->c[2]);
                DESCR_t sv = nd->c[2]->value;
                if (sv.v == DT_R) { is_real = 1; by_r = sv.r; }
                else               by_i = IS_INT_fn(sv) ? sv.i : 1;
            }
            if (is_real) {
                if (by_r == 0.0) by_r = 1.0;
                nd->ival2 = 1;
                nd->dval  = from_r;
                memcpy(&nd->ival3, &by_r, sizeof(double));
            } else {
                if (by_i == 0) by_i = 1;
                nd->ival2 = 0;
                nd->counter = from_i;
                nd->ival    = by_i;
            }
            nd->state = 1;
        }
        if (nd->ival2) {
            double by_r; memcpy(&by_r, &nd->ival3, sizeof(double));
            double to_r = 0.0;
            if (nd->n > 1 && nd->c[1]) { IR_exec_node(nd->c[1]); DESCR_t hv = nd->c[1]->value; to_r = (hv.v == DT_R) ? hv.r : (double)(IS_INT_fn(hv) ? hv.i : 0); }
            if (by_r >= 0.0 ? nd->dval > to_r + 1e-12 : nd->dval < to_r - 1e-12) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
            DESCR_t rv; rv.v = DT_R; rv.r = nd->dval; nd->value = rv;
            nd->dval += by_r;
            return nd->γ;
        }
        int64_t to_val = 0;
        if (nd->n > 1 && nd->c[1]) { IR_exec_node(nd->c[1]); to_val = nd->c[1]->value.i; }
        int64_t by = nd->ival;
        if (by >= 0 ? nd->counter > to_val : nd->counter < to_val) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        nd->value    = INTVAL(nd->counter);
        nd->counter += by;
        return nd->γ;
    }
    case IR_ALTERNATE:
        nd->value = FAILDESCR;
        return nd->ω;
    case IR_PAT_LIT: {
        const char *lit = nd->sval ? nd->sval : "";
        int         len = (int)strlen(lit);
        if (nd->state == 0) {
            if (Δ + len > Σlen || (len > 0 && memcmp(Σ + Δ, lit, (size_t)len) != 0)) {
                nd->value = FAILDESCR;
                return nd->ω;
            }
            nd->counter = len;
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, len);
            Δ += len;
            return nd->γ;
        }
        Δ -= (int)nd->counter;
        nd->state = 0;
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_PAT_ANY: {
        const char *chars = nd->sval ? nd->sval : "";
        if (nd->state == 0) {
            if (Δ >= Σlen || !strchr(chars, Σ[Δ])) {
                nd->value = FAILDESCR;
                return nd->ω;
            }
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, 1);
            Δ++;
            return nd->γ;
        }
        Δ--;
        nd->state = 0;
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_NOT: {
        /* Icon `not E`. Succeeds with &null if E fails; fails if E succeeds. One child c[0]=E. */
        if (nd->n < 1 || !nd->c[0]) { nd->value = NULVCL; return nd->γ; }
        nd->c[0]->state = 0;
        IR_exec_node(nd->c[0]);
        if (IS_FAIL_fn(nd->c[0]->value)) { nd->value = NULVCL; return nd->γ; }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_BREAK: {
        /* Icon break — set loop_break flag on current frame; propagate failure upward.         */
        if (frame_depth > 0) FRAME.loop_break = 1;
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_NEXT: {
        /* Icon next — set loop_next flag on current frame; propagate failure upward.           */
        if (frame_depth > 0) FRAME.loop_next = 1;
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_NONNULL: {
        /* Icon \x nonnull-test — c[0]=operand; succeed with operand if non-null, else ω.      */
        if (nd->n < 1 || !nd->c[0]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t v = nd->c[0]->value;
        if (IS_FAIL_fn(v)) { nd->value = FAILDESCR; return nd->ω; }
        if (v.v == DT_SNUL) { nd->value = FAILDESCR; return nd->ω; }
        nd->value = v;
        return nd->γ;
    }
    case IR_IDENTICAL: {
        /* Icon === — c[0]=lhs, c[1]=rhs; succeed with rhs if identical (same object), else ω. */
        if (nd->n < 2 || !nd->c[0] || !nd->c[1]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t lv = nd->c[0]->value;
        IR_exec_node(nd->c[1]);
        DESCR_t rv = nd->c[1]->value;
        if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) { nd->value = FAILDESCR; return nd->ω; }
        int ident = 0;
        if (lv.v == rv.v) {
            if (lv.v == DT_SNUL) ident = 1;
            else if (lv.v == DT_I) ident = (lv.i == rv.i);
            else if (lv.v == DT_S || lv.v == DT_K) ident = (lv.s == rv.s) || (lv.s && rv.s && strcmp(lv.s, rv.s) == 0);
            else if (lv.v == DT_DATA) ident = (lv.ptr == rv.ptr);
            else if (lv.v == DT_T) ident = (lv.tbl == rv.tbl);
            else ident = (lv.i == rv.i);
        }
        if (!ident) { nd->value = FAILDESCR; return nd->ω; }
        nd->value = rv;
        return nd->γ;
    }
    case IR_NULL_TEST: {
        /* Icon \x null-test — c[0]=operand; succeed with &null if operand is &null, else ω.   */
        if (nd->n < 1 || !nd->c[0]) { nd->value = NULVCL; return nd->γ; }
        IR_exec_node(nd->c[0]);
        DESCR_t v = nd->c[0]->value;
        if (IS_FAIL_fn(v)) { nd->value = FAILDESCR; return nd->ω; }
        if (v.v == DT_SNUL) { nd->value = NULVCL; return nd->γ; }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_RANDOM: {
        /* Icon ?E — random element of string/list/table/integer. c[0]=operand.                 */
        if (nd->n < 1 || !nd->c[0]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t v = nd->c[0]->value;
        if (IS_FAIL_fn(v)) { nd->value = FAILDESCR; return nd->ω; }
        extern uint64_t bb_icn_rnd_seed;
        bb_icn_rnd_seed = bb_icn_rnd_seed * 6364136223846793005UL + 1442695040888963407UL;
        unsigned long rnd = (unsigned long)(bb_icn_rnd_seed >> 33);
        if (IS_INT_fn(v)) {
            int64_t n = v.i;
            if (n <= 0) { nd->value = INTVAL(0); return nd->γ; }
            nd->value = INTVAL((int64_t)(rnd % (unsigned long)n) + 1);
            return nd->γ;
        }
        if (v.v == DT_T) {
            if (!v.tbl || v.tbl->size <= 0) { nd->value = FAILDESCR; return nd->ω; }
            int target = (int)(rnd % (unsigned long)v.tbl->size);
            int seen = 0;
            for (int b = 0; b < TABLE_BUCKETS; b++) {
                for (TBPAIR_t *p = v.tbl->buckets[b]; p; p = p->next) {
                    if (seen == target) { nd->value = p->val; return nd->γ; }
                    seen++;
                }
            }
            nd->value = FAILDESCR; return nd->ω;
        }
        const char *s = VARVAL_fn(v);
        if (s) {
            long slen = v.slen > 0 ? v.slen : (long)strlen(s);
            if (slen <= 0) { nd->value = FAILDESCR; return nd->ω; }
            int idx = (int)(rnd % (unsigned long)slen);
            char buf[2] = { s[idx], '\0' };
            nd->value = STRVAL(GC_strdup(buf));
            return nd->γ;
        }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_NEG: {
        /* Icon -E unary minus.  Evaluate c[0]; reuse icn_binop_apply(SUB, 0, v) for coercion.       */
        if (nd->n < 1 || !nd->c[0]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t v = nd->c[0]->value;
        if (IS_FAIL_fn(v)) { nd->value = FAILDESCR; return nd->ω; }
        int rel_fail = 0;
        DESCR_t result = icn_binop_apply(ICN_BINOP_SUB, INTVAL(0), v, &rel_fail);
        if (IS_FAIL_fn(result)) { nd->value = FAILDESCR; return nd->ω; }
        nd->value = result;
        return nd->γ;
    }
    case IR_POS: {
        /* Icon +E unary plus.  Numeric coerce via icn_binop_apply(ADD, 0, v).                       */
        if (nd->n < 1 || !nd->c[0]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t v = nd->c[0]->value;
        if (IS_FAIL_fn(v)) { nd->value = FAILDESCR; return nd->ω; }
        int rel_fail = 0;
        DESCR_t result = icn_binop_apply(ICN_BINOP_ADD, INTVAL(0), v, &rel_fail);
        if (IS_FAIL_fn(result)) { nd->value = FAILDESCR; return nd->ω; }
        nd->value = result;
        return nd->γ;
    }
    case IR_CSET_COMPL: {
        /* Icon ~E cset complement.  Evaluate c[0]; coerce int/real → string; complement against     */
        /* the 256-char universal cset via icn_cset_complement.  Mirrors TT_CSET_COMPL in icn_value.c. */
        if (nd->n < 1 || !nd->c[0]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t v = nd->c[0]->value;
        if (IS_FAIL_fn(v)) { nd->value = FAILDESCR; return nd->ω; }
        if (IS_INT_fn(v) || IS_REAL_fn(v)) v = descr_to_str_icn(v);
        const char *cs = IS_NULL_fn(v) ? "" : VARVAL_fn(v);
        nd->value = CSETVAL(icn_cset_complement(cs ? cs : ""));
        return nd->γ;
    }
    case IR_CSET_UNION:
    case IR_CSET_DIFF:
    case IR_CSET_INTER: {
        /* Icon cset binops: E1++E2 / E1--E2 / E1**E2.  Both operands coerced to strings, then       */
        /* canonical-form-merged via icn_cset_{union,diff,inter}.  Mirrors TT_CSET_* in icn_value.c. */
        if (nd->n < 2 || !nd->c[0] || !nd->c[1]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t lv = nd->c[0]->value;
        if (IS_FAIL_fn(lv)) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[1]);
        DESCR_t rv = nd->c[1]->value;
        if (IS_FAIL_fn(rv)) { nd->value = FAILDESCR; return nd->ω; }
        if (IS_INT_fn(lv) || IS_REAL_fn(lv)) lv = descr_to_str_icn(lv);
        if (IS_INT_fn(rv) || IS_REAL_fn(rv)) rv = descr_to_str_icn(rv);
        const char *a = IS_NULL_fn(lv) ? "" : VARVAL_fn(lv); if (!a) a = "";
        const char *b = IS_NULL_fn(rv) ? "" : VARVAL_fn(rv); if (!b) b = "";
        const char *raw = (nd->t == IR_CSET_UNION) ? icn_cset_union(a, b)
                        : (nd->t == IR_CSET_DIFF)  ? icn_cset_diff (a, b)
                                                   : icn_cset_inter(a, b);
        nd->value = CSETVAL(icn_cset_canonical(raw));
        return nd->γ;
    }
    case IR_ICN_SCAN: {
        /* Icon subj ? body.  c[0]=subj, c[1]=body.  Push scan_subj/scan_pos, eval body, restore.    */
        if (nd->n < 1 || !nd->c[0]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t sv = nd->c[0]->value;
        if (IS_FAIL_fn(sv)) { nd->value = FAILDESCR; return nd->ω; }
        const char *s = VARVAL_fn(sv);
        if (!s) s = "";
        if (scan_depth < SCAN_STACK_MAX) {
            scan_stack[scan_depth].subj = scan_subj;
            scan_stack[scan_depth].pos  = scan_pos;
            scan_depth++;
        }
        scan_subj = s;
        scan_pos  = 1;
        DESCR_t body_val = NULVCL;
        int body_ok = 1;
        if (nd->n >= 2 && nd->c[1]) {
            IR_exec_node(nd->c[1]);
            body_val = nd->c[1]->value;
            if (IS_FAIL_fn(body_val)) body_ok = 0;
        }
        if (scan_depth > 0) {
            scan_depth--;
            scan_subj = scan_stack[scan_depth].subj;
            scan_pos  = scan_stack[scan_depth].pos;
        }
        if (!body_ok) { nd->value = FAILDESCR; return nd->ω; }
        nd->value = body_val;
        return nd->γ;
    }
    case IR_ICN_KEYWORD: {
        /* Icon &name keyword read.  sval = "&subject", "&pos", "&null", "&fail", etc.               */
        if (!nd->sval) { nd->value = NULVCL; return nd->γ; }
        const char *kw = nd->sval[0] == '&' ? nd->sval + 1 : nd->sval;
        if (!strcmp(kw, "subject")) {
            nd->value = scan_subj ? STRVAL(scan_subj) : NULVCL;
            return nd->γ;
        }
        if (!strcmp(kw, "pos")) {
            nd->value = INTVAL((int64_t)scan_pos);
            return nd->γ;
        }
        if (!strcmp(kw, "null")) {
            nd->value = NULVCL;
            return nd->γ;
        }
        if (!strcmp(kw, "fail")) {
            nd->value = FAILDESCR;
            return nd->ω;
        }
        /* Delegate to the central keyword dispatcher: handles &cset, &ascii, &lcase, &ucase,         */
        /* &letters, &digits, &e, &pi, &phi, &error, &trace, &dump, &random, &level, &date, &time,   */
        /* &clock, &dateline, &version, &input/&output/&errout, &main/&source/&current, and the      */
        /* mouse-event keywords.  Returns FAILDESCR only for genuinely-unknown keywords, at which    */
        /* point we fall through to the global-var lookup (handles user '&'-prefixed globals).      */
        DESCR_t kv = icn_kw_read(kw);
        if (!IS_FAIL_fn(kv)) {
            nd->value = kv;
            return nd->γ;
        }
        /* Unknown keyword: fall back to global var lookup with leading '&'. */
        DESCR_t gv = NV_GET_fn(nd->sval);
        nd->value = gv;
        return IS_FAIL_fn(gv) ? nd->ω : nd->γ;
    }
    case IR_SIZE: {
        /* Icon *E — size of string/list/table. One child c[0]=E. Returns integer length.       */
        /* For csets, mirrors icn_value.c::TT_SIZE: use icn_kw_cset_len(ptr) for keyword csets   */
        /* (&cset/&ascii/&lcase/...) since their canonical-form buffer is null-prefixed and     */
        /* would otherwise read as length 0 via strlen.                                          */
        if (nd->n < 1 || !nd->c[0]) { nd->value = INTVAL(0); return nd->γ; }
        IR_exec_node(nd->c[0]);
        DESCR_t v = nd->c[0]->value;
        if (IS_FAIL_fn(v)) { nd->value = FAILDESCR; return nd->ω; }
        if (IS_INT_fn(v) || IS_REAL_fn(v)) { nd->value = INTVAL(0); return nd->γ; }
        /* DT_DATA: list (icn_type="list") → frame_size field; record → nfields. */
        if (v.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(v, "icn_type");
            if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                nd->value = INTVAL((int)FIELD_GET_fn(v, "frame_size").i);
                return nd->γ;
            }
            if (v.u && v.u->type) { nd->value = INTVAL(v.u->type->nfields); return nd->γ; }
            nd->value = INTVAL(0); return nd->γ;
        }
        /* DT_T: table → count entries across all buckets. */
        if (v.v == DT_T && v.tbl) {
            long cnt = 0;
            for (int b = 0; b < TABLE_BUCKETS; b++)
                for (TBPAIR_t *ep = v.tbl->buckets[b]; ep; ep = ep->next) cnt++;
            nd->value = INTVAL(cnt); return nd->γ;
        }
        long len;
        if (IS_CSET_fn(v)) {
            int klen = icn_kw_cset_len(v.s);
            len = klen >= 0 ? (long)klen : (v.s ? (long)strlen(v.s) : 0);
        } else {
            const char *s = VARVAL_fn(v);
            len = s ? (long)strlen(s) : 0;
        }
        nd->value = INTVAL(len);
        return nd->γ;
    }
    case IR_ICN_IDX: {
        /* Icon s[i] string/list/table subscript. c[0]=base, c[1]=index. Calls subscript_get for         */
        /* type-dispatched subscript: strings (1-based, negative-OK), lists, tables. FAIL on OOB.        */
        if (nd->n < 2 || !nd->c[0] || !nd->c[1]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t base = nd->c[0]->value;
        if (IS_FAIL_fn(base)) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[1]);
        DESCR_t idx = nd->c[1]->value;
        if (IS_FAIL_fn(idx)) { nd->value = FAILDESCR; return nd->ω; }
        DESCR_t r = subscript_get(base, idx);
        if (IS_FAIL_fn(r)) { nd->value = FAILDESCR; return nd->ω; }
        nd->value = r;
        return nd->γ;
    }
    case IR_ICN_SECTION: {
        /* Icon s[i:j] / s[i+:n] / s[i-:n] section. c[0]=base, c[1]=i, c[2]=j. nd->ival encodes kind:    */
        /* 0=RANGE (i:j), 1=PLUS (i+:n → [i, i+n)), 2=MINUS (i-:n → [i-n, i)). subscript_get2 expects    */
        /* RANGE bounds; for PLUS/MINUS we transform j to absolute before the call (int-mode only).     */
        if (nd->n < 3 || !nd->c[0] || !nd->c[1] || !nd->c[2]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t base = nd->c[0]->value;
        if (IS_FAIL_fn(base)) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[1]);
        DESCR_t i1 = nd->c[1]->value;
        if (IS_FAIL_fn(i1)) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[2]);
        DESCR_t i2 = nd->c[2]->value;
        if (IS_FAIL_fn(i2)) { nd->value = FAILDESCR; return nd->ω; }
        if (nd->ival == 1 && IS_INT_fn(i1) && IS_INT_fn(i2)) {
            i2 = INTVAL(i1.i + i2.i);
        } else if (nd->ival == 2 && IS_INT_fn(i1) && IS_INT_fn(i2)) {
            int64_t lo = i1.i - i2.i;
            i2 = i1;
            i1 = INTVAL(lo);
        }
        DESCR_t r = subscript_get2(base, i1, i2);
        if (IS_FAIL_fn(r)) { nd->value = FAILDESCR; return nd->ω; }
        nd->value = r;
        return nd->γ;
    }
    case IR_ICN_LIST_BANG: {
        /* Icon !L generator.  c[0]=iterable expr (list, table, or string).                          */
        /* On α (state==0): evaluate c[0] once and cache the DT_DATA descriptor in opaque.           */
        /* On β (state==1): advance pos. γ on hit, ω on exhaustion.                                  */
        /* Supports DT_DATA lists (icn_type="list") and DT_T tables (yield values).                  */
        if (nd->n < 1 || !nd->c[0]) { nd->value = FAILDESCR; return nd->ω; }
        if (nd->state == 0) {
            /* α path: evaluate the iterable and cache it. */
            IR_exec_node(nd->c[0]);
            DESCR_t obj = nd->c[0]->value;
            if (IS_FAIL_fn(obj)) { nd->value = FAILDESCR; return nd->ω; }
            /* Cache the collection descriptor in opaque so β doesn't re-evaluate. */
            DESCR_t *cached = GC_malloc(sizeof(DESCR_t));
            *cached = obj;
            nd->opaque  = (void *)cached;
            nd->counter = 0;       /* element index */
            nd->state   = 1;
        } else {
            nd->counter++;
        }
        DESCR_t obj = *(DESCR_t *)nd->opaque;
        /* DT_DATA path: list (icn_type="list") or record. */
        if (obj.v == DT_DATA) {
            DESCR_t tag = FIELD_GET_fn(obj, "icn_type");
            if (tag.v == DT_S && tag.s && strcmp(tag.s, "list") == 0) {
                int n       = (int)FIELD_GET_fn(obj, "frame_size").i;
                DESCR_t ea  = FIELD_GET_fn(obj, "frame_elems");
                DESCR_t *elems = (ea.v == DT_DATA) ? (DESCR_t *)ea.ptr : NULL;
                if (!elems || nd->counter >= n) {
                    nd->state = 0; nd->opaque = NULL; nd->value = FAILDESCR; return nd->ω;
                }
                nd->value = elems[nd->counter];
                return nd->γ;
            }
            /* Record iteration: yield each field value in order. */
            if (obj.u && obj.u->type && obj.u->type->nfields > 0) {
                int nf = obj.u->type->nfields;
                if (nd->counter >= nf) {
                    nd->state = 0; nd->opaque = NULL; nd->value = FAILDESCR; return nd->ω;
                }
                nd->value = obj.u->fields[nd->counter];
                return nd->γ;
            }
        }
        /* DT_T path: table — yield entry values in bucket order. */
        if (obj.v == DT_T && obj.tbl) {
            TBBLK_t *tbl = obj.tbl;
            /* Walk bucket+chain to find the nd->counter-th entry. */
            int64_t target = nd->counter;
            int64_t seen   = 0;
            for (int b = 0; b < TABLE_BUCKETS; b++) {
                for (TBPAIR_t *ep = tbl->buckets[b]; ep; ep = ep->next) {
                    if (seen == target) { nd->value = ep->val; return nd->γ; }
                    seen++;
                }
            }
            nd->state = 0; nd->opaque = NULL; nd->value = FAILDESCR; return nd->ω;
        }
        /* Fallback: string iteration — each character. */
        {
            DESCR_t sv = obj;
            const char *s = (sv.v == DT_S) ? sv.s : NULL;
            int64_t slen  = s ? (int64_t)(sv.slen > 0 ? sv.slen : strlen(s)) : 0;
            if (!s || nd->counter >= slen) {
                nd->state = 0; nd->opaque = NULL; nd->value = FAILDESCR; return nd->ω;
            }
            char *ch = GC_malloc(2);
            ch[0] = s[nd->counter];
            ch[1] = '\0';
            nd->value = (DESCR_t){ .v = DT_S, .slen = 1, .s = ch };
            return nd->γ;
        }
    }
    case IR_ICN_RECORD_DEF: {
        /* Register the record type on first execution (nd->state==0).  sval=spec string.          */
        /* Idempotent: sc_dat_register is safe to call multiple times (no-ops if already exists). */
        if (nd->state == 0 && nd->sval) {
            DEFDAT_fn(nd->sval);
            sc_dat_register(nd->sval);
            nd->state = 1;
        }
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_ICN_FIELD_GET: {
        /* obj.field read.  c[0]=object expr, sval=field name.                                    */
        if (nd->n < 1 || !nd->c[0] || !nd->sval) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t obj = nd->c[0]->value;
        if (IS_FAIL_fn(obj)) { nd->value = FAILDESCR; return nd->ω; }
        DESCR_t *cell = data_field_ptr(nd->sval, obj);
        if (!cell) { nd->value = FAILDESCR; return nd->ω; }
        nd->value = *cell;
        return nd->γ;
    }
    case IR_ICN_FIELD_SET: {
        /* obj.field := rhs.  c[0]=object expr, c[1]=rhs expr, sval=field name.                  */
        /* Returns the rhs value (Icon assignment expression value semantics).                     */
        if (nd->n < 2 || !nd->c[0] || !nd->c[1] || !nd->sval) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[1]);
        DESCR_t rhs = nd->c[1]->value;
        if (IS_FAIL_fn(rhs)) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t obj = nd->c[0]->value;
        if (IS_FAIL_fn(obj)) { nd->value = FAILDESCR; return nd->ω; }
        DESCR_t *cell = data_field_ptr(nd->sval, obj);
        if (!cell) { nd->value = FAILDESCR; return nd->ω; }
        *cell = rhs;
        nd->value = rhs;
        return nd->γ;
    }
    case IR_ICN_IDX_SET: {
        /* Icon base[idx] := rhs.  c[0]=base, c[1]=index, c[2]=rhs; calls subscript_set.         */
        /* Returns the rhs value on success (Icon assignment expression semantics).                */
        if (nd->n < 3 || !nd->c[0] || !nd->c[1] || !nd->c[2]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t base = nd->c[0]->value;
        if (IS_FAIL_fn(base)) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[1]);
        DESCR_t idx = nd->c[1]->value;
        if (IS_FAIL_fn(idx)) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[2]);
        DESCR_t rhs = nd->c[2]->value;
        if (IS_FAIL_fn(rhs)) { nd->value = FAILDESCR; return nd->ω; }
        if (!subscript_set(base, idx, rhs)) { nd->value = FAILDESCR; return nd->ω; }
        nd->value = rhs;
        return nd->γ;
    }
    case IR_ICN_KEY_GEN: {
        /* key(t) generator.  c[0]=table expr.  Yields each key in bucket order.                  */
        /* α (state==0): evaluate table, cache in opaque, reset bucket+entry pointers.             */
        /* β (state==1): advance to next entry.  γ on hit, ω when all buckets exhausted.          */
        if (nd->n < 1 || !nd->c[0]) { nd->value = FAILDESCR; return nd->ω; }
        TBBLK_t *tbl = NULL;
        if (nd->state == 0) {
            IR_exec_node(nd->c[0]);
            DESCR_t tv = nd->c[0]->value;
            if (IS_FAIL_fn(tv) || tv.v != DT_T || !tv.tbl) { nd->value = FAILDESCR; return nd->ω; }
            tbl = tv.tbl;
            nd->opaque  = (void *)tbl;
            nd->counter = 0;   /* current bucket index */
            nd->ival    = 0;   /* entry index within all entries (flat) */
            nd->state   = 1;
        } else {
            tbl = (TBBLK_t *)nd->opaque;
            nd->ival++;        /* advance to next entry */
        }
        if (!tbl) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        /* Walk entries in order to find the nd->ival-th one. */
        int64_t target = nd->ival, seen = 0;
        for (int b = 0; b < TABLE_BUCKETS; b++) {
            for (TBPAIR_t *ep = tbl->buckets[b]; ep; ep = ep->next) {
                if (seen == target) {
                    nd->value = ep->key_descr;
                    return nd->γ;
                }
                seen++;
            }
        }
        nd->state = 0; nd->opaque = NULL; nd->value = FAILDESCR; return nd->ω;
    }
    case IR_CASE: {
        /* Icon case E of { K1: V1; K2: V2; ...; default: VD }.                                  */
        /* c[0]=selector; c[1],c[2]=key1,val1; c[3],c[4]=key2,val2; ... last child=default.      */
        /* Selector compared with keys using string equality (Icon case uses === then string eq). */
        if (nd->n < 1 || !nd->c[0]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]);
        DESCR_t sel = nd->c[0]->value;
        if (IS_FAIL_fn(sel)) { nd->value = FAILDESCR; return nd->ω; }
        /* Determine if last child is a default (odd number of children after selector). */
        int npairs = (nd->n - 1) / 2;
        int has_default = ((nd->n - 1) % 2 == 1);
        for (int i = 0; i < npairs; i++) {
            IR_t *key_nd = nd->c[1 + i * 2];
            IR_t *val_nd = nd->c[2 + i * 2];
            if (!key_nd || !val_nd) continue;
            IR_exec_node(key_nd);
            DESCR_t kv = key_nd->value;
            /* Compare selector to key: numeric equality first, then string. */
            int match = 0;
            if (IS_INT_fn(sel) && IS_INT_fn(kv)) match = (sel.i == kv.i);
            else {
                const char *ss = VARVAL_fn(sel); if (!ss) ss = "";
                const char *ks = VARVAL_fn(kv);  if (!ks) ks = "";
                match = (strcmp(ss, ks) == 0);
            }
            if (match) {
                IR_exec_node(val_nd);
                nd->value = val_nd->value;
                return IS_FAIL_fn(nd->value) ? nd->ω : nd->γ;
            }
        }
        if (has_default && nd->c[nd->n - 1]) {
            IR_exec_node(nd->c[nd->n - 1]);
            nd->value = nd->c[nd->n - 1]->value;
            return IS_FAIL_fn(nd->value) ? nd->ω : nd->γ;
        }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_PAT_BREAK: {
        const char *chars = nd->sval ? nd->sval : "";
        if (nd->state == 0) {
            int i = 0;
            while (Δ + i < Σlen && !strchr(chars, Σ[Δ + i])) i++;
            nd->counter = i;
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, i);
            Δ += i;
            return nd->γ;
        }
        Δ -= (int)nd->counter;
        nd->state = 0;
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_PAT_SPAN: {
        const char *chars = nd->sval ? nd->sval : "";
        if (nd->state == 0) {
            int i = 0;
            while (Δ + i < Σlen && strchr(chars, Σ[Δ + i])) i++;
            if (i == 0) { nd->value = FAILDESCR; return nd->ω; }
            nd->counter = i;
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, i);
            Δ += i;
            return nd->γ;
        }
        if (nd->state == 1) {
            Δ -= (int)nd->counter;
            nd->counter--;
            if (nd->counter < 1) { nd->state = 2; nd->value = FAILDESCR; return nd->ω; }
            nd->value = descr_match_span(Σ + Δ, (int)nd->counter);
            Δ += (int)nd->counter;
            return nd->γ;
        }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_PAT_ARB: {
        if (nd->state == 0) {
            nd->counter = 0;
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, 0);
            return nd->γ;
        }
        if (nd->state == 1) {
            Δ -= (int)nd->counter;
            nd->counter++;
            if (Δ + (int)nd->counter > Σlen) {
                nd->state = 2;
                nd->value = FAILDESCR;
                return nd->ω;
            }
            nd->value = descr_match_span(Σ + Δ, (int)nd->counter);
            Δ += (int)nd->counter;
            return nd->γ;
        }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_PAT_REM: {
        if (nd->state == 0) {
            int rem = Σlen - Δ;
            nd->counter = rem;
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, rem);
            Δ = Σlen;
            return nd->γ;
        }
        Δ -= (int)nd->counter;
        nd->state = 0;
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_PAT_FENCE: {
        if (nd->state == 0) {
            nd->state = 1;
            nd->value = NULVCL;
            return nd->γ;
        }
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_PAT_ABORT: {
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_PAT_LEN: {
        int64_t n = nd->ival;
        if (nd->state == 0) {
            if (n < 0 || Δ + (int)n > Σlen) { nd->value = FAILDESCR; return nd->ω; }
            nd->counter = n;
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, (int)n);
            Δ += (int)n;
            return nd->γ;
        }
        Δ -= (int)nd->counter;
        nd->state = 0; nd->value = FAILDESCR; return nd->ω;
    }
    case IR_PAT_NOTANY: {
        const char *chars = nd->sval ? nd->sval : "";
        if (nd->state == 0) {
            if (Δ >= Σlen || strchr(chars, Σ[Δ])) { nd->value = FAILDESCR; return nd->ω; }
            nd->state = 1;
            nd->value = descr_match_span(Σ + Δ, 1);
            Δ++;
            return nd->γ;
        }
        Δ--; nd->state = 0; nd->value = FAILDESCR; return nd->ω;
    }
    case IR_PAT_POS: {
        if (nd->state == 0) {
            int64_t arg = nd->ival;
            int     pos = nd->n ? (Σlen - (int)arg) : (int)arg;
            if (pos < 0 || pos > Σlen || Δ != pos) { nd->value = FAILDESCR; return nd->ω; }
            nd->state = 1;
            nd->value = NULVCL;
            return nd->γ;
        }
        nd->state = 0; nd->value = FAILDESCR; return nd->ω;
    }
    case IR_PAT_TAB: {
        if (nd->state == 0) {
            int64_t arg    = nd->ival;
            int     target = nd->n ? (Σlen - (int)arg) : (int)arg;
            if (target < 0 || target > Σlen || Δ > target) { nd->value = FAILDESCR; return nd->ω; }
            nd->counter = Δ;
            nd->state   = 1;
            nd->value   = descr_match_span(Σ + Δ, target - Δ);
            Δ = target;
            return nd->γ;
        }
        Δ = (int)nd->counter;
        nd->state = 0; nd->value = FAILDESCR; return nd->ω;
    }
    case IR_PAT_CAT:
    case IR_PAT_ALT:
        nd->value = NULVCL;
        return nd->γ;
    case IR_PAT_ASSIGN_COND: {
        if (nd->state == 0) {
            nd->counter = Δ;
            nd->state   = 1;
            nd->value   = NULVCL;
            return nd->α;
        }
        if (nd->sval && *nd->sval) {
            int matched_len = Δ - (int)nd->counter;
            if (matched_len < 0) matched_len = 0;
            char *copy = (char *)GC_MALLOC((size_t)matched_len + 1);
            if (copy) { memcpy(copy, Σ + (int)nd->counter, (size_t)matched_len); copy[matched_len] = '\0'; }
            DESCR_t matched = { .v = DT_S, .slen = (uint32_t)matched_len, .s = copy ? copy : "" };
            NV_SET_fn(nd->sval, matched);
        }
        nd->state = 0;
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_PAT_ASSIGN_IMM: {
        if (nd->state == 0) {
            nd->counter = Δ;
            nd->state   = 1;
            nd->value   = NULVCL;
            return nd->α;
        }
        if (nd->sval && *nd->sval) {
            int matched_len = Δ - (int)nd->counter;
            if (matched_len < 0) matched_len = 0;
            char *copy = (char *)GC_MALLOC((size_t)matched_len + 1);
            if (copy) { memcpy(copy, Σ + (int)nd->counter, (size_t)matched_len); copy[matched_len] = '\0'; }
            DESCR_t matched = { .v = DT_S, .slen = (uint32_t)matched_len, .s = copy ? copy : "" };
            NV_SET_fn(nd->sval, matched);
        }
        nd->state = 0;
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_PAT_ARBNO: {
        IR_block_t * inner_blk = nd->c ? (IR_block_t *)((void **)nd->c)[0] : NULL;
        int       * pos_stack = nd->c ? (int       *)((void **)nd->c)[1] : NULL;
        if (!inner_blk || !pos_stack) { nd->value = FAILDESCR; return nd->ω; }
        if (nd->state == 0) {
            int depth = 0;
            int cap   = nd->n;
            nd->counter = Δ;
            while (depth < cap) {
                int pre = Δ;
                DESCR_t r = IR_exec_once(inner_blk);
                if (IS_FAIL_fn(r) || Δ == pre) break;
                pos_stack[depth++] = Δ;
            }
            nd->state = depth;
            nd->value = NULVCL;
            return nd->γ;
        }
        nd->state--;
        if (nd->state < 0) { nd->value = FAILDESCR; return nd->ω; }
        Δ = (nd->state > 0) ? pos_stack[nd->state - 1] : (int)nd->counter;
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_ICN_UPTO: {
        if (nd->state == 0) nd->counter = 0;
        nd->state = 1;
        const char *cset = nd->sval ? nd->sval : "";
        const char *hay  = nd->sval2 ? nd->sval2 : "";
        int slen = (int)strlen(hay);
        while (nd->counter < slen) {
            char c = hay[nd->counter];
            nd->counter++;
            if (strchr(cset, c)) {
                nd->value = INTVAL((int64_t)nd->counter);
                return nd->γ;
            }
        }
        nd->state = 0; nd->value = FAILDESCR; return nd->ω;
    }
    case IR_ICN_TO: {
        /* Gen-kind check for operand re-pumping (mirrors IR_BINOP_GEN logic).                    */
        #define IR_IS_GEN_KIND_TO(k) ( \
            (k) == IR_ICN_TO || (k) == IR_ICN_TO_BY || (k) == IR_ICN_UPTO || \
            (k) == IR_ALT || (k) == IR_ALTERNATE || (k) == IR_BINOP_GEN || \
            (k) == IR_ICN_ITERATE || (k) == IR_ICN_LIMIT || (k) == IR_ICN_PROC_GEN || \
            (k) == IR_ICN_LIST_BANG || (k) == IR_ICN_KEY_GEN || (k) == IR_TO_BY)
        int has_dyn = (nd->n >= 2 && nd->c[0] && nd->c[1]);
        int lo_gen  = has_dyn && IR_IS_GEN_KIND_TO(nd->c[0]->t);
        int hi_gen  = has_dyn && IR_IS_GEN_KIND_TO(nd->c[1]->t);
        if (nd->state == 0) {
            /* α path: if c[0]/c[1] present, evaluate them now to seed ival/ival2 (dynamic bounds). */
            if (has_dyn) {
                IR_exec_node(nd->c[0]);
                if (IS_FAIL_fn(nd->c[0]->value)) { nd->value = FAILDESCR; return nd->ω; }
                IR_exec_node(nd->c[1]);
                if (IS_FAIL_fn(nd->c[1]->value)) { nd->value = FAILDESCR; return nd->ω; }
                nd->ival  = nd->c[0]->value.i;
                nd->ival2 = nd->c[1]->value.i;
            }
            nd->counter = nd->ival;
        }
        else nd->counter++;
        nd->state = 1;
        if (nd->counter > nd->ival2) {
            /* Counter exhausted. If hi is a generator, advance it. If hi also exhausts and lo is a */
            /* generator, reset hi and advance lo. If both exhaust (or neither is a generator),    */
            /* fail. This is the cross-product semantics from Icon's paper §2 example 3:           */
            /*   every write((1 to 2) to (2 to 3))  →  1,2,1,2,3,2,2,3                              */
            if (hi_gen) {
                IR_exec_node(nd->c[1]);
                if (IS_FAIL_fn(nd->c[1]->value)) {
                    if (!lo_gen) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                    IR_exec_node(nd->c[0]);
                    if (IS_FAIL_fn(nd->c[0]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                    nd->c[1]->state = 0;
                    IR_exec_node(nd->c[1]);
                    if (IS_FAIL_fn(nd->c[1]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                    nd->ival = nd->c[0]->value.i;
                }
                nd->ival2 = nd->c[1]->value.i;
                nd->counter = nd->ival;
                if (nd->counter > nd->ival2) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
            } else if (lo_gen) {
                IR_exec_node(nd->c[0]);
                if (IS_FAIL_fn(nd->c[0]->value)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                nd->ival = nd->c[0]->value.i;
                nd->counter = nd->ival;
                if (nd->counter > nd->ival2) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
            } else {
                nd->state = 0; nd->value = FAILDESCR; return nd->ω;
            }
        }
        nd->value = INTVAL(nd->counter);
        return nd->γ;
        #undef IR_IS_GEN_KIND_TO
    }
    case IR_ICN_TO_BY: {
        int64_t step = nd->ival3 ? nd->ival3 : 1;
        if (nd->state == 0) nd->counter = nd->ival;
        else nd->counter += step;
        nd->state = 1;
        int exhausted = (step > 0) ? (nd->counter > nd->ival2) : (nd->counter < nd->ival2);
        if (exhausted) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        nd->value = INTVAL(nd->counter);
        return nd->γ;
    }
    case IR_ICN_ITERATE: {
        if (nd->state == 0) nd->counter = 0;
        else nd->counter++;
        nd->state = 1;
        int64_t len = nd->ival;
        const char *str = nd->sval2 ? nd->sval2 : "";
        if (nd->counter >= len) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        char *ch = GC_malloc(2);
        ch[0] = str[nd->counter];
        ch[1] = '\0';
        nd->value = (DESCR_t){ .v = DT_S, .slen = 1, .s = ch };
        return nd->γ;
    }
    case IR_ICN_ALTERNATE: {
        icn_alt_dcg_t *z = (icn_alt_dcg_t *)nd->opaque;
        if (!z) { nd->value = FAILDESCR; return nd->ω; }
        if (nd->state == 0) {
            z->which = 0;
            DESCR_t v = z->gen[0].fn(z->gen[0].ζ, α);
            if (!IS_FAIL_fn(v)) { nd->value = v; nd->state = 1; return nd->γ; }
            z->which = 1;
            DESCR_t v2 = z->gen[1].fn(z->gen[1].ζ, α);
            if (!IS_FAIL_fn(v2)) { nd->value = v2; nd->state = 1; return nd->γ; }
            nd->value = FAILDESCR; return nd->ω;
        }
        DESCR_t v = z->gen[z->which].fn(z->gen[z->which].ζ, β);
        if (!IS_FAIL_fn(v)) { nd->value = v; return nd->γ; }
        if (z->which == 0) {
            z->which = 1;
            DESCR_t v2 = z->gen[1].fn(z->gen[1].ζ, α);
            if (!IS_FAIL_fn(v2)) { nd->value = v2; return nd->γ; }
        }
        nd->state = 0; nd->value = FAILDESCR; return nd->ω;
    }
    case IR_ICN_BINOP: {
        icn_binop_dcg_t *z = (icn_binop_dcg_t *)nd->opaque;
        if (!z) { nd->value = FAILDESCR; return nd->ω; }
        if (nd->state == 0) {
            z->left_val = z->left.fn(z->left.ζ, α);
            if (IS_FAIL_fn(z->left_val)) { nd->value = FAILDESCR; return nd->ω; }
            z->right_val = z->right.fn(z->right.ζ, α);
            if (IS_FAIL_fn(z->right_val)) { nd->value = FAILDESCR; return nd->ω; }
            nd->state = 1;
        } else {
            for (;;) {
                DESCR_t rv = z->right.fn(z->right.ζ, β);
                if (!IS_FAIL_fn(rv)) { z->right_val = rv; break; }
                DESCR_t lv = z->left.fn(z->left.ζ, β);
                if (IS_FAIL_fn(lv)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
                z->left_val = lv;
                z->right_val = z->right.fn(z->right.ζ, α);
                if (!IS_FAIL_fn(z->right_val)) break;
                nd->state = 0; nd->value = FAILDESCR; return nd->ω;
            }
        }
        for (;;) {
            int rel_fail = 0;
            DESCR_t result = icn_binop_apply(z->op, z->left_val, z->right_val, &rel_fail);
            if (!IS_FAIL_fn(result)) { nd->value = result; return nd->γ; }
            if (!rel_fail) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
            DESCR_t rv = z->right.fn(z->right.ζ, β);
            if (!IS_FAIL_fn(rv)) { z->right_val = rv; continue; }
            DESCR_t lv = z->left.fn(z->left.ζ, β);
            if (IS_FAIL_fn(lv)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
            z->left_val = lv;
            z->right_val = z->right.fn(z->right.ζ, α);
            if (IS_FAIL_fn(z->right_val)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        }
    }
    case IR_ICN_TO_NESTED: {
        icn_to_nested_state_t *z = (icn_to_nested_state_t *)nd->opaque;
        if (!z || z->nlo == 0 || z->nhi == 0) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        if (nd->state == 0) { z->li = 0; z->hi2 = 0; z->cur = z->lo_vals[0]; nd->state = 1; }
        else z->cur++;
        while (z->cur > z->hi_vals[z->hi2]) {
            z->hi2++;
            if (z->hi2 >= z->nhi) { z->hi2 = 0; z->li++; }
            if (z->li >= z->nlo) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
            z->cur = z->lo_vals[z->li];
        }
        nd->value = INTVAL(z->cur);
        return nd->γ;
    }
    case IR_ICN_LIMIT: {
        icn_lim_dcg_t *z = (icn_lim_dcg_t *)nd->opaque;
        if (!z) { nd->value = FAILDESCR; return nd->ω; }
        if (nd->state == 0) z->count = 0;
        nd->state = 1;
        if (z->count >= z->max) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        int tick = (z->count == 0) ? α : β;
        DESCR_t v = z->gen.fn(z->gen.ζ, tick);
        if (IS_FAIL_fn(v)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        z->count++;
        nd->value = v;
        return nd->γ;
    }
    case IR_ICN_EVERY: {
        bb_node_t *gen = (bb_node_t *)nd->opaque;
        if (!gen) { nd->value = FAILDESCR; return nd->ω; }
        int tick = (nd->state == 0) ? α : β;
        nd->state = 1;
        DESCR_t v = gen->fn(gen->ζ, tick);
        if (IS_FAIL_fn(v)) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        icn_every_body_pre();
        if (nd->sval2) bb_exec_stmt((void *)nd->sval2);
        if (icn_every_body_broke()) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        nd->value = v;
        return nd->γ;
    }
    case IR_ICN_PROC_GEN: {
        GeneratorState *gs = (GeneratorState *)nd->opaque;
        if (!gs) { nd->value = FAILDESCR; return nd->ω; }
        DESCR_t v;
        int ok = bb_broker_drive_sm_one(gs, &v);
        if (!ok) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        nd->state = 1;
        nd->value = v;
        return nd->γ;
    }
    case IR_PL_SEQ: {
        /* Prolog conjunction with backtracking pump.                                                */
        /* Forward: run goals left-to-right; on success advance, on failure trigger backtrack.       */
        /* Backtrack: scan leftward for a resumable IR_PL_CALL (state==1) or IR_PL_ALT (state==1).   */
        /*   - If resume yields a new solution: restart forward execution at found+1.                */
        /*   - If resume is exhausted: continue scanning leftward from found-1 (don't restart goal). */
        /*   - If no resumable goal remains to the left: fail the whole SEQ outward.                 */
        extern Trail g_pl_trail; extern Term **g_pl_env; extern int g_pl_cut_flag;
        typedef struct { Term **callee_env; Term **saved_env; int trail_mark; int nslots; } PlCallSt;
        int j = 0;
        int backtrack_from = -1;
        while (j < nd->n) {
            if (backtrack_from < 0) {
                if (!nd->c[j]) { j++; continue; }
                IR_exec_node(nd->c[j]);
                DESCR_t cv = nd->c[j]->value;
                if (!IS_FAIL_fn(cv)) {
                    if (frame_depth > 0 && FRAME.returning) break;
                    j++;
                    continue;
                }
                backtrack_from = j;
            }
            /* Backtrack: search left of backtrack_from for a resumable goal */
            int found = -1;
            for (int k = backtrack_from - 1; k >= 0; k--) {
                IR_t *gk = nd->c[k];
                if (!gk) continue;
                if (gk->t == IR_PL_CALL && gk->state == 1 && gk->opaque) { found = k; break; }
                if (gk->t == IR_PL_ALT && gk->state == 1) { found = k; break; }
            }
            if (found < 0) { nd->value = FAILDESCR; return nd->ω; }
            IR_t *gen = nd->c[found];
            if (gen->t == IR_PL_CALL) {
                PlCallSt *cs = (PlCallSt *)gen->opaque;
                char rkey[128]; snprintf(rkey, sizeof rkey, "%s/%d", gen->sval, (int)gen->ival2);
                Pl_PredEntry_BB *rbb = pl_dcg_lookup(rkey, (int)gen->ival2);
                if (!rbb || !rbb->ir_body) { free(cs); gen->opaque = NULL; gen->state = 0; backtrack_from = found; continue; }
                trail_unwind(&g_pl_trail, cs->trail_mark);
                g_pl_env = cs->callee_env;
                cs->trail_mark = trail_mark(&g_pl_trail);
                DESCR_t res2 = IR_exec_resume(rbb->ir_body);
                if (IS_FAIL_fn(res2)) {
                    g_pl_env = cs->saved_env;
                    free(cs); gen->opaque = NULL; gen->state = 0;
                    backtrack_from = found;
                    continue;
                }
                for (int ai = 0; ai < gen->n && ai < (int)gen->ival2; ai++) {
                    if (!gen->c[ai] || gen->c[ai]->t != IR_PL_VAR) continue;
                    int caller_slot = (int)gen->c[ai]->ival2;
                    Term *shared = cs->callee_env[ai];
                    if (shared && cs->saved_env && caller_slot >= 0) cs->saved_env[caller_slot] = shared;
                }
                g_pl_env = cs->saved_env;
                gen->value = INTVAL(1);
                j = found + 1;
                backtrack_from = -1;
                continue;
            }
            if (gen->t == IR_PL_ALT) {
                gen->state = 2;
                IR_exec_node(gen->c[1]); DESCR_t ra = gen->c[1]->value;
                gen->state = 0;
                if (IS_FAIL_fn(ra)) { backtrack_from = found; continue; }
                gen->value = ra;
                j = found + 1;
                backtrack_from = -1;
                continue;
            }
            nd->value = FAILDESCR; return nd->ω;
        }
        nd->value = INTVAL(1);
        return nd->γ;
    }
    case IR_PL_ALT: {
        extern Trail g_pl_trail; extern Term **g_pl_env;
        if (!nd->c || nd->n < 2) { nd->value = FAILDESCR; return nd->ω; }
        /* state==2 means: skip left, run right directly (called from SEQ backtrack pump) */
        if (nd->state == 2) {
            int mark = trail_mark(&g_pl_trail); Term **saved_env = g_pl_env;
            IR_exec_node(nd->c[1]); DESCR_t r1 = nd->c[1]->value;
            if (!IS_FAIL_fn(r1)) { nd->value = r1; nd->state = 0; return nd->γ; }
            trail_unwind(&g_pl_trail, mark); g_pl_env = saved_env;
            nd->value = FAILDESCR; nd->state = 0; return nd->ω;
        }
        int mark = trail_mark(&g_pl_trail); Term **saved_env = g_pl_env;
        IR_exec_node(nd->c[0]); DESCR_t r0 = nd->c[0]->value;
        if (!IS_FAIL_fn(r0)) { nd->state = 1; nd->value = r0; return nd->γ; }
        trail_unwind(&g_pl_trail, mark); g_pl_env = saved_env;
        IR_exec_node(nd->c[1]); DESCR_t r1 = nd->c[1]->value;
        if (!IS_FAIL_fn(r1)) { nd->state = 0; nd->value = r1; return nd->γ; }
        nd->state = 0; nd->value = FAILDESCR; return nd->ω;
    }
    case IR_PL_CHOICE: {
        /* Stateful multi-clause choice point. nd->state = next clause to try (0=fresh).            */
        /* On first entry state==0 resets and tries from clause 0.                                  */
        /* IR_exec_resume re-enters here with state already advanced by previous successful call.   */
        extern Trail g_pl_trail; extern Term **g_pl_env; extern int g_pl_cut_flag;
        if (!nd->c || nd->n == 0) { nd->value = FAILDESCR; return nd->ω; }
        int saved_cut = g_pl_cut_flag;
        g_pl_cut_flag = 0;
        int start = nd->state;
        for (int ci = start; ci < nd->n; ci++) {
            int mark = trail_mark(&g_pl_trail);
            IR_block_t *body = nd->c[ci] ? (IR_block_t *)nd->c[ci]->opaque : NULL;
            Term **saved_for_retry = g_pl_env;
            DESCR_t res = body ? IR_exec_once(body) : FAILDESCR;
            if (!IS_FAIL_fn(res)) {
                g_pl_cut_flag = saved_cut;
                nd->state = ci + 1;
                nd->counter = (int64_t)mark;
                nd->opaque = (void *)saved_for_retry;
                nd->value = res; return nd->γ;
            }
            if (g_pl_cut_flag) {
                g_pl_cut_flag = saved_cut;
                nd->state = 0; nd->value = FAILDESCR; return nd->ω;
            }
            trail_unwind(&g_pl_trail, mark);
            g_pl_env = saved_for_retry;
        }
        g_pl_cut_flag = saved_cut;
        nd->state = 0; nd->value = FAILDESCR; return nd->ω;
    }
    case IR_PL_CALL: {
        /* Resumable predicate call. state==0: fresh call. state==1: has live callee_env in opaque. */
        extern Term **g_pl_env; extern Trail g_pl_trail;
        const char *callee = nd->sval; int carity = (int)nd->ival2;
        if (!callee) { nd->value = FAILDESCR; return nd->ω; }
        char key[128]; snprintf(key, sizeof key, "%s/%d", callee, carity);
        Pl_PredEntry_BB *bb = pl_dcg_lookup(key, carity);
        if (!bb || !bb->ir_body) { nd->value = FAILDESCR; return nd->ω; }
        typedef struct { Term **callee_env; Term **saved_env; int trail_mark; int nslots; } PlCallSt;
        if (nd->state == 0) {
            /* Fresh call: allocate env, bind args, call once */
            int nslots = carity + 16;
            Term **callee_env = calloc((size_t)nslots, sizeof(Term *));
            Term **saved_env_pre = g_pl_env;
            for (int ai = 0; ai < nd->n && ai < carity; ai++) {
                if (!nd->c[ai]) continue;
                Term *at = term_new_var(ai);
                callee_env[ai] = at;
                /* If caller arg is an IR_PL_VAR, check whether the caller slot is bound; only unify if so. */
                if (nd->c[ai]->t == IR_PL_VAR) {
                    int caller_slot = (int)nd->c[ai]->ival2;
                    Term *cv = (saved_env_pre && caller_slot >= 0) ? saved_env_pre[caller_slot] : NULL;
                    if (cv) {
                        Term *cdt = term_deref(cv);
                        if (cdt && cdt->tag != TERM_VAR) unify(at, cdt, &g_pl_trail);
                    }
                    continue;
                }
                IR_exec_node(nd->c[ai]); DESCR_t av = nd->c[ai]->value;
                if (av.v == DT_I) { Term *vt = term_new_int((long)av.i); unify(at, vt, &g_pl_trail); }
                else if (av.v == DT_S && av.s && av.s[0]) { Term *vt = term_new_atom(prolog_atom_intern(av.s)); unify(at, vt, &g_pl_trail); }
            }
            Term **saved_env = g_pl_env;
            g_pl_env = callee_env;
            int mark = trail_mark(&g_pl_trail);
            IR_reset(bb->ir_body);
            DESCR_t res = IR_exec_once(bb->ir_body);
            if (IS_FAIL_fn(res)) { trail_unwind(&g_pl_trail, mark); g_pl_env = saved_env; free(callee_env); nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
            PlCallSt *cs = malloc(sizeof(PlCallSt));
            cs->callee_env = callee_env; cs->saved_env = saved_env; cs->trail_mark = mark; cs->nslots = nslots;
            nd->opaque = cs;
            nd->state = 1;
            for (int ai = 0; ai < nd->n && ai < carity; ai++) {
                if (!nd->c[ai] || nd->c[ai]->t != IR_PL_VAR) continue;
                int caller_slot = (int)nd->c[ai]->ival2;
                Term *shared = callee_env[ai];
                if (shared && saved_env && caller_slot >= 0) saved_env[caller_slot] = shared;
            }
            g_pl_env = saved_env;
            nd->value = INTVAL(1); return nd->γ;
        }
        /* Resume path: driven by IR_PL_SEQ backtracking pump — should not reach here directly */
        nd->value = FAILDESCR; return nd->ω;
    }
    case IR_PL_CUT: {
        extern int g_pl_cut_flag;
        g_pl_cut_flag = 1;
        nd->value = INTVAL(1); return nd->γ;
    }
    case IR_PL_ATOM: {
        nd->value = nd->sval ? STRVAL(nd->sval) : NULVCL;
        return nd->γ;
    }
    case IR_PL_VAR: {
        extern Term **g_pl_env;
        int slot = (int)nd->ival2;
        if (!g_pl_env || slot < 0) { nd->value = NULVCL; return nd->γ; }
        Term *t = g_pl_env[slot] ? term_deref(g_pl_env[slot]) : NULL;
        if (!t) { nd->value = NULVCL; return nd->γ; }
        if (t->tag == TERM_INT)   { nd->value = INTVAL(t->ival);  return nd->γ; }
        if (t->tag == TERM_FLOAT) { nd->value = REALVAL(t->fval); return nd->γ; }
        if (t->tag == TERM_ATOM)  { const char *nm = prolog_atom_name(t->atom_id); nd->value = nm ? STRVAL(nm) : NULVCL; return nd->γ; }
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_PL_ARITH: {
        if (nd->n < 2 || !nd->c[0] || !nd->c[1]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]); DESCR_t lv = nd->c[0]->value;
        IR_exec_node(nd->c[1]); DESCR_t rv = nd->c[1]->value;
        if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) { nd->value = FAILDESCR; return nd->ω; }
        long li = (lv.v == DT_I) ? (long)lv.i : (long)lv.r;
        long ri = (rv.v == DT_I) ? (long)rv.i : (long)rv.r;
        const char *op = nd->sval ? nd->sval : "+";
        long res = (op[0]=='+') ? li+ri : (op[0]=='-') ? li-ri : (op[0]=='*') ? li*ri : (ri ? li/ri : 0);
        nd->value = INTVAL(res);
        return nd->γ;
    }
    case IR_PL_UNIFY: {
        extern Term **g_pl_env; extern Trail g_pl_trail;
        if (nd->n < 2 || !nd->c[0] || !nd->c[1]) { nd->value = FAILDESCR; return nd->ω; }
        IR_exec_node(nd->c[0]); DESCR_t lv = nd->c[0]->value;
        IR_exec_node(nd->c[1]); DESCR_t rv = nd->c[1]->value;
        Term *lt = NULL, *rt = NULL;
        if (nd->c[0]->t == IR_PL_VAR) {
            int slot = (int)nd->c[0]->ival2;
            lt = (g_pl_env && slot >= 0 && g_pl_env[slot]) ? term_deref(g_pl_env[slot]) : NULL;
            if (!lt) { lt = term_new_var(slot); if (g_pl_env && slot >= 0) g_pl_env[slot] = lt; }
        } else {
            if (lv.v == DT_I)   lt = term_new_int((long)lv.i);
            else if (lv.v == DT_S && lv.s) { int aid = prolog_atom_intern(lv.s); lt = term_new_atom(aid); }
            else lt = term_new_atom(prolog_atom_intern("[]"));
        }
        if (nd->c[1]->t == IR_PL_VAR) {
            int slot = (int)nd->c[1]->ival2;
            rt = (g_pl_env && slot >= 0 && g_pl_env[slot]) ? term_deref(g_pl_env[slot]) : NULL;
            if (!rt) { rt = term_new_var(slot); if (g_pl_env && slot >= 0) g_pl_env[slot] = rt; }
        } else {
            if (rv.v == DT_I)   rt = term_new_int((long)rv.i);
            else if (rv.v == DT_S && rv.s) { int aid = prolog_atom_intern(rv.s); rt = term_new_atom(aid); }
            else rt = term_new_atom(prolog_atom_intern("[]"));
        }
        if (!lt || !rt) { nd->value = FAILDESCR; return nd->ω; }
        int mark = trail_mark(&g_pl_trail);
        if (!unify(lt, rt, &g_pl_trail)) { trail_unwind(&g_pl_trail, mark); nd->value = FAILDESCR; return nd->ω; }
        nd->value = INTVAL(1);
        return nd->γ;
    }
    case IR_PL_BUILTIN: {
        const char *fn = nd->sval ? nd->sval : "";
        if (strcmp(fn, "nl") == 0) { putchar('\n'); nd->value = INTVAL(1); return nd->γ; }
        if (nd->n >= 2 && nd->c[0] && nd->c[1] &&
            (strcmp(fn,">")==0||strcmp(fn,"<")==0||strcmp(fn,">=")==0||strcmp(fn,"<=")==0||strcmp(fn,"=:=")==0||strcmp(fn,"=\\=")==0)) {
            IR_exec_node(nd->c[0]); DESCR_t lv = nd->c[0]->value;
            IR_exec_node(nd->c[1]); DESCR_t rv = nd->c[1]->value;
            double l = (lv.v == DT_I) ? (double)lv.i : lv.r;
            double r = (rv.v == DT_I) ? (double)rv.i : rv.r;
            int ok = (strcmp(fn,">")==0)?(l>r):(strcmp(fn,"<")==0)?(l<r):(strcmp(fn,">=")==0)?(l>=r):(strcmp(fn,"<=")==0)?(l<=r):(strcmp(fn,"=:=")==0)?(l==r):(l!=r);
            if (ok) { nd->value = INTVAL(1); return nd->γ; }
            nd->value = FAILDESCR; return nd->ω;
        }
        if (nd->n >= 1 && nd->c[0]) {
            IR_exec_node(nd->c[0]); DESCR_t av = nd->c[0]->value;
            if (strcmp(fn, "write") == 0 || strcmp(fn, "writeln") == 0) {
                if (av.v == DT_I) printf("%ld", (long)av.i);
                else if (av.v == DT_R) printf("%g", av.r);
                else if ((av.v == DT_S || av.v == DT_SNUL) && av.s) fputs(av.s, stdout);
                if (strcmp(fn, "writeln") == 0) putchar('\n');
                nd->value = INTVAL(1); return nd->γ;
            }
            if (strcmp(fn, "is") == 0 && nd->n >= 2 && nd->c[1]) {
                extern Term **g_pl_env;
                IR_exec_node(nd->c[1]); DESCR_t rv = nd->c[1]->value;
                if (IS_FAIL_fn(rv)) { nd->value = FAILDESCR; return nd->ω; }
                if (nd->c[0]->t == IR_PL_VAR) {
                    int slot = (int)nd->c[0]->ival2;
                    Term *vt = (rv.v == DT_I) ? term_new_int((long)rv.i) : term_new_float(rv.r);
                    if (g_pl_env && slot >= 0) g_pl_env[slot] = vt;
                }
                nd->value = INTVAL(1); return nd->γ;
            }
        }
        nd->value = FAILDESCR; return nd->ω;
    }
    default:
        nd->value = FAILDESCR;
        return nd->ω;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t IR_exec_once(IR_block_t * cfg) {
    if (!cfg || !cfg->entry) return FAILDESCR;
    IR_reset(cfg);
    IR_t * cur = cfg->entry;
    int safety = cfg->n * 64 + 256;
    while (cur && safety-- > 0) {
        IR_t * next = IR_exec_node(cur);
        if (!next) {
            return IS_FAIL_fn(cur->value) ? FAILDESCR : cur->value;
        }
        cur = next;
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t IR_exec_resume(IR_block_t * cfg) {
    if (!cfg || !cfg->entry) return FAILDESCR;
    IR_t * cur = cfg->entry;
    int safety = cfg->n * 64 + 256;
    while (cur && safety-- > 0) {
        IR_t * next = IR_exec_node(cur);
        if (!next) {
            return IS_FAIL_fn(cur->value) ? FAILDESCR : cur->value;
        }
        cur = next;
    }
    return FAILDESCR;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int IR_exec_pump(IR_block_t * cfg, IR_body_fn body_fn, void * ctx) {
    if (!cfg || !cfg->entry) return 0;
    IR_reset(cfg);
    int ticks  = 0;
    int safety = cfg->n * 256 + 1024;
    IR_t * cur = cfg->entry;
    while (cur && safety-- > 0) {
        IR_t * next = IR_exec_node(cur);
        if (!next) {
            if (!IS_FAIL_fn(cur->value)) {
                ticks++;
                if (body_fn && body_fn(cur->value, ctx)) break;
                next = cur->β;
                if (!next) break;
            } else {
                break;
            }
        } else if (next == cur) {
            continue;
        }
        cur = next;
    }
    return ticks;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int IR_exec_pat(IR_block_t *cfg,
                const char *subj_name,
                DESCR_t    *subj_var,
                DESCR_t    *repl,
                int         has_repl)
{
    if (!cfg || !cfg->entry) return 0;
    const char *subj_str = "";
    int         subj_len = 0;
    DESCR_t subj_fetched;
    if (subj_name && *subj_name) {
        subj_fetched = NV_GET_fn(subj_name);
        subj_var     = &subj_fetched;
    }
    if (subj_var) {
        DESCR_t sv = VARVAL_d_fn(*subj_var);
        if (sv.v == DT_S || sv.v == DT_SNUL) {
            subj_str = sv.s ? sv.s : "";
            subj_len = sv.slen ? (int)sv.slen : (int)strlen(subj_str);
        }
    }
    Σ    = subj_str;
    Σlen = subj_len;
    Ω    = subj_len;
    int match_start = -1;
    int match_end   = -1;
    extern int64_t kw_anchor;
    int max_start = kw_anchor ? 0 : Ω;
    for (int start = 0; start <= max_start; start++) {
        Δ = start;
        IR_reset(cfg);
        DESCR_t result = IR_exec_once(cfg);
        if (!IS_FAIL_fn(result)) {
            match_start = start;
            match_end   = Δ;
            break;
        }
    }
    if (match_start < 0) return 0;
    if (!has_repl || !repl) return 1;
    if (!subj_name && !subj_var)        return 0;
    const char *repl_str = "";
    int         repl_len = 0;
    if (repl->v == DT_S && repl->s) {
        repl_str = repl->s;
        repl_len = repl->slen ? (int)repl->slen : (int)strlen(repl->s);
    } else if (repl->v == DT_I) {
        char ibuf[32];
        snprintf(ibuf, sizeof(ibuf), "%lld", (long long)repl->i);
        char *gs = (char *)GC_MALLOC(strlen(ibuf) + 1);
        strcpy(gs, ibuf);
        repl_str = gs;
        repl_len = (int)strlen(gs);
    }
    int   new_len = match_start + repl_len + (subj_len - match_end);
    char *new_s   = (char *)GC_MALLOC((size_t)new_len + 1);
    memcpy(new_s,                          subj_str,                (size_t)match_start);
    memcpy(new_s + match_start,            repl_str,                (size_t)repl_len);
    memcpy(new_s + match_start + repl_len, subj_str + match_end,    (size_t)(subj_len - match_end));
    new_s[new_len] = '\0';
    DESCR_t new_val = { .v = DT_S, .slen = (uint32_t)new_len, .s = new_s };
    if (subj_name && *subj_name) {
        NV_SET_fn(subj_name, new_val);
    } else if (subj_var) {
        *subj_var = new_val;
    }
    return 1;
}
