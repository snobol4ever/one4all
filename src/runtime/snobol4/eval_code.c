#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "snobol4.h"
#include "sil_macros.h"
#include "sm_interp.h"
#include "../../frontend/snobol4/scrip_cc.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int exec_stmt(const char  *subj_name,
                          DESCR_t     *subj_var,
                          DESCR_t      pat,
                          DESCR_t     *repl,
                          int          has_repl);
extern const char *Σ;
extern int         Ω;
extern int         Δ;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t eval_node(tree_t *e)
{
    if (!e) return NULVCL;
    switch (e->t) {
    case TT_DEFER:
        if (e->n < 1) return NULVCL;
        {
            DESCR_t d;
            d.v    = DT_E;
            d.slen = 0;
            d.s    = NULL;
            d.ptr  = e->c[0];
            return d;
        }
    case TT_ILIT:
        return INTVAL(e->v.ival);
    case TT_FLIT:
        return REALVAL(e->v.dval);
    case TT_QLIT:
        return e->v.sval ? STRVAL(e->v.sval) : NULVCL;
    case TT_NUL:
        return NULVCL;
    case TT_VAR:
        if (e->v.sval && *e->v.sval)
            return NV_GET_fn(e->v.sval);
        return NULVCL;
    case TT_KEYWORD: {
        if (!e->v.sval || !*e->v.sval) return NULVCL;
        char kbuf[128];
        snprintf(kbuf, sizeof kbuf, "&%s", e->v.sval);
        return NV_GET_fn(kbuf);
    }
    case TT_MNS:
        if (e->n < 1) return FAILDESCR;
        return neg(eval_node(e->c[0]));
    case TT_ADD: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = eval_node(e->c[0]);
        DESCR_t r = eval_node(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return add(l, r);
    }
    case TT_SUB: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = eval_node(e->c[0]);
        DESCR_t r = eval_node(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return sub(l, r);
    }
    case TT_MUL: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = eval_node(e->c[0]);
        DESCR_t r = eval_node(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return mul(l, r);
    }
    case TT_DIV: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = eval_node(e->c[0]);
        DESCR_t r = eval_node(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return DIVIDE_fn(l, r);
    }
    case TT_POW: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t l = eval_node(e->c[0]);
        DESCR_t r = eval_node(e->c[1]);
        if (IS_FAIL_fn(l) || IS_FAIL_fn(r)) return FAILDESCR;
        return POWER_fn(l, r);
    }
    case TT_CAT:
    case TT_SEQ: {
        if (e->n == 0) return NULVCL;
        DESCR_t acc = eval_node(e->c[0]);
        if (IS_FAIL_fn(acc)) return FAILDESCR;
        for (int i = 1; i < e->n; i++) {
            DESCR_t next = eval_node(e->c[i]);
            if (IS_FAIL_fn(next)) return FAILDESCR;
            if (acc.v == DT_P || next.v == DT_P) {
                extern DESCR_t pat_cat(DESCR_t a, DESCR_t b);
                acc = pat_cat(acc, next);
            } else {
                acc = CONCAT_fn(acc, next);
            }
            if (IS_FAIL_fn(acc)) return FAILDESCR;
        }
        return acc;
    }
    case TT_ASSIGN: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t val = eval_node(e->c[1]);
        if (IS_FAIL_fn(val)) return FAILDESCR;
        tree_t *lv = e->c[0];
        if (lv && lv->t == TT_VAR && lv->v.sval)
            NV_SET_fn(lv->v.sval, val);
        else if (lv && lv->t == TT_INDIRECT && lv->n > 0) {
            DESCR_t name_d = eval_node(lv->c[0]);
            const char *nm = VARVAL_fn(name_d);
            if (nm && *nm) {
                char *fn = GC_strdup(nm);
                NV_SET_fn(fn, val);
            }
        }
        return val;
    }
    case TT_INDIRECT: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t name_d = eval_node(e->c[0]);
        const char *nm = VARVAL_fn(name_d);
        if (!nm || !*nm) return NULVCL;
        char *fn = GC_strdup(nm);
        return NV_GET_fn(fn);
    }
    case TT_FNC: {
        if (!e->v.sval || !*e->v.sval) return FAILDESCR;
        int nargs = e->n;
        DESCR_t *args = nargs > 0
            ? (DESCR_t *)alloca((size_t)nargs * sizeof(DESCR_t))
            : NULL;
        for (int i = 0; i < nargs; i++) {
            args[i] = eval_node(e->c[i]);
            if (IS_FAIL_fn(args[i])) return FAILDESCR;
        }
        return APPLY_fn(e->v.sval, args, nargs);
    }
    case TT_IDX: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t base = eval_node(e->c[0]);
        if (IS_FAIL_fn(base)) return FAILDESCR;
        if (e->n == 2) {
            DESCR_t idx = eval_node(e->c[1]);
            if (IS_FAIL_fn(idx)) return FAILDESCR;
            return subscript_get(base, idx);
        } else {
            DESCR_t i1 = eval_node(e->c[1]);
            DESCR_t i2 = eval_node(e->c[2]);
            if (IS_FAIL_fn(i1) || IS_FAIL_fn(i2)) return FAILDESCR;
            return subscript_get2(base, i1, i2);
        }
    }
    case TT_NAME: {
        if (e->n < 1) return FAILDESCR;
        tree_t *child = e->c[0];
        if (!child) return FAILDESCR;
        if (child->t == TT_VAR && child->v.sval)
            return NAME_fn(child->v.sval);
        if (child->t == TT_KEYWORD && child->v.sval) {
            char kbuf[128];
            snprintf(kbuf, sizeof kbuf, "&%s", child->v.sval);
            return NAME_fn(kbuf);
        }
        DESCR_t inner = eval_node(child);
        if (IS_FAIL_fn(inner)) return FAILDESCR;
        const char *nm = VARVAL_fn(inner);
        if (!nm || !*nm) return FAILDESCR;
        return NAME_fn(nm);
    }
    case TT_PLS: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t arg = eval_node(e->c[0]);
        if (IS_FAIL_fn(arg)) return FAILDESCR;
        return APPLY_fn("PLS", &arg, 1);
    }
    case TT_INTERROGATE: {
        if (e->n < 1) return FAILDESCR;
        DESCR_t res = eval_node(e->c[0]);
        if (IS_FAIL_fn(res)) return FAILDESCR;
        return NULVCL;
    }
    case TT_ALT: {
        if (e->n < 1) return NULVCL;
        extern DESCR_t pat_alt(DESCR_t a, DESCR_t b);
        DESCR_t acc = eval_node(e->c[0]);
        if (IS_FAIL_fn(acc)) return FAILDESCR;
        acc = PATVAL_fn(acc);
        if (IS_FAIL_fn(acc)) return FAILDESCR;
        for (int i = 1; i < e->n; i++) {
            DESCR_t rhs = eval_node(e->c[i]);
            if (IS_FAIL_fn(rhs)) return FAILDESCR;
            rhs = PATVAL_fn(rhs);
            if (IS_FAIL_fn(rhs)) return FAILDESCR;
            acc = pat_alt(acc, rhs);
            if (IS_FAIL_fn(acc)) return FAILDESCR;
        }
        return acc;
    }
    case TT_CAPT_COND_ASGN: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t pat  = eval_node(e->c[0]);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        pat = PATVAL_fn(pat);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        DESCR_t name;
        tree_t *tgt = e->c[1];
        tree_t *dtgt = (tgt && tgt->t == TT_DEFER && tgt->n > 0) ? tgt->c[0] : NULL;
        if (dtgt && dtgt->t == TT_FNC && dtgt->v.sval) {
            int nargs = dtgt->n;
            DESCR_t *argv = NULL;
            if (nargs > 0) {
                argv = (DESCR_t *)GC_MALLOC((size_t)nargs * sizeof(DESCR_t));
                for (int i = 0; i < nargs; i++)
                    argv[i] = eval_node(dtgt->c[i]);
            }
            const char *fname = dtgt->v.sval;
            return pat_assign_callcap(pat, fname, argv, nargs);
        } else if (dtgt && dtgt->t == TT_VAR && dtgt->v.sval) {
            name = NAME_fn(dtgt->v.sval);
        } else if (tgt && tgt->t == TT_VAR && tgt->v.sval) {
            name = NAME_fn(tgt->v.sval);
        } else if (tgt && tgt->t == TT_KEYWORD && tgt->v.sval) {
            char kbuf[128];
            snprintf(kbuf, sizeof kbuf, "&%s", tgt->v.sval);
            name = NAME_fn(kbuf);
        } else if (tgt && tgt->t == TT_INDIRECT && tgt->n > 0) {
            tree_t *ic = tgt->c[0];
            const char *nm = NULL;
            if (ic->t == TT_QLIT && ic->v.sval)        nm = ic->v.sval;
            else if (ic->t == TT_VAR  && ic->v.sval) { DESCR_t xv = NV_GET_fn(ic->v.sval); nm = VARVAL_fn(xv); }
            else                                      { DESCR_t nd = eval_node(ic);        nm = VARVAL_fn(nd); }
            if (!nm) return FAILDESCR;
            char *fn = GC_strdup(nm);
            name = NAME_fn(fn);
        } else {
            name = eval_node(tgt);
        }
        if (IS_FAIL_fn(name)) return FAILDESCR;
        return pat_assign_cond(pat, name);
    }
    case TT_CAPT_IMMED_ASGN: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t pat  = eval_node(e->c[0]);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        pat = PATVAL_fn(pat);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        DESCR_t name;
        tree_t *tgt = e->c[1];
        tree_t *dtgt = (tgt && tgt->t == TT_DEFER && tgt->n > 0) ? tgt->c[0] : NULL;
        if (dtgt && dtgt->t == TT_FNC && dtgt->v.sval) {
            int nargs = dtgt->n;
            DESCR_t *argv = NULL;
            if (nargs > 0) {
                argv = (DESCR_t *)GC_MALLOC((size_t)nargs * sizeof(DESCR_t));
                for (int i = 0; i < nargs; i++)
                    argv[i] = eval_node(dtgt->c[i]);
            }
            const char *fname = dtgt->v.sval;
            return pat_assign_callcap_named_imm(pat, fname, argv, nargs, NULL, 0);
        } else if (dtgt && dtgt->t == TT_VAR && dtgt->v.sval) {
            name = NAME_fn(dtgt->v.sval);
        } else if (tgt && tgt->t == TT_VAR && tgt->v.sval) {
            name = NAME_fn(tgt->v.sval);
        } else if (tgt && tgt->t == TT_KEYWORD && tgt->v.sval) {
            char kbuf[128];
            snprintf(kbuf, sizeof kbuf, "&%s", tgt->v.sval);
            name = NAME_fn(kbuf);
        } else if (tgt && tgt->t == TT_INDIRECT && tgt->n > 0) {
            tree_t *ic = tgt->c[0];
            const char *nm = NULL;
            if (ic->t == TT_QLIT && ic->v.sval)        nm = ic->v.sval;
            else if (ic->t == TT_VAR  && ic->v.sval) { DESCR_t xv = NV_GET_fn(ic->v.sval); nm = VARVAL_fn(xv); }
            else                                      { DESCR_t nd = eval_node(ic);        nm = VARVAL_fn(nd); }
            if (!nm) return FAILDESCR;
            char *fn = GC_strdup(nm);
            name = NAME_fn(fn);
        } else {
            name = eval_node(tgt);
        }
        if (IS_FAIL_fn(name)) return FAILDESCR;
        return pat_assign_imm(pat, name);
    }
    case TT_CAPT_CURSOR: {
        if (e->n < 1) return FAILDESCR;
        tree_t *child = e->c[0];
        const char *varname = NULL;
        if (child && child->t == TT_VAR  && child->v.sval) varname = child->v.sval;
        if (child && child->t == TT_NAME && child->n > 0
                && child->c[0] && child->c[0]->v.sval)
            varname = child->c[0]->v.sval;
        if (!varname) return FAILDESCR;
        { extern DESCR_t pat_at_cursor(const char *varname);
          return pat_at_cursor(varname); }
    }
    case TT_SCAN: {
        if (e->n < 2) return FAILDESCR;
        DESCR_t subj = eval_node(e->c[0]);
        if (IS_FAIL_fn(subj)) return FAILDESCR;
        DESCR_t pat  = eval_node(e->c[1]);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        pat = PATVAL_fn(pat);
        if (IS_FAIL_fn(pat)) return FAILDESCR;
        return APPLY_fn("__scan", &subj, 1);
    }
    default:
        return NULVCL;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t eval_expr(const char *src)
{
    if (!src || !*src) return NULVCL;
    tree_t *tree = parse_expr_pat_from_str(src);
    if (!tree) return FAILDESCR;
    return eval_node(tree);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t code(const char *src)
{
    if (!src || !*src) return FAILDESCR;
    extern tree_t *sno_parse_string_ast(const char *src, CODE_t **code_out);
    tree_t *ast = sno_parse_string_ast(src, NULL);
    if (!ast || ast->n == 0) return FAILDESCR;
    DESCR_t d;
    d.v   = DT_C;
    d.ptr = ast;           /* tree_t* stored as generic GC pointer */
    d.slen = 0;
    return d;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const char *exec_code(DESCR_t code_block)
{
    if (code_block.v != DT_C || !code_block.ptr) return NULL;
    const tree_t *prog = (const tree_t *)code_block.ptr;  /* SI-6: TT_PROGRAM* */
    for (int _ci = 0; _ci < prog->n; _ci++) {
        const tree_t *s = prog->c[_ci];
        if (!s) continue;
        if (s->t == TT_END) return "";
        int has_eq = stmt_attr_find(s, ":eq") != NULL;
        tree_t *subject     = stmt_attr_expr(stmt_attr_find(s, ":subj"));
        tree_t *pattern     = stmt_attr_expr(stmt_attr_find(s, ":pat"));
        tree_t *replacement = stmt_attr_expr(stmt_attr_find(s, ":repl"));
        /* PST-SN4-1b mirror: split TT_SCAN/TT_SEQ subject into subject+pattern */
        if (!pattern && subject && subject->t == TT_SCAN && subject->n == 2) {
            pattern = subject->c[1];
            subject = subject->c[0];
        }
        if (!pattern && subject && subject->t == TT_SEQ && subject->n >= 2) {
            tree_t *first = subject->c[0];
            if (first->t == TT_VAR || first->t == TT_KEYWORD || first->t == TT_QLIT || first->t == TT_INDIRECT) {
                int nc = subject->n - 1;
                tree_t *rest;
                if (nc == 1) { rest = subject->c[1]; }
                else { rest = ast_node_new(TT_SEQ); for (int i = 1; i < subject->n; i++) expr_add_child(rest, subject->c[i]); }
                pattern = rest;
                subject = first;
            }
        }
        /* PST-SN4-1c: TT_GOTO_S/F/U children */
        const char *goto_u = goto_node_str(stmt_goto_find(s, TT_GOTO_U));
        const char *goto_s = goto_node_str(stmt_goto_find(s, TT_GOTO_S));
        const char *goto_f = goto_node_str(stmt_goto_find(s, TT_GOTO_F));
        DESCR_t subj_val = NULVCL;
        const char *subj_name = NULL;
        if (subject) {
            if (subject->t == TT_VAR && subject->v.sval) {
                subj_name = subject->v.sval;
                subj_val  = NV_GET_fn(subj_name);
            } else {
                subj_val = eval_node(subject);
            }
        }
        int succeeded = 1;
        if (pattern) {
            DESCR_t pat_d = eval_node(pattern);
            if (IS_FAIL_fn(pat_d)) {
                succeeded = 0;
            } else {
                DESCR_t repl_val;
                int has_repl = 0;
                if (has_eq && replacement) {
                    repl_val = eval_node(replacement);
                    has_repl = !IS_FAIL_fn(repl_val);
                }
                succeeded = exec_stmt(
                    subj_name,
                    subj_name ? NULL : &subj_val,
                    pat_d,
                    has_repl ? &repl_val : NULL,
                    has_repl);
            }
        } else if (has_eq && replacement && subj_name) {
            DESCR_t repl_val = eval_node(replacement);
            if (IS_FAIL_fn(repl_val)) {
                succeeded = 0;
            } else {
                NV_SET_fn(subj_name, repl_val);
                succeeded = 1;
            }
        } else if (subject && !pattern && !has_eq) {
            if (IS_FAIL_fn(subj_val)) succeeded = 0;
        }
        if (goto_u || goto_s || goto_f) {
            if (goto_u && *goto_u) return goto_u;
            if (succeeded && goto_s && *goto_s) return goto_s;
            if (!succeeded && goto_f && *goto_f) return goto_f;
        }
    }
    return "";
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t EXPVAL_fn(DESCR_t expr_d)
{
    if (expr_d.v == DT_E) {
        if (expr_d.slen == 1) {
            int entry_pc = (int)expr_d.i;
            return sm_call_expression(entry_pc);
        }
        if (expr_d.slen == 2) {
            typedef void (*expr_thunk_t)(void);
            expr_thunk_t fn = (expr_thunk_t)(uintptr_t)expr_d.i;
            extern int     rt_vstack_depth(void);
            extern DESCR_t rt_vstack_pop(void);
            int sp0 = rt_vstack_depth();
            __asm__ __volatile__(
                "mov  %%rsp, %%rbx\n\t"
                "and  $-16, %%rsp\n\t"
                "sub  $8, %%rsp\n\t"
                "call *%0\n\t"
                "mov  %%rbx, %%rsp\n\t"
                : : "r"(fn) : "rbx", "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory", "cc");
            int sp1 = rt_vstack_depth();
            if (sp1 > sp0) return rt_vstack_pop();
            return FAILDESCR;
        }
        if (!expr_d.ptr) return FAILDESCR;
        const char *save_Σ = Σ;
        int         save_Ω = Ω;
        int         save_Δ = Δ;
        NAME_ctx_t eval_ctx;
        NAME_ctx_enter(&eval_ctx);
        DESCR_t result = eval_node((tree_t *)expr_d.ptr);
        NAME_ctx_leave();
        Σ = save_Σ;
        Ω = save_Ω;
        Δ = save_Δ;
        return result;
    }
    if (expr_d.v == DT_C) {
        exec_code(expr_d);
        return NULVCL;
    }
    const char *s = VARVAL_fn(expr_d);
    if (!s || !*s) return NULVCL;
    return eval_expr(s);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t CONVE_fn(DESCR_t str_d)
{
    const char *s = VARVAL_fn(str_d);
    if (!s || !*s) return FAILDESCR;
    tree_t *tree = parse_expr_pat_from_str(s);
    if (!tree) return FAILDESCR;
    DESCR_t d;
    d.v    = DT_E;
    d.slen = 0;
    d.s    = NULL;
    d.ptr  = tree;
    return d;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t CODE_fn(DESCR_t str_d)
{
    const char *s = VARVAL_fn(str_d);
    if (!s || !*s) return FAILDESCR;
    return code(s);
}
