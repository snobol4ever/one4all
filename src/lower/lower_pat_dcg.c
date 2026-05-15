#include "lower_pat_dcg.h"
#include "IR.h"
#include "../ast/ast.h"
#include "snobol4.h"
#include <gc/gc.h>
#include <string.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int count_tree(const tree_t * t) {
    if (!t) return 0;
    int n = 1;
    for (int i = 0; i < t->n; i++) n += count_tree(t->c[i]);
    return n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static IR_block_t * build_node(IR_block_t * cfg, const tree_t * t, IR_t * sp, IR_t * fp);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static IR_block_t * build_node(IR_block_t * cfg, const tree_t * t, IR_t * sp, IR_t * fp) {
    if (!t) return sp;
    IR_t * nd = NULL;
    switch (t->t) {
    case TT_QLIT: {
        nd = IR_node_alloc(cfg, IR_PAT_LIT);
        nd->sval = t->v.sval ? t->v.sval : "";
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_ARB: {
        nd = IR_node_alloc(cfg, IR_PAT_ARB);
        nd->α = nd; nd->β = nd; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_REM: {
        nd = IR_node_alloc(cfg, IR_PAT_REM);
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_ABORT: {
        nd = IR_node_alloc(cfg, IR_PAT_ABORT);
        nd->α = nd; nd->β = fp; nd->γ = fp; nd->ω = fp;
        return nd;
    }
    case TT_SPAN: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_QLIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_SPAN);
        nd->sval = t->c[0]->v.sval ? t->c[0]->v.sval : "";
        nd->α = nd; nd->β = nd; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_ANY: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_QLIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_ANY);
        nd->sval = t->c[0]->v.sval ? t->c[0]->v.sval : "";
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_BREAK: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_QLIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_BREAK);
        nd->sval = t->c[0]->v.sval ? t->c[0]->v.sval : "";
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_FENCE: {
        IR_t * inner = (t->n > 0 && t->c[0]) ? build_node(cfg, t->c[0], sp, fp) : sp;
        if (t->n > 0 && !inner) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_FENCE);
        nd->α = nd; nd->β = fp;
        nd->γ = inner ? inner : sp; nd->ω = fp;
        return nd;
    }
    case TT_ARBNO: {
        if (t->n < 1 || !t->c[0]) return NULL;
        int inner_cap = count_tree(t->c[0]) * 8 + 16;
        IR_block_t * inner_blk = IR_alloc(inner_cap, IR_LANG_SNO);
        if (!inner_blk) return NULL;
        IR_t * inner_entry = build_node(inner_blk, t->c[0], NULL, NULL);
        if (!inner_entry) { IR_free(inner_blk); return NULL; }
        inner_blk->entry = inner_entry;
        nd = IR_node_alloc(cfg, IR_PAT_ARBNO);
        int stack_cap = 64;
        int * pos_stack = (int *)GC_MALLOC((size_t)stack_cap * sizeof(int));
        void ** storage = (void **)GC_MALLOC(2 * sizeof(void *));
        storage[0] = inner_blk;
        storage[1] = pos_stack;
        nd->c = (IR_t **)storage;
        nd->n = stack_cap;
        nd->α = nd; nd->β = nd; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_SEQ:
    case TT_CAT: {
        if (t->n == 0) return sp;
        if (t->n == 1) return build_node(cfg, t->c[0], sp, fp);
        IR_t * chain = sp;
        IR_t ** entries = (IR_t **)GC_malloc(t->n * sizeof(IR_t *));
        for (int i = t->n - 1; i >= 0; i--) {
            IR_t * e = build_node(cfg, t->c[i], chain, fp);
            if (!e) return NULL;
            entries[i] = e;
            chain = e;
        }
        for (int i = 0; i < t->n - 1; i++) {
            IR_t * a = entries[i], * b = entries[i+1];
            if (a && b && b->ω == fp) b->ω = a->β ? a->β : fp;
        }
        return entries[0];
    }
    case TT_ALT: {
        if (t->n == 0) return fp;
        if (t->n == 1) return build_node(cfg, t->c[0], sp, fp);
        IR_t * alt_fail = fp;
        IR_t * first    = NULL;
        for (int i = t->n - 1; i >= 0; i--) {
            IR_t * e = build_node(cfg, t->c[i], sp, alt_fail);
            if (!e) return NULL;
            first    = e;
            alt_fail = e;
        }
        return first;
    }
    case TT_CAPT_COND_ASGN: {
        if (t->n < 1) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_ASSIGN_COND);
        nd->sval = (t->n > 1 && t->c[1] && t->c[1]->v.sval) ? t->c[1]->v.sval : NULL;
        nd->γ = sp;
        nd->ω = fp;
        IR_t * inner = build_node(cfg, t->c[0], nd, fp);
        if (!inner) return NULL;
        nd->α = inner;
        nd->β = inner->β;
        return nd;
    }
    case TT_CAPT_IMMED_ASGN: {
        if (t->n < 1) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_ASSIGN_IMM);
        nd->sval = (t->n > 1 && t->c[1] && t->c[1]->v.sval) ? t->c[1]->v.sval : NULL;
        nd->γ = sp;
        nd->ω = fp;
        IR_t * inner = build_node(cfg, t->c[0], nd, fp);
        if (!inner) return NULL;
        nd->α = inner;
        nd->β = inner->β;
        return nd;
    }
    case TT_LEN: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_ILIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_LEN);
        nd->ival = t->c[0]->v.ival;
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_NOTANY: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_QLIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_NOTANY);
        nd->sval = t->c[0]->v.sval ? t->c[0]->v.sval : "";
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_POS: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_ILIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_POS);
        nd->ival = t->c[0]->v.ival;
        nd->n    = 0;
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_RPOS: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_ILIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_POS);
        nd->ival = t->c[0]->v.ival;
        nd->n    = 1;
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_TAB: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_ILIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_TAB);
        nd->ival = t->c[0]->v.ival;
        nd->n    = 0;
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_RTAB: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_ILIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_TAB);
        nd->ival = t->c[0]->v.ival;
        nd->n    = 1;
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    default:
        return NULL;
    }
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
IR_block_t * IR_lower_pat(const tree_t * pat_tree) {
    if (!pat_tree) return NULL;
    int cap = count_tree(pat_tree) * 8 + 32;
    IR_block_t * cfg = IR_alloc(cap, IR_LANG_SNO);
    if (!cfg) return NULL;
    IR_t * entry = build_node(cfg, pat_tree, NULL, NULL);
    if (!entry) { IR_free(cfg); return NULL; }
    cfg->entry = entry;
    return cfg;
}
