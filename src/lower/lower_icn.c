#include "lower_icn.h"
#include "IR.h"
#include "snobol4.h"
#include "coerce.h"
#include "ast.h"
#include <string.h>
#include <stdlib.h>
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
    case TT_QLIT: {
        IR_t *nd = IR_node_alloc(cfg, IR_LIT_S);
        if (!nd) return NULL;
        nd->sval = e->v.sval ? e->v.sval : "";
        return nd;
    }
    case TT_VAR: {
        if (!e->v.sval) return NULL;
        if (e->v.sval[0] == '&') return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_VAR);
        if (!nd) return NULL;
        nd->sval = e->v.sval;
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
    default:
        return NULL;
    }
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
    IR_block_t *cfg = IR_alloc(128, IR_LANG_ICN);
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
