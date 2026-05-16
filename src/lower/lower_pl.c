#include "lower_pl.h"
#include "IR.h"
#include "ast.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static IR_t *lower_pl_stmt_node(IR_block_t *cfg, const tree_t *e);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static IR_t *lower_pl_term_node(IR_block_t *cfg, const tree_t *e) {
    if (!e) return NULL;
    switch (e->t) {
    case TT_ILIT: { IR_t *nd = IR_node_alloc(cfg, IR_LIT_I); if (!nd) return NULL; nd->ival = e->v.ival; return nd; }
    case TT_FLIT: { IR_t *nd = IR_node_alloc(cfg, IR_LIT_F); if (!nd) return NULL; nd->dval = e->v.dval; return nd; }
    case TT_QLIT: case TT_NAME: { IR_t *nd = IR_node_alloc(cfg, IR_PL_ATOM); if (!nd) return NULL; nd->sval = e->v.sval; return nd; }
    case TT_VAR:  { IR_t *nd = IR_node_alloc(cfg, IR_PL_VAR);  if (!nd) return NULL; nd->ival2 = e->v.ival; nd->sval = e->v.sval; return nd; }
    case TT_ADD: case TT_SUB: case TT_MUL: case TT_DIV: {
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *lhs = lower_pl_term_node(cfg, e->c[0]); IR_t *rhs = lower_pl_term_node(cfg, e->c[1]);
        if (!lhs || !rhs) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_PL_ARITH); if (!nd) return NULL;
        nd->sval = (e->t==TT_ADD)?"+": (e->t==TT_SUB)?"-": (e->t==TT_MUL)?"*":"/";
        nd->c = malloc(2*sizeof(IR_t*)); if (!nd->c) return NULL;
        nd->c[0] = lhs; nd->c[1] = rhs; nd->n = 2; return nd;
    }
    case TT_FNC:
        if (e->n == 0) { IR_t *nd = IR_node_alloc(cfg, IR_PL_ATOM); if (!nd) return NULL; nd->sval = e->v.sval; return nd; }
        return NULL;
    default: return NULL;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static IR_t *lower_pl_seq(IR_block_t *cfg, const tree_t *e) {
    if (!e) return NULL;
    if (e->t == TT_FNC && e->v.sval && strcmp(e->v.sval, ",") == 0 && e->n >= 2) {
        IR_t *seq = IR_node_alloc(cfg, IR_SEQ); if (!seq) return NULL;
        seq->c = malloc((size_t)e->n * sizeof(IR_t*)); if (!seq->c) return NULL;
        seq->n = 0;
        for (int i = 0; i < e->n; i++) {
            IR_t *ch = lower_pl_seq(cfg, e->c[i]); if (!ch) return NULL;
            seq->c[seq->n++] = ch;
        }
        return seq;
    }
    return lower_pl_stmt_node(cfg, e);
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static IR_t *lower_pl_stmt_node(IR_block_t *cfg, const tree_t *e) {
    if (!e) return NULL;
    if (e->t == TT_GT || e->t == TT_LT || e->t == TT_GE || e->t == TT_LE || e->t == TT_EQ || e->t == TT_NE) {
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *lhs = lower_pl_term_node(cfg, e->c[0]); IR_t *rhs = lower_pl_term_node(cfg, e->c[1]);
        if (!lhs || !rhs) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_PL_BUILTIN); if (!nd) return NULL;
        nd->sval = (e->t==TT_GT)?">": (e->t==TT_LT)?"<": (e->t==TT_GE)?">=": (e->t==TT_LE)?"<=": (e->t==TT_EQ)?"=:=": "=\\=";
        nd->c = malloc(2*sizeof(IR_t*)); if (!nd->c) return NULL;
        nd->c[0] = lhs; nd->c[1] = rhs; nd->n = 2; return nd;
    }
    if (e->t == TT_UNIFY) {
        if (e->n < 2 || !e->c[0] || !e->c[1]) return NULL;
        IR_t *lhs = lower_pl_term_node(cfg, e->c[0]); IR_t *rhs = lower_pl_term_node(cfg, e->c[1]);
        if (!lhs || !rhs) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_PL_UNIFY); if (!nd) return NULL;
        nd->c = malloc(2*sizeof(IR_t*)); if (!nd->c) return NULL;
        nd->c[0] = lhs; nd->c[1] = rhs; nd->n = 2; return nd;
    }
    if (e->t == TT_CUT) { return IR_node_alloc(cfg, IR_PL_CUT); }
    if (e->t != TT_FNC || !e->v.sval) return NULL;
    const char *fn = e->v.sval;
    if (strcmp(fn, ",") == 0 && e->n == 2) return lower_pl_seq(cfg, e);
    if (strcmp(fn, ";") == 0 && e->n == 2) {
        IR_t *a = lower_pl_seq(cfg, e->c[0]); if (!a) return NULL;
        IR_t *b = lower_pl_seq(cfg, e->c[1]); if (!b) return NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_PL_ALT); if (!nd) return NULL;
        nd->c = malloc(2*sizeof(IR_t*)); if (!nd->c) return NULL;
        nd->c[0] = a; nd->c[1] = b; nd->n = 2;
        return nd;
    }
    if (strcmp(fn,"true")==0||strcmp(fn,"otherwise")==0) return IR_node_alloc(cfg, IR_SUCCEED);
    if (strcmp(fn,"fail")==0||strcmp(fn,"false")==0)     return IR_node_alloc(cfg, IR_FAIL);
    if (strcmp(fn,"nl")==0) { IR_t *nd = IR_node_alloc(cfg, IR_PL_BUILTIN); if (!nd) return NULL; nd->sval = fn; nd->n = 0; return nd; }
    if (strcmp(fn,"write")==0||strcmp(fn,"writeln")==0||strcmp(fn,"is")==0
        ||strcmp(fn,">")==0||strcmp(fn,"<")==0||strcmp(fn,">=")==0||strcmp(fn,"<=")==0
        ||strcmp(fn,"=:=")==0||strcmp(fn,"=\\=")==0) {
        IR_t *nd = IR_node_alloc(cfg, IR_PL_BUILTIN); if (!nd) return NULL; nd->sval = fn;
        if (e->n > 0) {
            nd->c = malloc((size_t)e->n*sizeof(IR_t*)); if (!nd->c) return NULL; nd->n = 0;
            for (int i = 0; i < e->n; i++) { IR_t *a = lower_pl_term_node(cfg, e->c[i]); if (!a) return NULL; nd->c[nd->n++] = a; }
        }
        return nd;
    }
    {
        IR_t *nd = IR_node_alloc(cfg, IR_PL_CALL); if (!nd) return NULL;
        nd->sval = fn; nd->ival2 = e->n; nd->n = 0;
        if (e->n > 0) {
            nd->c = malloc((size_t)e->n*sizeof(IR_t*)); if (!nd->c) return NULL;
            for (int i = 0; i < e->n; i++) { IR_t *a = lower_pl_term_node(cfg, e->c[i]); if (!a) return NULL; nd->c[nd->n++] = a; }
        }
        return nd;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* lower_pl_clause_body: build IR_block_t for one clause.                                        */
/* Inserts IR_PL_UNIFY(IR_PL_VAR(i), head_term_i) for each head arg at the top.                 */
/* Callee's env slot i will hold the actual arg value set by the caller before driving.          */
static IR_block_t *lower_pl_clause_body(const tree_t *clause, int n_args) {
    if (!clause || clause->t != TT_CLAUSE) return NULL;
    int n_body = clause->n - n_args;
    int n_total = n_args + (n_body > 0 ? n_body : 1);
    IR_block_t *cfg = IR_alloc(128, IR_LANG_PL); if (!cfg) return NULL;
    IR_t **stmts = calloc((size_t)n_total, sizeof(IR_t*)); if (!stmts) { IR_free(cfg); return NULL; }
    int built = 0;
    for (int i = 0; i < n_args; i++) {
        if (!clause->c[i]) continue;
        IR_t *head_ir = lower_pl_term_node(cfg, clause->c[i]);
        if (!head_ir) { free(stmts); IR_free(cfg); return NULL; }
        IR_t *slot_var = IR_node_alloc(cfg, IR_PL_VAR); if (!slot_var) { free(stmts); IR_free(cfg); return NULL; }
        slot_var->ival2 = i; slot_var->sval = NULL;
        IR_t *nd = IR_node_alloc(cfg, IR_PL_UNIFY); if (!nd) { free(stmts); IR_free(cfg); return NULL; }
        nd->c = malloc(2*sizeof(IR_t*)); if (!nd->c) { free(stmts); IR_free(cfg); return NULL; }
        nd->c[0] = slot_var; nd->c[1] = head_ir; nd->n = 2; stmts[built++] = nd;
    }
    for (int i = 0; i < n_body; i++) {
        tree_t *goal = clause->c[n_args + i]; if (!goal) continue;
        IR_t *nd = lower_pl_stmt_node(cfg, goal); if (!nd) { free(stmts); IR_free(cfg); return NULL; }
        stmts[built++] = nd;
    }
    if (built == 0) { IR_t *nd = IR_node_alloc(cfg, IR_SUCCEED); if (!nd) { free(stmts); IR_free(cfg); return NULL; } stmts[built++] = nd; }
    IR_t *seq = IR_node_alloc(cfg, IR_SEQ); if (!seq) { free(stmts); IR_free(cfg); return NULL; }
    seq->c = stmts; seq->n = built; cfg->entry = seq; return cfg;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t *lower_pl_predicate(tree_t *choice) {
    if (!choice || choice->t != TT_CHOICE || choice->n < 1) return NULL;
    const char *_csl = choice->v.sval ? strrchr(choice->v.sval, '/') : NULL;
    int arity = _csl ? atoi(_csl + 1) : 0;
    if (choice->n == 1) return lower_pl_clause_body(choice->c[0], arity);
    IR_block_t *cfg = IR_alloc(64, IR_LANG_PL); if (!cfg) return NULL;
    IR_t *nd = IR_node_alloc(cfg, IR_PL_CHOICE); if (!nd) { IR_free(cfg); return NULL; }
    nd->opaque = NULL;
    nd->c = malloc((size_t)choice->n * sizeof(IR_t*)); if (!nd->c) { IR_free(cfg); return NULL; }
    nd->n = 0;
    for (int i = 0; i < choice->n; i++) {
        IR_block_t *body = lower_pl_clause_body(choice->c[i], arity);
        if (!body) { IR_free(cfg); return NULL; }
        IR_t *blk = IR_node_alloc(cfg, IR_SUCCEED); if (!blk) { IR_free(cfg); return NULL; }
        blk->opaque = body; nd->c[nd->n++] = blk;
    }
    cfg->entry = nd; return cfg;
}
