#include "lower_pl.h"
#include "IR.h"
#include "ast.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* lower_pl_term_node — build one IR_t* for a Prolog term node (argument position).               */
/* TT_ILIT → IR_LIT_I; TT_FLIT → IR_LIT_F; TT_QLIT/TT_FNC(0-arg atom) → IR_PL_ATOM;            */
/* TT_VAR → IR_PL_VAR(slot); arithmetic TT_ADD/SUB/MUL/DIV → IR_PL_ARITH.                        */
/* Returns NULL when the kind is not yet supported.                                                */
static IR_t *lower_pl_term_node(IR_block_t *cfg, const tree_t *e) {
    if (!e) return NULL;
    switch (e->t) {
    case TT_ILIT: {
        IR_t *nd = IR_node_alloc(cfg, IR_LIT_I);
        if (!nd) return NULL;
        nd->ival = e->v.ival;
        nd->γ = NULL; nd->ω = NULL;
        return nd;
    }
    case TT_FLIT: {
        IR_t *nd = IR_node_alloc(cfg, IR_LIT_F);
        if (!nd) return NULL;
        nd->dval = e->v.dval;
        nd->γ = NULL; nd->ω = NULL;
        return nd;
    }
    case TT_QLIT: case TT_NAME: {
        IR_t *nd = IR_node_alloc(cfg, IR_PL_ATOM);
        if (!nd) return NULL;
        nd->sval = e->v.sval;
        nd->γ = NULL; nd->ω = NULL;
        return nd;
    }
    case TT_VAR: {
        IR_t *nd = IR_node_alloc(cfg, IR_PL_VAR);
        if (!nd) return NULL;
        nd->ival = e->v.ival;
        nd->sval = e->v.sval;
        nd->γ = NULL; nd->ω = NULL;
        return nd;
    }
    case TT_ADD: case TT_SUB: case TT_MUL: case TT_DIV: {
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *lhs = lower_pl_term_node(cfg, e->c[0]);
        IR_t *rhs = lower_pl_term_node(cfg, e->c[1]);
        if (!lhs || !rhs) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_PL_ARITH);
        if (!nd) return NULL;
        nd->sval = (e->t == TT_ADD) ? "+" : (e->t == TT_SUB) ? "-" : (e->t == TT_MUL) ? "*" : "/";
        nd->c = malloc(2 * sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = lhs; nd->c[1] = rhs; nd->n = 2;
        nd->γ = NULL; nd->ω = NULL;
        return nd;
    }
    case TT_FNC: {
        if (e->n == 0) {
            IR_t *nd = IR_node_alloc(cfg, IR_PL_ATOM);
            if (!nd) return NULL;
            nd->sval = e->v.sval;
            nd->γ = NULL; nd->ω = NULL;
            return nd;
        }
        return NULL;
    }
    default:
        return NULL;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* lower_pl_stmt_node — build one IR_t* for a Prolog body goal (statement position).              */
/* TT_FNC(write/nl/is) → IR_PL_BUILTIN; TT_UNIFY → IR_PL_UNIFY.                                  */
/* Returns NULL when the kind is not yet supported (caller aborts lower_pl_predicate).             */
static IR_t *lower_pl_stmt_node(IR_block_t *cfg, const tree_t *e) {
    if (!e) return NULL;
    switch (e->t) {
    case TT_UNIFY: {
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *lhs = lower_pl_term_node(cfg, e->c[0]);
        IR_t *rhs = lower_pl_term_node(cfg, e->c[1]);
        if (!lhs || !rhs) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_PL_UNIFY);
        if (!nd) return NULL;
        nd->c = malloc(2 * sizeof(IR_t *));
        if (!nd->c) return NULL;
        nd->c[0] = lhs; nd->c[1] = rhs; nd->n = 2;
        nd->γ = NULL; nd->ω = NULL;
        return nd;
    }
    case TT_FNC: {
        const char *fn = e->v.sval ? e->v.sval : "";
        IR_t *nd = IR_node_alloc(cfg, IR_PL_BUILTIN);
        if (!nd) return NULL;
        nd->sval = fn;
        if (e->n > 0) {
            nd->c = malloc((size_t)e->n * sizeof(IR_t *));
            if (!nd->c) return NULL;
            nd->n = 0;
            for (int i = 0; i < e->n; i++) {
                IR_t *arg = lower_pl_term_node(cfg, e->c[i]);
                if (!arg) return NULL;
                nd->c[nd->n++] = arg;
            }
        }
        nd->γ = NULL; nd->ω = NULL;
        return nd;
    }
    default:
        return NULL;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* lower_pl_predicate — build IR_block_t* for a single Prolog predicate (TT_CHOICE node).        */
/* Iterates over TT_CLAUSE children; for the common single-clause case builds IR_SEQ over body.   */
/* Returns NULL when any goal in the body cannot yet be lowered (caller keeps ir_body=NULL).       */
IR_block_t *lower_pl_predicate(tree_t *choice) {
    if (!choice || choice->t != TT_CHOICE) return NULL;
    if (choice->n != 1) return NULL;
    tree_t *clause = choice->c[0];
    if (!clause || clause->t != TT_CLAUSE) return NULL;
    int n_args = (int)clause->v.dval;
    int n_body = clause->n - n_args;
    if (n_body <= 0) return NULL;
    IR_block_t *cfg = IR_alloc(64, IR_LANG_PL);
    if (!cfg) return NULL;
    IR_t **stmts = calloc((size_t)n_body, sizeof(IR_t *));
    if (!stmts) { IR_free(cfg); return NULL; }
    int built = 0;
    for (int i = 0; i < n_body; i++) {
        tree_t *goal = clause->c[n_args + i];
        if (!goal) continue;
        IR_t *nd = lower_pl_stmt_node(cfg, goal);
        if (!nd) { free(stmts); IR_free(cfg); return NULL; }
        stmts[built++] = nd;
    }
    if (built == 0) { free(stmts); IR_free(cfg); return NULL; }
    IR_t *seq = IR_node_alloc(cfg, IR_SEQ);
    if (!seq) { free(stmts); IR_free(cfg); return NULL; }
    seq->c = stmts; seq->n = built;
    cfg->entry = seq;
    return cfg;
}
