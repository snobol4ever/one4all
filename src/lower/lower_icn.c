/*
 * lower_icn.c -- build IR_block_t DCG for Icon generator expressions (IJ-19-lower)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 *
 * Reference: .github/jcon_irgen.icn ir_a_* for four-port wiring.
 * Reference: git show HEAD~1:src/runtime/interp/icon_gen.c for alpha/beta logic.
 */
#include "lower_icn.h"
#include "scrip_ir.h"
#include "snobol4.h"
#include "coerce.h"
#include <string.h>

/*
 * lower_icn_upto — DCG for upto(cset, str) with compile-time-constant scalar args.
 *
 * Single generator node; four-port wiring (jcon_irgen.icn ir_a_Call scalar case):
 *   α / β → same node (re-entry advances counter)
 *   γ     → success (value = 1-based position)
 *   ω     → failure
 *
 * IR_t fields used:
 *   sval      = cset (compile-time string)
 *   value.s   = hay  (compile-time string)
 *   value.slen = strlen(hay)
 *   counter   = current scan pos (0-based); reset to 0 at α, advanced at β
 */
IR_block_t *lower_icn_upto(const char *cset, const char *hay) {
    if (!cset || !hay) return NULL;
    IR_block_t *cfg = IR_alloc(4, IR_LANG_ICN);
    if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_ICN_UPTO);
    if (!nd) return NULL;
    nd->sval    = cset;
    nd->sval2   = hay;
    nd->counter = 0;
    nd->α = nd;   /* re-enter on α (fresh) and β (advance) — executor distinguishes via state */
    nd->β = nd;
    nd->γ = NULL; /* wired by caller / executor to success continuation */
    nd->ω = NULL; /* wired by caller / executor to fail continuation    */
    cfg->entry = nd;
    return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* lower_icn_to — DCG for (lo to hi) integer range generator.
 * Single node; ival=lo, ival2=hi; counter=cur; state 0=α(fresh), 1=β(advance). */
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
/* lower_icn_to_nested — DCG for (lo_gen to hi_gen) cross-product range generator.
 * opaque=icn_to_nested_state_t* (pre-populated with lo_vals/hi_vals arrays). */
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
/* lower_icn_every — DCG for (every gen [do body]).
 * Single node; opaque=bb_node_t* gen box; sval2=tree_t* body (may be NULL).
 * state 0=α(pump gen fresh), 1=β(pump gen next). */
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
/* lower_icn_to_by — DCG for (lo to hi by step) integer range generator.
 * Single node; ival=lo, ival2=hi, ival3=step; counter=cur; state 0=α, 1=β. */
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
/* lower_icn_iterate — DCG for (!str) char-by-char iteration.
 * Single node; sval2=str (stable), ival=len; counter=pos; state 0=α, 1=β. */
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
/* lower_icn_alternate — DCG for (A|B) alternate generator.
 * Single node; opaque=icn_alt_dcg_t*{gen[2],which}.
 * state 0=α(fresh): try left α then right α. state 1=β: pump current; fallthrough to right. */
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
/* lower_icn_limit — DCG for (gen \ N) limit generator.
 * Single node; opaque=icn_lim_dcg_t*{gen,max,count}; yields up to max ticks from gen. */
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
/* lower_icn_binop — DCG for arith/relop with generative operands.
 * opaque=icn_binop_dcg_t*{left,right,op,is_relop,left_val,right_val,phase}. */
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
/* icn_binop_apply — apply one arithmetic/relational op; set *rel_fail on comparison failure. */
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
