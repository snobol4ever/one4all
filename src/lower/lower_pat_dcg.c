/*
 * lower_pat_dcg.c -- build IR_prog_t (DCG) from SNOBOL4 pattern tree_t (LR-S1)
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6 (LR-S1, 2026-05-14)
 *
 * Additive: called from lower.c after lower_pat_expr() to also build the DCG.
 * Existing SM_PAT_* / bb_node_t path is UNCHANGED -- this is a parallel compile-time
 * wiring pass from tree_t*. On success, caller stores IR_prog_t* in SM_EXEC_STMT a[2].ptr.
 * exec_stmt() checks a[2].ptr and routes through IR_exec_once() when set.
 *
 * Phase 1 (LR-S1): TT_QLIT, TT_CAT, TT_ALT, TT_ARB, TT_SPAN, TT_ANY,
 * TT_BREAK, TT_REM, TT_FENCE, TT_ABORT, TT_CAPT_COND_ASGN, TT_CAPT_IMMED_ASGN.
 * Unimplemented tree kinds: return NULL (fall through to existing bb_node_t path).
 */
#include "lower_pat_dcg.h"
#include "scrip_ir.h"
#include "../ast/ast.h"
#include "snobol4.h"
#include <gc/gc.h>
#include <string.h>
static int count_tree(const tree_t * t) {
    if (!t) return 0;
    int n = 1;
    for (int i = 0; i < t->n; i++) n += count_tree(t->c[i]);
    return n;
}
static IR_prog_t * build_node(IR_prog_t * cfg, const tree_t * t, IR_t * sp, IR_t * fp);
static IR_prog_t * build_node(IR_prog_t * cfg, const tree_t * t, IR_t * sp, IR_t * fp) {
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
    case TT_SEQ:
    case TT_CAT: {
        if (t->n == 0) return sp;
        if (t->n == 1) return build_node(cfg, t->c[0], sp, fp);
        /* Build children right-to-left; each child's sp = next child's entry. */
        IR_t * chain = sp;
        IR_t ** entries = (IR_t **)GC_malloc(t->n * sizeof(IR_t *));
        for (int i = t->n - 1; i >= 0; i--) {
            IR_t * e = build_node(cfg, t->c[i], chain, fp);
            if (!e) return NULL;
            entries[i] = e;
            chain = e;
        }
        /* wire back-edges for generative nodes: child[i+1].ω → child[i].β */
        for (int i = 0; i < t->n - 1; i++) {
            IR_t * a = entries[i], * b = entries[i+1];
            if (a && b && b->ω == fp) b->ω = a->β ? a->β : fp;
        }
        /* entry is the first child — no wrapper node needed */
        return entries[0];
    }
    case TT_ALT: {
        if (t->n == 0) return fp;
        if (t->n == 1) return build_node(cfg, t->c[0], sp, fp);
        /* Build alternatives right-to-left; each alt's fp = next alt's entry. */
        IR_t * alt_fail = fp;
        IR_t * first    = NULL;
        for (int i = t->n - 1; i >= 0; i--) {
            IR_t * e = build_node(cfg, t->c[i], sp, alt_fail);
            if (!e) return NULL;
            first    = e;
            alt_fail = e;
        }
        return first;   /* no wrapper needed — first alt is the entry */
    }
    case TT_CAPT_COND_ASGN: {
        /* P $ V — conditional capture: assign on final pattern success.
         * nd is the intercept node: inner runs with sp=nd; when inner
         * succeeds it routes to nd (state 1) which does assignment then
         * routes to the real sp. */
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
        /* P . V — immediate capture: assign on every inner success. */
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
        /* LEN(n): child 0 must be TT_ILIT */
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_ILIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_LEN);
        nd->ival = t->c[0]->v.ival;   /* n */
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_NOTANY: {
        /* NOTANY(cset): child 0 must be TT_QLIT */
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_QLIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_NOTANY);
        nd->sval = t->c[0]->v.sval ? t->c[0]->v.sval : "";
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_POS: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_ILIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_POS);
        nd->ival = t->c[0]->v.ival;   /* n; sval=NULL → POS (left) */
        nd->sval = NULL;
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_RPOS: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_ILIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_POS);
        nd->ival = t->c[0]->v.ival;
        nd->sval = "R";   /* flag: RPOS */
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_TAB: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_ILIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_TAB);
        nd->ival = t->c[0]->v.ival;   /* n; sval=NULL → TAB (left) */
        nd->sval = NULL;
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_RTAB: {
        if (t->n < 1 || !t->c[0] || t->c[0]->t != TT_ILIT) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_TAB);
        nd->ival = t->c[0]->v.ival;
        nd->sval = "R";   /* flag: RTAB */
        nd->α = nd; nd->β = fp; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    default:
        return NULL;   /* unsupported -- fall back to bb_node_t path */
    }
}
IR_prog_t * IR_lower_pat(const tree_t * pat_tree) {
    if (!pat_tree) return NULL;
    int cap = count_tree(pat_tree) * 8 + 32;
    IR_prog_t * cfg = IR_alloc(cap, IR_LANG_SNO);
    if (!cfg) return NULL;
    IR_t * entry = build_node(cfg, pat_tree, NULL, NULL);
    if (!entry) { IR_free(cfg); return NULL; }
    cfg->entry = entry;
    return cfg;
}
