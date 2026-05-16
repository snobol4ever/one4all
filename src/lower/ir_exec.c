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
extern int icn_try_call_builtin_by_name(const char *fn, DESCR_t *args, int nargs, DESCR_t *out);
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
        DESCR_t out = FAILDESCR;
        if (icn_try_call_builtin_by_name(nd->sval, args, nargs, &out)) {
            nd->value = out;
            return IS_FAIL_fn(out) ? nd->ω : nd->γ;
        }
        /* User-defined proc: look up proc_table by name; if ir_body exists, push frame and exec. */
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
            IR_reset(proc_table[_pi].ir_body);
            out = IR_exec_once(proc_table[_pi].ir_body);
            if (frame_depth > 0 && FRAME.returning) { out = g_ir_return_val; FRAME.returning = 0; }
            frame_depth--;
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
        if (nd->n < 1 || !nd->c[0]) { nd->value = NULVCL; return nd->γ; }
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
    case IR_ALT: {
        /* Icon alternation A|B|C... n-ary. On alpha: try children left to right; on beta: */
        /* resume current child; if it exhausts, advance to next child.                     */
        /* nd->counter holds current child index (0..n-1). nd->state: 0=fresh, 1=active.   */
        if (nd->n < 1) { nd->value = FAILDESCR; return nd->ω; }
        if (nd->state == 0) {
            for (int i = 0; i < nd->n; i++) {
                if (!nd->c[i]) continue;
                nd->c[i]->state = 0;
                IR_exec_node(nd->c[i]);
                if (!IS_FAIL_fn(nd->c[i]->value)) {
                    nd->value = nd->c[i]->value;
                    nd->counter = i;
                    nd->state = 1;
                    return nd->γ;
                }
            }
            nd->value = FAILDESCR;
            return nd->ω;
        }
        /* beta: resume current child */
        int ci = (int)nd->counter;
        if (ci < nd->n && nd->c[ci]) {
            IR_exec_node(nd->c[ci]);
            if (!IS_FAIL_fn(nd->c[ci]->value)) { nd->value = nd->c[ci]->value; return nd->γ; }
        }
        /* current exhausted: try next children from fresh */
        for (int i = ci + 1; i < nd->n; i++) {
            if (!nd->c[i]) continue;
            nd->c[i]->state = 0;
            IR_exec_node(nd->c[i]);
            if (!IS_FAIL_fn(nd->c[i]->value)) {
                nd->value = nd->c[i]->value;
                nd->counter = i;
                return nd->γ;
            }
        }
        nd->state = 0;
        nd->value = FAILDESCR;
        return nd->ω;
    }
    case IR_TO_BY: {
        if (nd->state == 0) {
            int64_t from = 0, by = 1;
            if (nd->n > 0 && nd->c[0]) { IR_exec_node(nd->c[0]); from = nd->c[0]->value.i; }
            if (nd->n > 2 && nd->c[2]) { IR_exec_node(nd->c[2]); by   = nd->c[2]->value.i; }
            if (by == 0) by = 1;
            nd->counter = from;
            nd->ival    = by;
            nd->state   = 1;
        }
        if (nd->state == 2) {
            nd->value = FAILDESCR;
            return nd->ω;
        }
        int64_t to_val = 0;
        if (nd->n > 1 && nd->c[1]) { IR_exec_node(nd->c[1]); to_val = nd->c[1]->value.i; }
        int64_t by = nd->ival;
        if (by >= 0 ? nd->counter > to_val : nd->counter < to_val) {
            nd->state = 2;
            nd->value = FAILDESCR;
            return nd->ω;
        }
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
    case IR_SIZE: {
        /* Icon *E — size of string/list/table. One child c[0]=E. Returns integer length.       */
        if (nd->n < 1 || !nd->c[0]) { nd->value = INTVAL(0); return nd->γ; }
        IR_exec_node(nd->c[0]);
        DESCR_t v = nd->c[0]->value;
        if (IS_FAIL_fn(v)) { nd->value = FAILDESCR; return nd->ω; }
        if (IS_INT_fn(v) || IS_REAL_fn(v)) { nd->value = INTVAL(0); return nd->γ; }
        const char *s = VARVAL_fn(v);
        long len = s ? (long)strlen(s) : 0;
        nd->value = INTVAL(len);
        return nd->γ;
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
        if (nd->state == 0) {
            /* α path: if c[0]/c[1] present, evaluate them now to seed ival/ival2 (dynamic bounds). */
            if (nd->n >= 2 && nd->c[0] && nd->c[1]) {
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
        if (nd->counter > nd->ival2) { nd->state = 0; nd->value = FAILDESCR; return nd->ω; }
        nd->value = INTVAL(nd->counter);
        return nd->γ;
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
    case IR_PL_ALT: {
        extern Trail g_pl_trail; extern Term **g_pl_env;
        if (!nd->c || nd->n < 2) { nd->value = FAILDESCR; return nd->ω; }
        int mark = trail_mark(&g_pl_trail); Term **saved_env = g_pl_env;
        IR_exec_node(nd->c[0]); DESCR_t r0 = nd->c[0]->value;
        if (!IS_FAIL_fn(r0)) { nd->value = r0; return nd->γ; }
        trail_unwind(&g_pl_trail, mark); g_pl_env = saved_env;
        IR_exec_node(nd->c[1]); DESCR_t r1 = nd->c[1]->value;
        if (!IS_FAIL_fn(r1)) { nd->value = r1; return nd->γ; }
        nd->value = FAILDESCR; return nd->ω;
    }
    case IR_PL_CHOICE: {
        extern Trail g_pl_trail; extern Term **g_pl_env;
        if (!nd->c || nd->n == 0) { nd->value = FAILDESCR; return nd->ω; }
        for (int ci = 0; ci < nd->n; ci++) {
            int mark = trail_mark(&g_pl_trail);
            IR_block_t *body = nd->c[ci] ? (IR_block_t *)nd->c[ci]->opaque : NULL;
            Term **saved_for_retry = g_pl_env;
            DESCR_t res = body ? IR_exec_once(body) : FAILDESCR;
            if (!IS_FAIL_fn(res)) { nd->value = res; return nd->γ; }
            trail_unwind(&g_pl_trail, mark);
            g_pl_env = saved_for_retry;
        }
        nd->value = FAILDESCR; return nd->ω;
    }
    case IR_PL_CALL: {
        extern Term **g_pl_env; extern Trail g_pl_trail;
        const char *callee = nd->sval; int carity = (int)nd->ival2;
        if (!callee) { nd->value = FAILDESCR; return nd->ω; }
        char key[128]; snprintf(key, sizeof key, "%s/%d", callee, carity);
        Pl_PredEntry_BB *bb = pl_dcg_lookup(key, carity);
        if (!bb || !bb->ir_body) { nd->value = FAILDESCR; return nd->ω; }
        int nslots = carity + 16;
        Term **callee_env = calloc((size_t)nslots, sizeof(Term *));
        for (int ai = 0; ai < nd->n && ai < carity; ai++) {
            if (!nd->c[ai]) continue;
            IR_exec_node(nd->c[ai]); DESCR_t av = nd->c[ai]->value;
            Term *at = term_new_var(ai);
            if (av.v == DT_I) { Term *vt = term_new_int((long)av.i); unify(at, vt, &g_pl_trail); }
            else if ((av.v == DT_S || av.v == DT_SNUL) && av.s) { Term *vt = term_new_atom(prolog_atom_intern(av.s)); unify(at, vt, &g_pl_trail); }
            callee_env[ai] = at;
        }
        Term **saved_env = g_pl_env;
        g_pl_env = callee_env;
        int mark = trail_mark(&g_pl_trail);
        DESCR_t res = IR_exec_once(bb->ir_body);
        if (IS_FAIL_fn(res)) { trail_unwind(&g_pl_trail, mark); g_pl_env = saved_env; free(callee_env); nd->value = FAILDESCR; return nd->ω; }
        for (int ai = 0; ai < nd->n && ai < carity; ai++) {
            if (!nd->c[ai] || nd->c[ai]->t != IR_PL_VAR) continue;
            int caller_slot = (int)nd->c[ai]->ival2;
            Term *bound = callee_env[ai] ? term_deref(callee_env[ai]) : NULL;
            if (bound && saved_env && caller_slot >= 0) saved_env[caller_slot] = bound;
        }
        g_pl_env = saved_env; free(callee_env);
        nd->value = INTVAL(1); return nd->γ;
    }
    case IR_PL_CUT: {
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
            int slot = (int)nd->c[0]->ival;
            lt = (g_pl_env && slot >= 0 && g_pl_env[slot]) ? term_deref(g_pl_env[slot]) : NULL;
            if (!lt) { lt = term_new_var(slot); if (g_pl_env && slot >= 0) g_pl_env[slot] = lt; }
        } else {
            if (lv.v == DT_I)   lt = term_new_int((long)lv.i);
            else if (lv.v == DT_S && lv.s) { int aid = prolog_atom_intern(lv.s); lt = term_new_atom(aid); }
            else lt = term_new_atom(prolog_atom_intern("[]"));
        }
        if (nd->c[1]->t == IR_PL_VAR) {
            int slot = (int)nd->c[1]->ival;
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
