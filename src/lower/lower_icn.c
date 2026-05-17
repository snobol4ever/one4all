#include "lower_icn.h"
#include "IR.h"
#include "snobol4.h"
#include "coerce.h"
#include "ast.h"
#include "../frontend/icon/icon_lex.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern int is_suspendable(tree_t *e);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t *lower_icn_upto(const char *cset, const char *hay) {
    if (!cset || !hay) return NULL;
    IR_block_t *cfg = IR_alloc(4, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_ICN_UPTO);
    if (!nd) return NULL;
    nd->sval    = cset;
    nd->sval2   = hay;
    nd->counter = 0;
    nd->α = nd;
    nd->β = nd;
    nd->γ = NULL;
    nd->ω = NULL;
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t *lower_icn_to(int64_t lo, int64_t hi) {
    IR_block_t *cfg = IR_alloc(4, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_ICN_TO);
    if (!nd) return NULL;
    nd->ival    = lo;
    nd->ival2   = hi;
    nd->counter = lo;
    nd->α = nd;
    nd->β = nd;
    nd->γ = NULL;
    nd->ω = NULL;
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t *lower_icn_to_nested(icn_to_nested_state_t *z) {
    if (!z) return NULL;
    IR_block_t *cfg = IR_alloc(4, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_ICN_TO_NESTED);
    if (!nd) return NULL;
    nd->opaque = (void *)z;
    nd->α = nd;
    nd->β = nd;
    nd->γ = NULL;
    nd->ω = NULL;
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t *lower_icn_every(bb_node_t *gen, void *body) {
    if (!gen) return NULL;
    IR_block_t *cfg = IR_alloc(4, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_ICN_EVERY);
    if (!nd) return NULL;
    nd->opaque = (void *)gen;
    nd->sval2  = (const char *)body;
    nd->α = nd;
    nd->β = nd;
    nd->γ = NULL;
    nd->ω = NULL;
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t *lower_icn_to_by(int64_t lo, int64_t hi, int64_t step) {
    IR_block_t *cfg = IR_alloc(4, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_ICN_TO_BY);
    if (!nd) return NULL;
    nd->ival    = lo;
    nd->ival2   = hi;
    nd->ival3   = step ? step : 1;
    nd->counter = lo;
    nd->α = nd;
    nd->β = nd;
    nd->γ = NULL;
    nd->ω = NULL;
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t *lower_icn_iterate(const char *str, int64_t len) {
    if (!str) return NULL;
    IR_block_t *cfg = IR_alloc(4, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_ICN_ITERATE);
    if (!nd) return NULL;
    nd->sval2   = str;
    nd->ival    = len;
    nd->counter = 0;
    nd->α = nd;
    nd->β = nd;
    nd->γ = NULL;
    nd->ω = NULL;
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t *lower_icn_alternate(bb_node_t left, bb_node_t right) {
    IR_block_t *cfg = IR_alloc(4, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_ICN_ALTERNATE);
    if (!nd) return NULL;
    icn_alt_dcg_t *z = calloc(1, sizeof(*z));
    z->gen[0] = left;
    z->gen[1] = right;
    z->which  = 0;
    nd->opaque = (void *)z;
    nd->α = nd;
    nd->β = nd;
    nd->γ = NULL;
    nd->ω = NULL;
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t *lower_icn_limit(bb_node_t gen, int64_t max) {
    IR_block_t *cfg = IR_alloc(4, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_ICN_LIMIT);
    if (!nd) return NULL;
    icn_lim_dcg_t *z = calloc(1, sizeof(*z));
    z->gen   = gen;
    z->max   = max < 0 ? 0 : max;
    z->count = 0;
    nd->opaque = (void *)z;
    nd->α = nd;
    nd->β = nd;
    nd->γ = NULL;
    nd->ω = NULL;
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t *lower_icn_binop(bb_node_t left, bb_node_t right, IcnBinopKind op, int is_relop) {
    IR_block_t *cfg = IR_alloc(4, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_ICN_BINOP);
    if (!nd) return NULL;
    icn_binop_dcg_t *z = calloc(1, sizeof(*z));
    z->left     = left;
    z->right    = right;
    z->op       = op;
    z->is_relop = is_relop;
    nd->opaque = (void *)z;
    nd->α = nd;
    nd->β = nd;
    nd->γ = NULL;
    nd->ω = NULL;
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t icn_binop_apply(IcnBinopKind op, DESCR_t lv, DESCR_t rv, int *rel_fail) {
    *rel_fail = 0;
    if (IS_FAIL_fn(lv) || IS_FAIL_fn(rv)) return FAILDESCR;
    int either_real = (IS_REAL_fn(lv) || IS_REAL_fn(rv));
    double ld = IS_REAL_fn(lv) ? lv.r : (double)(IS_INT_fn(lv) ? lv.i : 0);
    double rd = IS_REAL_fn(rv) ? rv.r : (double)(IS_INT_fn(rv) ? rv.i : 0);
    long   li = IS_INT_fn(lv) ? lv.i : (long)lv.r;
    long   ri = IS_INT_fn(rv) ? rv.i : (long)rv.r;
    DESCR_t real_result;
    switch (op) {
        case ICN_BINOP_ADD: if (either_real) { real_result.v=DT_R; real_result.r=ld+rd; return real_result; } return INTVAL(li + ri);
        case ICN_BINOP_SUB: if (either_real) { real_result.v=DT_R; real_result.r=ld-rd; return real_result; } return INTVAL(li - ri);
        case ICN_BINOP_MUL: if (either_real) { real_result.v=DT_R; real_result.r=ld*rd; return real_result; } return INTVAL(li * ri);
        case ICN_BINOP_DIV: if (either_real) { if (rd == 0.0) return FAILDESCR; real_result.v=DT_R; real_result.r=ld/rd; return real_result; } return ri ? INTVAL(li / ri) : FAILDESCR;
        case ICN_BINOP_MOD: return ri ? INTVAL(li % ri) : FAILDESCR;
        case ICN_BINOP_POW: { double base = either_real ? ld : (double)li; double exp2 = either_real ? rd : (double)ri; double r = pow(base, exp2); real_result.v = DT_R; real_result.r = r; return real_result; }
        case ICN_BINOP_LT: *rel_fail = !(either_real ? ld <  rd : li <  ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_LE: *rel_fail = !(either_real ? ld <= rd : li <= ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_GT: *rel_fail = !(either_real ? ld >  rd : li >  ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_GE: *rel_fail = !(either_real ? ld >= rd : li >= ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_EQ: *rel_fail = !(either_real ? ld == rd : li == ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_NE: *rel_fail = !(either_real ? ld != rd : li != ri); return *rel_fail ? FAILDESCR : rv;
        case ICN_BINOP_CONCAT: {
            DESCR_t ls_d; ls_d = descr_to_str_icn(lv);
            DESCR_t rs_d; rs_d = descr_to_str_icn(rv);
            if (IS_FAIL_fn(ls_d) || IS_FAIL_fn(rs_d)) return FAILDESCR;
            const char *ls = ls_d.s ? ls_d.s : "";
            const char *rs = rs_d.s ? rs_d.s : "";
            size_t ll = ls_d.slen > 0 ? (size_t)ls_d.slen : strlen(ls);
            size_t rl = rs_d.slen > 0 ? (size_t)rs_d.slen : strlen(rs);
            char *buf = GC_malloc(ll + rl + 1);
            memcpy(buf, ls, ll); memcpy(buf + ll, rs, rl); buf[ll + rl] = '\0';
            { DESCR_t r2; r2.v = DT_S; r2.slen = (int)(ll + rl); r2.s = buf; return r2; }
        }
        case ICN_BINOP_SLT: case ICN_BINOP_SLE: case ICN_BINOP_SGT:
        case ICN_BINOP_SGE: case ICN_BINOP_SEQ: case ICN_BINOP_SNE: {
            DESCR_t ls_d = descr_to_str_icn(lv);
            DESCR_t rs_d = descr_to_str_icn(rv);
            const char *ls = (!IS_FAIL_fn(ls_d) && ls_d.s) ? ls_d.s : "";
            const char *rs = (!IS_FAIL_fn(rs_d) && rs_d.s) ? rs_d.s : "";
            int cmp = strcmp(ls, rs);
            int ok;
            switch (op) {
            case ICN_BINOP_SLT: ok = (cmp <  0); break;
            case ICN_BINOP_SLE: ok = (cmp <= 0); break;
            case ICN_BINOP_SGT: ok = (cmp >  0); break;
            case ICN_BINOP_SGE: ok = (cmp >= 0); break;
            case ICN_BINOP_SEQ: ok = (cmp == 0); break;
            case ICN_BINOP_SNE: ok = (cmp != 0); break;
            default:            ok = 0;           break;
            }
            *rel_fail = !ok;
            return ok ? rv : FAILDESCR;
        }
        default: return FAILDESCR;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#include "sm_interp.h"
IR_block_t *lower_icn_proc_gen(GeneratorState *gs) {
    if (!gs) return NULL;
    IR_block_t *cfg = IR_alloc(4, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_ICN_PROC_GEN);
    if (!nd) return NULL;
    nd->opaque = (void *)gs;
    nd->α      = nd;
    nd->β      = nd;
    nd->γ      = NULL;
    nd->ω      = NULL;
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* lower_icn_expr_node — recursively lower a single Icon AST expr to an IR_t* inside cfg.                                                                                                                  */
/* Returns NULL when the expression kind isn't yet supported.  Caller is responsible for falling back.                                                                                                     */
static IR_t *lower_icn_expr_node(IR_block_t *cfg, tree_t *e);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static IR_t *lower_icn_expr_node(IR_block_t *cfg, tree_t *e) {
    if (!cfg || !e) return NULL;
    switch (e->t) {
    case TT_ILIT: {
        IR_t *nd = IR_node_alloc(cfg, IR_LIT_I);
        if (!nd) return NULL;
        nd->ival = e->v.ival;
        return nd;
    }
    case TT_FLIT: {
        IR_t *nd = IR_node_alloc(cfg, IR_LIT_F);
        if (!nd) return NULL;
        nd->dval = e->v.dval;
        return nd;
    }
    case TT_QLIT: {
        IR_t *nd = IR_node_alloc(cfg, IR_LIT_S);
        if (!nd) return NULL;
        nd->sval = e->v.sval ? e->v.sval : "";
        return nd;
    }
    case TT_CSET: {
        /* Icon 'aeiou' cset literal.  Runtime represents csets as strings (one char per member). */
        IR_t *nd = IR_node_alloc(cfg, IR_LIT_S);
        if (!nd) return NULL;
        nd->sval = e->v.sval ? e->v.sval : "";
        return nd;
    }
    case TT_VAR: {
        if (!e->v.sval) return NULL;
        if (e->v.sval[0] == '&') {
            IR_t *nd = IR_node_alloc(cfg, IR_ICN_KEYWORD);
            if (!nd) return NULL;
            nd->sval = e->v.sval;
            return nd;
        }
        IR_t *nd = IR_node_alloc(cfg, IR_VAR);
        if (!nd) return NULL;
        nd->sval = e->v.sval;
        return nd;
    }
    case TT_KEYWORD: {
        if (!e->v.sval) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_ICN_KEYWORD);
        if (!nd) return NULL;
        nd->sval = e->v.sval;
        return nd;
    }
    case TT_SCAN: {
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *subj = lower_icn_expr_node(cfg, e->c[0]);
        if (!subj) return NULL;
        IR_t *body = NULL;
        if (e->n >= 2 && e->c[1]) {
            body = lower_icn_expr_node(cfg, e->c[1]);
            if (!body) return NULL;
        }
        IR_t *nd = IR_node_alloc(cfg, IR_ICN_SCAN);
        if (!nd) return NULL;
        int nc = body ? 2 : 1;
        nd->c = calloc((size_t)nc, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = subj;
        if (body) nd->c[1] = body;
        nd->n = nc;
        return nd;
    }
    case TT_ASSIGN: {
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        if (e->c[0]->t != TT_VAR) return NULL;
        if (e->c[0]->v.sval && e->c[0]->v.sval[0] == '&') return NULL;
        IR_t *lhs = lower_icn_expr_node(cfg, e->c[0]);
        if (!lhs) return NULL;
        IR_t *rhs = lower_icn_expr_node(cfg, e->c[1]);
        if (!rhs) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_ASSIGN);
        if (!nd) return NULL;
        nd->c = calloc(2, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = lhs;
        nd->c[1] = rhs;
        nd->n    = 2;
        return nd;
    }
    case TT_FNC: {
        if (e->n < 1 || !e->c[0] || e->c[0]->t != TT_VAR || !e->c[0]->v.sval) return NULL;
        int nargs = e->n - 1;
        IR_t **args = NULL;
        if (nargs > 0) {
            args = calloc((size_t)nargs, sizeof(IR_t *));
            if (!args) return NULL;
            for (int j = 0; j < nargs; j++) {
                args[j] = lower_icn_expr_node(cfg, e->c[1+j]);
                if (!args[j]) { free(args); return NULL; }
            }
        }
        IR_t *nd = IR_node_alloc(cfg, IR_CALL);
        if (!nd) { if (args) free(args); return NULL; }
        nd->sval = e->c[0]->v.sval;
        nd->c    = args;
        nd->n    = nargs;
        return nd;
    }
    case TT_IF: {
        /* if cond then E1 else E2.  c[0]=cond, c[1]=then (optional), c[2]=else (optional).                                                                                                                 */
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *cond = lower_icn_expr_node(cfg, e->c[0]);
        if (!cond) return NULL;
        IR_t *then_nd = NULL;
        IR_t *else_nd = NULL;
        if (e->n >= 2 && e->c[1]) {
            then_nd = lower_icn_expr_node(cfg, e->c[1]);
            if (!then_nd) return NULL;
        }
        if (e->n >= 3 && e->c[2]) {
            else_nd = lower_icn_expr_node(cfg, e->c[2]);
            if (!else_nd) return NULL;
        }
        IR_t *nd = IR_node_alloc(cfg, IR_IF);
        if (!nd) return NULL;
        int nc = else_nd ? 3 : (then_nd ? 2 : 1);
        nd->c = calloc((size_t)nc, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = cond;
        if (nc >= 2) nd->c[1] = then_nd;
        if (nc >= 3) nd->c[2] = else_nd;
        nd->n = nc;
        return nd;
    }
    case TT_TO: {
        /* Icon `lo to hi` generator. Emit IR_ICN_TO. If both bounds are literal ints, store them            */
        /* directly in ival/ival2. Otherwise recursively lower the bound expressions as c[0]/c[1] —         */
        /* the IR_ICN_TO executor evaluates them on the α path to seed the counter.                         */
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_ICN_TO);
        if (!nd) return NULL;
        if (e->c[0]->t == TT_ILIT && e->c[1]->t == TT_ILIT) {
            nd->ival    = e->c[0]->v.ival;
            nd->ival2   = e->c[1]->v.ival;
            nd->counter = nd->ival;
            nd->state   = 0;
            return nd;
        }
        IR_t *lo = lower_icn_expr_node(cfg, e->c[0]);
        if (!lo) return NULL;
        IR_t *hi = lower_icn_expr_node(cfg, e->c[1]);
        if (!hi) return NULL;
        nd->c = calloc(2, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = lo;
        nd->c[1] = hi;
        nd->n    = 2;
        nd->state = 0;
        return nd;
    }
    case TT_TO_BY: {
        /* Icon `lo to hi by step` generator — IJ-TOBY-REAL: supports integer and real-typed bounds.        */
        /* n=2: lo to hi (step defaults to +1). n=3: lo to hi by step.                                      */
        /* Emit IR_TO_BY with c[0]=lo, c[1]=hi, c[2]=step (if n>=3). The executor reads DT_R bounds.        */
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_TO_BY);
        if (!nd) return NULL;
        int nc = (e->n >= 3 && e->c[2]) ? 3 : 2;
        nd->c = calloc((size_t)nc, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = lower_icn_expr_node(cfg, e->c[0]);
        nd->c[1] = lower_icn_expr_node(cfg, e->c[1]);
        if (!nd->c[0] || !nd->c[1]) return NULL;
        if (nc == 3) {
            nd->c[2] = lower_icn_expr_node(cfg, e->c[2]);
            if (!nd->c[2]) return NULL;
        }
        nd->n     = nc;
        nd->state = 0;
        return nd;
    }
    case TT_EVERY: {
        /* every E [do B].  c[0]=generator expr (required), c[1]=body (optional).                                                                                                                            */
        /* Statement consumer: IR_EVERY drives c[0] to exhaustion in a single outer IR_exec_node call.                                                                                                       */
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *gen = lower_icn_expr_node(cfg, e->c[0]);
        if (!gen) return NULL;
        IR_t *body = NULL;
        if (e->n >= 2 && e->c[1]) {
            body = lower_icn_expr_node(cfg, e->c[1]);
            if (!body) return NULL;
        }
        IR_t *nd = IR_node_alloc(cfg, IR_EVERY);
        if (!nd) return NULL;
        int nc = body ? 2 : 1;
        nd->c = calloc((size_t)nc, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = gen;
        if (body) nd->c[1] = body;
        nd->n = nc;
        return nd;
    }
    case TT_WHILE:
    case TT_UNTIL: {
        /* while C [do B] / until C [do B].  c[0]=cond, c[1]=body (optional).                                                                                                                                */
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *cond = lower_icn_expr_node(cfg, e->c[0]);
        if (!cond) return NULL;
        IR_t *body = NULL;
        if (e->n >= 2 && e->c[1]) {
            body = lower_icn_expr_node(cfg, e->c[1]);
            if (!body) return NULL;
        }
        IR_t *nd = IR_node_alloc(cfg, e->t == TT_WHILE ? IR_WHILE : IR_UNTIL);
        if (!nd) return NULL;
        int nc = body ? 2 : 1;
        nd->c = calloc((size_t)nc, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = cond;
        if (body) nd->c[1] = body;
        nd->n = nc;
        return nd;
    }
    case TT_SEQ_EXPR: {
        /* { stmt1; stmt2; ... } — a compound block, parsed when 2+ statements appear in braces.                                                                                                              */
        /* Lower each child, wrap in IR_SEQ.  Single-statement blocks are unwrapped by the parser, so n>=2 here.                                                                                              */
        if (e->n < 1) return NULL;
        IR_t **stmts = calloc((size_t)e->n, sizeof(IR_t *));
        if (!stmts) return NULL;
        for (int j = 0; j < e->n; j++) {
            if (!e->c[j]) { free(stmts); return NULL; }
            stmts[j] = lower_icn_expr_node(cfg, e->c[j]);
            if (!stmts[j]) { free(stmts); return NULL; }
        }
        IR_t *nd = IR_node_alloc(cfg, IR_SEQ);
        if (!nd) { free(stmts); return NULL; }
        nd->c = stmts;
        nd->n = e->n;
        return nd;
    }
    case TT_ADD: case TT_SUB: case TT_MUL: case TT_DIV: case TT_MOD: case TT_POW:
    case TT_LT:  case TT_LE:  case TT_GT:  case TT_GE:  case TT_EQ:  case TT_NE:
    case TT_CAT: {
        /* Plain or generator-aware binop. If either operand is suspendable (a generator), emit       */
        /* IR_BINOP_GEN which yields the full cross-product; else emit IR_BINOP (single-shot).        */
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *lhs = lower_icn_expr_node(cfg, e->c[0]);
        if (!lhs) return NULL;
        IR_t *rhs = lower_icn_expr_node(cfg, e->c[1]);
        if (!rhs) return NULL;
        int is_gen = is_suspendable(e->c[0]) || is_suspendable(e->c[1]);
        IR_t *nd = IR_node_alloc(cfg, is_gen ? IR_BINOP_GEN : IR_BINOP);
        if (!nd) return NULL;
        nd->c = calloc(2, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = lhs;
        nd->c[1] = rhs;
        nd->n    = 2;
        /* Encode the IcnBinopKind in ival.                                                          */
        IcnBinopKind op = ICN_BINOP_ADD;
        int is_relop = 0;
        switch (e->t) {
        case TT_ADD: op = ICN_BINOP_ADD;    break;
        case TT_SUB: op = ICN_BINOP_SUB;    break;
        case TT_MUL: op = ICN_BINOP_MUL;    break;
        case TT_DIV: op = ICN_BINOP_DIV;    break;
        case TT_MOD: op = ICN_BINOP_MOD;    break;
        case TT_POW: op = ICN_BINOP_POW;    break;
        case TT_LT:  op = ICN_BINOP_LT; is_relop = 1; break;
        case TT_LE:  op = ICN_BINOP_LE; is_relop = 1; break;
        case TT_GT:  op = ICN_BINOP_GT; is_relop = 1; break;
        case TT_GE:  op = ICN_BINOP_GE; is_relop = 1; break;
        case TT_EQ:  op = ICN_BINOP_EQ; is_relop = 1; break;
        case TT_NE:  op = ICN_BINOP_NE; is_relop = 1; break;
        case TT_CAT: op = ICN_BINOP_CONCAT; break;
        default: break;
        }
        nd->ival  = (int64_t)op;
        nd->ival2 = (int64_t)is_relop;
        return nd;
    }
    case TT_GLOBAL:
    case TT_LOCAL:
    case TT_STATIC_DECL:
    case TT_INITIAL: {
        /* `local x;`, `static x;`, `global x;`, `initial expr;` — scope/init declarations.                                                                                                                   */
        /* Scope is built at lower time (proc_table[i].lower_sc); these are no-ops at IR exec time.                                                                                                            */
        /* Emit IR_SUCCEED which returns NULVCL via nd->γ.                                                                                                                                                    */
        IR_t *nd = IR_node_alloc(cfg, IR_SUCCEED);
        if (!nd) return NULL;
        return nd;
    }
    case TT_RETURN: {
        /* Icon return [E]. One optional child c[0]=return expression.                               */
        IR_t *nd = IR_node_alloc(cfg, IR_RETURN);
        if (!nd) return NULL;
        if (e->n >= 1 && e->c[0]) {
            IR_t *retval = lower_icn_expr_node(cfg, e->c[0]);
            if (!retval) return NULL;
            nd->c = calloc(1, sizeof(IR_t *));
            if (!nd->c) return NULL;
            nd->c[0] = retval;
            nd->n = 1;
        }
        return nd;
    }
    case TT_SEQ: {
        /* Icon E1 & E2 conjunction. Both must succeed; if E1 fails whole expr fails; returns E2.   */
        /* Lower as IR_IF(cond=E1, then=E2): succeeds with E2's value only when E1 succeeds.        */
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *e1 = lower_icn_expr_node(cfg, e->c[0]);
        if (!e1) return NULL;
        IR_t *e2 = lower_icn_expr_node(cfg, e->c[1]);
        if (!e2) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_IF);
        if (!nd) return NULL;
        nd->c = calloc(2, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = e1; nd->c[1] = e2; nd->n = 2;
        return nd;
    }
    case TT_SIZE: {
        /* Icon *E — string/list/table size. One child: the expression.                              */
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *inner = lower_icn_expr_node(cfg, e->c[0]);
        if (!inner) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_SIZE);
        if (!nd) return NULL;
        nd->c = calloc(1, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = inner;
        nd->n = 1;
        return nd;
    }
    case TT_IDX: {
        /* Icon s[i] subscript. c[0]=base, c[1]=index. Always 2-arg (icon_parse e_binary).              */
        /* Executor calls subscript_get for type-dispatched access (strings/lists/tables).              */
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *base = lower_icn_expr_node(cfg, e->c[0]);
        if (!base) return NULL;
        IR_t *idx  = lower_icn_expr_node(cfg, e->c[1]);
        if (!idx) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_ICN_IDX);
        if (!nd) return NULL;
        nd->c = calloc(2, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = base;
        nd->c[1] = idx;
        nd->n    = 2;
        return nd;
    }
    case TT_CASE: {
        /* Icon case E of { K1: V1; ...; default: VD }. c[0]=sel, then key/val pairs, opt default. */
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t **children = calloc((size_t)e->n, sizeof(IR_t *));
        if (!children) return NULL;
        for (int j = 0; j < e->n; j++) {
            if (!e->c[j]) { free(children); return NULL; }
            children[j] = lower_icn_expr_node(cfg, e->c[j]);
            if (!children[j]) { free(children); return NULL; }
        }
        IR_t *nd = IR_node_alloc(cfg, IR_CASE);
        if (!nd) { free(children); return NULL; }
        nd->c = children;
        nd->n = e->n;
        return nd;
    }
    case TT_LLT: case TT_LLE: case TT_LGT: case TT_LGE: case TT_LEQ: case TT_LNE: {
        /* Icon string relational operators: <<  <<=  >>  >>=  ==  ~=                               */
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *lhs = lower_icn_expr_node(cfg, e->c[0]);
        if (!lhs) return NULL;
        IR_t *rhs = lower_icn_expr_node(cfg, e->c[1]);
        if (!rhs) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_BINOP);
        if (!nd) return NULL;
        nd->c = calloc(2, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = lhs; nd->c[1] = rhs; nd->n = 2;
        IcnBinopKind op;
        switch (e->t) {
        case TT_LLT: op = ICN_BINOP_SLT; break;
        case TT_LLE: op = ICN_BINOP_SLE; break;
        case TT_LGT: op = ICN_BINOP_SGT; break;
        case TT_LGE: op = ICN_BINOP_SGE; break;
        case TT_LEQ: op = ICN_BINOP_SEQ; break;
        case TT_LNE: op = ICN_BINOP_SNE; break;
        default:     op = ICN_BINOP_SEQ; break;
        }
        nd->ival  = (int64_t)op;
        nd->ival2 = 1;
        return nd;
    }
    case TT_NOT: {
        /* Icon `not E`. Succeeds with &null if E fails; fails if E succeeds. One child c[0]=E.     */
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *inner = lower_icn_expr_node(cfg, e->c[0]);
        if (!inner) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_NOT);
        if (!nd) return NULL;
        nd->c = calloc(1, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = inner;
        nd->n = 1;
        return nd;
    }
    case TT_REPEAT: {
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *body = lower_icn_expr_node(cfg, e->c[0]);
        if (!body) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_REPEAT);
        if (!nd) return NULL;
        nd->c = calloc(1, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = body;
        nd->n = 1;
        return nd;
    }
    case TT_ALTERNATE: {
        /* Icon A|B|C alternation — n-ary.  Lower all arms; emit IR_ALT with nd->c[0..n-1].        */
        if (e->n < 1) return NULL;
        IR_t **arms = calloc((size_t)e->n, sizeof(IR_t *));
        if (!arms) return NULL;
        for (int j = 0; j < e->n; j++) {
            if (!e->c[j]) { free(arms); return NULL; }
            arms[j] = lower_icn_expr_node(cfg, e->c[j]);
            if (!arms[j]) { free(arms); return NULL; }
        }
        IR_t *nd = IR_node_alloc(cfg, IR_ALT);
        if (!nd) { free(arms); return NULL; }
        nd->c = arms;
        nd->n = e->n;
        return nd;
    }
    case TT_AUGOP: {
        /* Icon x +:= rhs etc.  TT_AUGOP: c[0]=lhs (var), c[1]=rhs. v.ival=IcnTkKind of augop.    */
        /* Lower as: tmp = lhs op rhs; lhs := tmp.  Use IR_BINOP + IR_ASSIGN.                      */
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *lhs = lower_icn_expr_node(cfg, e->c[0]);
        if (!lhs) return NULL;
        IR_t *rhs = lower_icn_expr_node(cfg, e->c[1]);
        if (!rhs) return NULL;
        IcnBinopKind op = ICN_BINOP_ADD;
        int is_relop = 0;
        switch ((AugOp_e)e->v.ival) {
        case AUGOP_ADD:    op = ICN_BINOP_ADD;    break;
        case AUGOP_SUB:    op = ICN_BINOP_SUB;    break;
        case AUGOP_MUL:    op = ICN_BINOP_MUL;    break;
        case AUGOP_DIV:    op = ICN_BINOP_DIV;    break;
        case AUGOP_MOD:    op = ICN_BINOP_MOD;    break;
        case AUGOP_POW:    op = ICN_BINOP_POW;    break;
        case AUGOP_CONCAT: op = ICN_BINOP_CONCAT; break;
        case AUGOP_EQ:     op = ICN_BINOP_EQ;  is_relop = 1; break;
        case AUGOP_LT:     op = ICN_BINOP_LT;  is_relop = 1; break;
        case AUGOP_LE:     op = ICN_BINOP_LE;  is_relop = 1; break;
        case AUGOP_GT:     op = ICN_BINOP_GT;  is_relop = 1; break;
        case AUGOP_GE:     op = ICN_BINOP_GE;  is_relop = 1; break;
        case AUGOP_NE:     op = ICN_BINOP_NE;  is_relop = 1; break;
        default:           return NULL;
        }
        IR_t *binop = IR_node_alloc(cfg, IR_BINOP);
        if (!binop) return NULL;
        binop->c = calloc(2, sizeof(IR_t *));
        if (!binop->c) return NULL;
        binop->c[0] = lhs;
        binop->c[1] = rhs;
        binop->n    = 2;
        binop->ival  = (int64_t)op;
        binop->ival2 = (int64_t)is_relop;
        IR_t *asgn = IR_node_alloc(cfg, IR_ASSIGN);
        if (!asgn) return NULL;
        asgn->c = calloc(2, sizeof(IR_t *));
        if (!asgn->c) return NULL;
        /* lhs must be a fresh IR_VAR node pointing to same variable as c[0].                       */
        /* Re-lower e->c[0] to get a writable lvalue ref.                                           */
        IR_t *lhs2 = lower_icn_expr_node(cfg, e->c[0]);
        if (!lhs2) return NULL;
        asgn->c[0] = lhs2;
        asgn->c[1] = binop;
        asgn->n    = 2;
        return asgn;
    }
    case TT_LOOP_BREAK: {
        IR_t *nd = IR_node_alloc(cfg, IR_BREAK);
        return nd;
    }
    case TT_LOOP_NEXT: {
        IR_t *nd = IR_node_alloc(cfg, IR_NEXT);
        return nd;
    }
    case TT_PROC_FAIL: {
        IR_t *nd = IR_node_alloc(cfg, IR_FAIL);
        return nd;
    }
    case TT_IDENTICAL: {
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *lhs = lower_icn_expr_node(cfg, e->c[0]);
        if (!lhs) return NULL;
        IR_t *rhs = lower_icn_expr_node(cfg, e->c[1]);
        if (!rhs) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_IDENTICAL);
        if (!nd) return NULL;
        nd->c = calloc(2, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = lhs;
        nd->c[1] = rhs;
        nd->n    = 2;
        return nd;
    }
    case TT_NONNULL: {
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *inner = lower_icn_expr_node(cfg, e->c[0]);
        if (!inner) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_NONNULL);
        if (!nd) return NULL;
        nd->c = calloc(1, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = inner;
        nd->n    = 1;
        return nd;
    }
    case TT_NULL: {
        if (e->n < 1 || !e->c[0]) {
            IR_t *nd = IR_node_alloc(cfg, IR_LIT_NUL);
            return nd;
        }
        IR_t *inner = lower_icn_expr_node(cfg, e->c[0]);
        if (!inner) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_NULL_TEST);
        if (!nd) return NULL;
        nd->c = calloc(1, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = inner;
        nd->n    = 1;
        return nd;
    }
    case TT_RANDOM: {
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *inner = lower_icn_expr_node(cfg, e->c[0]);
        if (!inner) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_RANDOM);
        if (!nd) return NULL;
        nd->c = calloc(1, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = inner;
        nd->n    = 1;
        return nd;
    }
    case TT_MATCH_UNARY: {
        /* Icon =E unary match.  Lowers to IR_CALL("match", inner) — scan builtin. */
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *inner = lower_icn_expr_node(cfg, e->c[0]);
        if (!inner) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_CALL);
        if (!nd) return NULL;
        nd->sval = "match";
        nd->c = calloc(1, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = inner;
        nd->n    = 1;
        return nd;
    }
    case TT_MNS: {
        /* Icon -E unary minus.  Numeric negation of c[0] via icn_binop_apply(SUB, 0, x).            */
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *inner = lower_icn_expr_node(cfg, e->c[0]);
        if (!inner) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_NEG);
        if (!nd) return NULL;
        nd->c = calloc(1, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = inner;
        nd->n    = 1;
        return nd;
    }
    case TT_PLS: {
        /* Icon +E unary plus.  Numeric coerce of c[0] via icn_binop_apply(ADD, 0, x).               */
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *inner = lower_icn_expr_node(cfg, e->c[0]);
        if (!inner) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_POS);
        if (!nd) return NULL;
        nd->c = calloc(1, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = inner;
        nd->n    = 1;
        return nd;
    }
    case TT_CSET_COMPL: {
        /* Icon ~E cset complement.  Lowered IR node coerces operand to string at runtime and        */
        /* invokes icn_cset_complement against the 256-char universal cset.                          */
        if (e->n < 1 || !e->c[0]) return NULL;
        IR_t *inner = lower_icn_expr_node(cfg, e->c[0]);
        if (!inner) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_CSET_COMPL);
        if (!nd) return NULL;
        nd->c = calloc(1, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = inner;
        nd->n    = 1;
        return nd;
    }
    case TT_CSET_UNION:
    case TT_CSET_DIFF:
    case TT_CSET_INTER: {
        /* Icon cset binops E1++E2 / E1--E2 / E1**E2.  Lower both operands, allocate matching IR     */
        /* node.  Executor coerces operands to strings and merges via icn_cset_{union,diff,inter}.   */
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *lhs = lower_icn_expr_node(cfg, e->c[0]);
        if (!lhs) return NULL;
        IR_t *rhs = lower_icn_expr_node(cfg, e->c[1]);
        if (!rhs) return NULL;
        IR_e kind = (e->t == TT_CSET_UNION) ? IR_CSET_UNION
                  : (e->t == TT_CSET_DIFF)  ? IR_CSET_DIFF
                                            : IR_CSET_INTER;
        IR_t *nd = IR_node_alloc(cfg, kind);
        if (!nd) return NULL;
        nd->c = calloc(2, sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = lhs;
        nd->c[1] = rhs;
        nd->n    = 2;
        return nd;
    }
    default:
        return NULL;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* lower_icn_expr_top — public wrapper.  Build a fresh IR_block_t whose entry is the lowered expr.                                                                                                       */
/* Returns NULL on any unsupported AST node (caller falls back to legacy path).                                                                                                                          */
IR_block_t *lower_icn_expr_top(tree_t *e) {
    if (!e) return NULL;
    IR_block_t *cfg = IR_alloc(64, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = lower_icn_expr_node(cfg, e);
    if (!nd) { IR_free(cfg); return NULL; }
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* lower_icn_proc_body — build IR_block_t* for an Icon procedure body.                                                                                                                                     */
/* Input: the TT_PROC AST node. Children [0]=name-or-formals, [1..nparams]=params, [body_start..] = body statements.                                                                                       */
/* Returns NULL if any statement cannot be lowered yet — caller falls back to legacy SM path.                                                                                                              */
/* The resulting block: IR_SEQ over the body statements; body fails (FAILDESCR) so bb_broker exits after one tick.                                                                                         */
IR_block_t *lower_icn_proc_body(tree_t *proc) {
    if (!proc || proc->t != TT_FNC) return NULL;
    int nparams    = proc->_id;
    int body_start = 1 + nparams;
    int n_stmts    = proc->n - body_start;
    if (n_stmts <= 0) return NULL;
    IR_block_t *cfg = IR_alloc(4096, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t **stmt_nodes = calloc((size_t)n_stmts, sizeof(IR_t *));
    if (!stmt_nodes) { IR_free(cfg); return NULL; }
    int built = 0;
    for (int i = 0; i < n_stmts; i++) {
        tree_t *st = proc->c[body_start + i];
        if (!st) continue;
        IR_t *nd = lower_icn_expr_node(cfg, st);
        if (!nd) { free(stmt_nodes); IR_free(cfg); return NULL; }
        stmt_nodes[built++] = nd;
    }
    IR_t *seq = IR_node_alloc(cfg, IR_SEQ);
    if (!seq) { free(stmt_nodes); IR_free(cfg); return NULL; }
    seq->c = stmt_nodes;
    seq->n = built;
    cfg->entry = seq;
    return cfg;
}
