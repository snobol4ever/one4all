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
#include "../../runtime/common/scrip_ir.h"
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
        IR_t * chain = sp;
        IR_t ** entries = GC_malloc(t->n * sizeof(IR_t *));
        for (int i = t->n - 1; i >= 0; i--) {
            IR_t * e = build_node(cfg, t->c[i], chain, fp);
            if (!e) return NULL;
            entries[i] = e;
            chain = e;
        }
        /* wire back-edges: child[i+1].fail -> child[i].resume */
        for (int i = 0; i < t->n - 1; i++) {
            IR_t * a = entries[i], * b = entries[i+1];
            if (a && b) b->ω = a->β ? a->β : fp;
        }
        nd = IR_node_alloc(cfg, IR_PAT_CAT);
        nd->α = entries[0];
        nd->β = entries[0] ? entries[0]->β : fp;
        nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_ALT: {
        if (t->n == 0) return fp;
        IR_t * alt_fail = fp;
        IR_t * last = NULL;
        for (int i = t->n - 1; i >= 0; i--) {
            IR_t * e = build_node(cfg, t->c[i], sp, alt_fail);
            if (!e) return NULL;
            last = e; alt_fail = e;
        }
        nd = IR_node_alloc(cfg, IR_PAT_ALT);
        nd->α = last; nd->β = last; nd->γ = sp; nd->ω = fp;
        return nd;
    }
    case TT_CAPT_COND_ASGN: {
        if (t->n < 1) return NULL;
        IR_t * inner = build_node(cfg, t->c[0], sp, fp);
        if (!inner) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_ASSIGN_COND);
        nd->sval = (t->n > 1 && t->c[1] && t->c[1]->v.sval) ? t->c[1]->v.sval : NULL;
        nd->α = inner; nd->β = inner->β;
        return nd;
    }
    case TT_CAPT_IMMED_ASGN: {
        if (t->n < 1) return NULL;
        IR_t * inner = build_node(cfg, t->c[0], sp, fp);
        if (!inner) return NULL;
        nd = IR_node_alloc(cfg, IR_PAT_ASSIGN_IMM);
        nd->sval = (t->n > 1 && t->c[1] && t->c[1]->v.sval) ? t->c[1]->v.sval : NULL;
        nd->α = inner; nd->β = inner->β;
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
