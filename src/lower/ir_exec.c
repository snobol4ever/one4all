#include "ir_exec.h"
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
        int safety = 1000000;
        while (safety-- > 0) {
            IR_exec_node(nd->c[0]);
            DESCR_t v = nd->c[0]->value;
            if (IS_FAIL_fn(v)) break;
            if (nd->n >= 2 && nd->c[1]) IR_exec_node(nd->c[1]);
        }
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_WHILE: {
        /* Icon while C [do B].  c[0]=cond (mandatory), c[1]=optional body.                                                                                                                                  */
        /* Loop: eval cond; if cond fails, break; else eval body; repeat.  Cond's state is reset before each pump via IR_exec_node — for plain scalar conds (BINOP relops) this is correct; for generator   */
        /* conds, the resume-on-next-iteration is the right semantics for while (cond drives one pump per iteration).  Always succeeds with NULVCL after termination.                                       */
        if (nd->n < 1 || !nd->c[0]) { nd->value = NULVCL; return nd->γ; }
        int safety = 1000000;
        while (safety-- > 0) {
            nd->c[0]->state = 0;
            IR_exec_node(nd->c[0]);
            DESCR_t cv = nd->c[0]->value;
            if (IS_FAIL_fn(cv)) break;
            if (nd->n >= 2 && nd->c[1]) IR_exec_node(nd->c[1]);
        }
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_UNTIL: {
        /* Icon until C [do B].  c[0]=cond (mandatory), c[1]=optional body.  Inverse of WHILE: loop while cond FAILS, exit when cond succeeds.                                                                */
        if (nd->n < 1 || !nd->c[0]) { nd->value = NULVCL; return nd->γ; }
        int safety = 1000000;
        while (safety-- > 0) {
            nd->c[0]->state = 0;
            IR_exec_node(nd->c[0]);
            DESCR_t cv = nd->c[0]->value;
            if (!IS_FAIL_fn(cv)) break;
            if (nd->n >= 2 && nd->c[1]) IR_exec_node(nd->c[1]);
        }
        nd->value = NULVCL;
        return nd->γ;
    }
    case IR_REPEAT: {
        /* Icon repeat B. Run body forever; exit when body produces FAIL (break semantics). */
        /* nd->c[0] = body. Safety cap prevents infinite loops in broken programs.          */
        if (nd->n < 1 || !nd->c[0]) { nd->value = NULVCL; return nd->γ; }
        int safety = 1000000;
        while (safety-- > 0) {
            nd->c[0]->state = 0;
            IR_exec_node(nd->c[0]);
            if (IS_FAIL_fn(nd->c[0]->value)) break;
        }
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
