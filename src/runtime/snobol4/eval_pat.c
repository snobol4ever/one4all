#include "snobol4.h"
#include "sil_macros.h"
#include "../ast/ast.h"
#include "../interp/icn_runtime.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern DESCR_t eval_node(tree_t *e);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline DESCR_t NAME_DEREF(DESCR_t d) {
    if (IS_NAME(d)) {
        if (IS_NAMEPTR(d)) return NAME_DEREF_PTR(d);
        if (IS_NAMEVAL(d)) return NV_GET_fn(d.s);
    }
    return d;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t interp_eval_pat(tree_t *e)
{
    NO_AST_WALK_GUARD("interp_eval_pat");
    if (!e) return NULVCL;
    switch (e->t) {
    case TT_SEQ:
    case TT_CAT: {
        if (e->n == 0) return NULVCL;
        DESCR_t acc = interp_eval_pat(e->c[0]);
        if (IS_FAIL_fn(acc)) return FAILDESCR;
        for (int i = 1; i < e->n; i++) {
            DESCR_t nxt = interp_eval_pat(e->c[i]);
            if (IS_FAIL_fn(nxt)) return FAILDESCR;
            acc = pat_cat(acc, nxt);
        }
        return acc;
    }
    case TT_ALT: {
        if (e->n == 0) return pat_epsilon();
        DESCR_t acc = interp_eval_pat(e->c[0]);
        if (IS_FAIL_fn(acc)) return FAILDESCR;
        for (int i = 1; i < e->n; i++) {
            DESCR_t nxt = interp_eval_pat(e->c[i]);
            if (IS_FAIL_fn(nxt)) return FAILDESCR;
            acc = pat_alt(acc, nxt);
        }
        return acc;
    }
    case TT_VLIST: {
        if (e->n == 0) return FAILDESCR;
        for (int i = 0; i < e->n; i++) {
            DESCR_t v = eval_node(e->c[i]);
            if (!IS_FAIL_fn(v)) return v;
        }
        return FAILDESCR;
    }
    case TT_VAR:
        if (e->v.sval && *e->v.sval) {
            if (_is_pat_fnc_name(e->v.sval)) {
                DESCR_t _fr = APPLY_fn(e->v.sval, NULL, 0);
                if (!IS_FAIL_fn(_fr)) return _fr;
            }
            DESCR_t _v = eval_node(e);
            if (_v.v == DT_N) {
                if (_v.slen == 1 && _v.ptr) _v = *(DESCR_t *)_v.ptr;
                else if (_v.slen == 0 && _v.s) _v = NV_GET_fn(_v.s);
                else _v = NULVCL;
            }
            if (_v.v == DT_E && !_v.ptr) return NULVCL;
            if (_v.v == DT_E || _v.v == DT_I || _v.v == DT_R) return PATVAL_fn(_v);
            return _v;
        }
        return NULVCL;
    case TT_DEFER:
        if (e->n < 1) return pat_epsilon();
        {
            tree_t *child = e->c[0];
            if (child->t == TT_FNC && child->v.sval) {
                int na = child->n;
                DESCR_t *av = NULL;
                if (na > 0) {
                    av = GC_malloc(na * sizeof(DESCR_t));
                    for (int i = 0; i < na; i++) {
                        tree_t *arg = child->c[i];
                        if (arg && (arg->t == TT_FNC || arg->t == TT_VAR)) {
                            av[i].v = DT_E;
                            av[i].ptr = arg;
                            av[i].slen = 0;
                        } else {
                            av[i] = eval_node(arg);
                        }
                    }
                }
                if (getenv("ONE4ALL_USERCALL_TRACE")) {
                    fprintf(stderr, "PAT_USER_CALL_BUILD name=%s nargs=%d\n",
                            child->v.sval, na);
                }
                return pat_user_call(child->v.sval, av, na);
            }
            if (child->t == TT_VAR && child->v.sval)
                return pat_ref(child->v.sval);
            if (!_expr_is_pat(child)) {
                DESCR_t d; d.v = DT_E; d.ptr = child; d.slen = 0;
                return d;
            }
            DESCR_t r = interp_eval_pat(child);
            if (IS_NAMEPTR(r)) r = NAME_DEREF_PTR(r);
            return r;
        }
    case TT_ARB:     return pat_arb();
    case TT_REM:     return pat_rem();
    case TT_FAIL:    return pat_fail();
    case TT_SUCCEED: return pat_succeed();
    case TT_FENCE:
        if (e->n > 0) {
            DESCR_t _inner = interp_eval_pat(e->c[0]);
            if (IS_FAIL_fn(_inner)) return FAILDESCR;
            return pat_fence_p(_inner);
        }
        return pat_fence();
    case TT_ABORT:   return pat_abort();
    case TT_BAL:     return pat_bal();
    case TT_POS: {
        if (e->n < 1) return pat_pos(0);
        DESCR_t a = eval_node(e->c[0]);
        return pat_pos((int64_t)(a.v==DT_I ? a.i : (int64_t)(a.v==DT_R ? (int64_t)a.r : 0)));
    }
    case TT_RPOS: {
        if (e->n < 1) return pat_rpos(0);
        DESCR_t a = eval_node(e->c[0]);
        return pat_rpos((int64_t)(a.v==DT_I ? a.i : (int64_t)(a.v==DT_R ? (int64_t)a.r : 0)));
    }
    case TT_TAB: {
        if (e->n < 1) return pat_tab(0);
        DESCR_t a = eval_node(e->c[0]);
        return pat_tab((int64_t)(a.v==DT_I ? a.i : (int64_t)(a.v==DT_R ? (int64_t)a.r : 0)));
    }
    case TT_RTAB: {
        if (e->n < 1) return pat_rtab(0);
        DESCR_t a = eval_node(e->c[0]);
        return pat_rtab((int64_t)(a.v==DT_I ? a.i : (int64_t)(a.v==DT_R ? (int64_t)a.r : 0)));
    }
    case TT_LEN: {
        if (e->n < 1) return pat_len(0);
        DESCR_t a = eval_node(e->c[0]);
        return pat_len((int64_t)(a.v==DT_I ? a.i : (int64_t)(a.v==DT_R ? (int64_t)a.r : 0)));
    }
    case TT_ANY: {
        if (e->n < 1) return pat_any_cs("");
        DESCR_t a = NAME_DEREF(eval_node(e->c[0]));
        const char *s = (a.v==DT_S||a.v==DT_SNUL) && a.s ? a.s : "";
        return pat_any_cs(s);
    }
    case TT_NOTANY: {
        if (e->n < 1) return pat_notany("");
        DESCR_t a = NAME_DEREF(eval_node(e->c[0]));
        const char *s = (a.v==DT_S||a.v==DT_SNUL) && a.s ? a.s : "";
        return pat_notany(s);
    }
    case TT_SPAN: {
        if (e->n < 1) return pat_span("");
        DESCR_t a = NAME_DEREF(eval_node(e->c[0]));
        const char *s = (a.v==DT_S||a.v==DT_SNUL) && a.s ? a.s : "";
        return pat_span(s);
    }
    case TT_BREAK: {
        if (e->n < 1) return pat_break_("");
        DESCR_t a = NAME_DEREF(eval_node(e->c[0]));
        const char *s = (a.v==DT_S||a.v==DT_SNUL) && a.s ? a.s : "";
        return pat_break_(s);
    }
    case TT_BREAKX: {
        extern DESCR_t pat_breakx(const char *);
        if (e->n < 1) return pat_breakx("");
        DESCR_t a = NAME_DEREF(eval_node(e->c[0]));
        const char *s = (a.v==DT_S||a.v==DT_SNUL) && a.s ? a.s : "";
        return pat_breakx(s);
    }
    case TT_ARBNO: {
        if (e->n < 1) return pat_arb();
        DESCR_t inner = interp_eval_pat(e->c[0]);
        return pat_arbno(inner);
    }
    case TT_FNC:
        return eval_node(e);
    default:
        return eval_node(e);
    }
}
