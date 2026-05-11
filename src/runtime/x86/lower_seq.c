/*
 * lower_seq.c — Sequence, alternation, and operator-synonym handlers (SR-7)
 *
 * AST kinds handled (value context):
 *   AST_VLIST  (v1 | v2 | ...)  SNOBOL4 paren-list / Snocone || disjunction
 *   AST_CAT    concatenation    (pattern if any child is AST_DEFER; else string)
 *   AST_SEQ    sequence         (same dual logic as AST_CAT)
 *   AST_ALT    pattern alternation used as a value
 *   AST_OPSYN  operator synonym call
 *
 * Cross-cutting: AST_CAT/SEQ inspect children for AST_DEFER to decide
 *   pattern vs string context; they call lower_pat_expr when needed.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"

static void lower_vlist(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren == 0) { sm_emit(p, SM_PUSH_NULL); return; }
    if (e->nchildren == 1) { lower_expr(c, e->children[0]); return; }
    int njs = e->nchildren - 1;
    int *jumps = (int *)malloc((size_t)njs * sizeof(int));
    for (int i = 0; i < e->nchildren; i++) {
        lower_expr(c, e->children[i]);
        if (i < e->nchildren - 1) {
            jumps[i] = sm_emit_i(p, SM_JUMP_S, 0);
            sm_emit(p, SM_VOID_POP);
        }
    }
    int done = sm_label(p);
    for (int i = 0; i < njs; i++) sm_patch_jump(p, jumps[i], done);
    free(jumps);
}

static void lower_cat_seq(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    /* Icon & conjunction uses AST_SEQ but is goal-directed, not string concat.
     * When lowering an Icon statement, emit JUMP_F between children so that
     * a failing operand short-circuits the whole conjunction. */
    extern int g_lang;
    if (e->kind == AST_SEQ && g_lang == LANG_ICN) {
        /* Goal-directed conjunction: each non-final child must succeed. */
        if (e->nchildren == 0) { sm_emit(p, SM_PUSH_NULL); return; }
        if (e->nchildren == 1) { lower_expr(c, e->children[0]); return; }
        int njumps = e->nchildren - 1;
        int *fail_jumps = (int *)GC_MALLOC((size_t)njumps * sizeof(int));
        for (int i = 0; i < e->nchildren; i++) {
            lower_expr(c, e->children[i]);
            if (i < e->nchildren - 1) {
                fail_jumps[i] = sm_emit_i(p, SM_JUMP_F, 0); /* -> done with FAILDESCR */
                sm_emit(p, SM_VOID_POP);
            }
        }
        int done_lbl = sm_label(p);
        for (int i = 0; i < njumps; i++) sm_patch_jump(p, fail_jumps[i], done_lbl);
        return;
    }
    /* If any child is AST_DEFER, lower as pattern-context concatenation. */
    int has_defer = 0;
    for (int j = 0; j < e->nchildren; j++) {
        const AST_t *cj = e->children[j];
        if (cj && cj->kind == AST_DEFER) { has_defer = 1; break; }
    }
    if (has_defer) {
        for (int i = 0; i < e->nchildren; i++) lower_pat_expr(c, e->children[i]);
        for (int i = 1; i < e->nchildren; i++) sm_emit(p, SM_PAT_CAT);
    } else {
        for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
        for (int i = 1; i < e->nchildren; i++) sm_emit(p, SM_CONCAT);
    }
}

static void lower_alt(LowerCtx *c, const AST_t *e)
{
    lower_pat_expr(c, e);
}

static void lower_opsyn(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    /* sval is mangled "BIATFN(@)" (op char between parens)
     * or bare "BARFN" / "AROWFN" for unary ops. */
    const char *raw = e->sval ? e->sval : "&";
    const char *op = raw;
    static char op_buf[4];
    const char *lp = strchr(raw, '(');
    if (lp && lp[1] && lp[2] == ')') { op_buf[0] = lp[1]; op_buf[1] = '\0'; op = op_buf; }
    else if (strcmp(raw, "BARFN")  == 0) { op = "|"; }
    else if (strcmp(raw, "AROWFN") == 0) { op = "^"; }
    for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
    sm_emit_si(p, SM_CALL_FN, op, (int64_t)e->nchildren);
}

void lower_seq_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_VLIST] = lower_vlist;
    tbl[AST_CAT]   = lower_cat_seq;
    tbl[AST_SEQ]   = lower_cat_seq;
    tbl[AST_ALT]   = lower_alt;
    tbl[AST_OPSYN] = lower_opsyn;
}
