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
