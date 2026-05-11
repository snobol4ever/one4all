/*
 * lower_icn_ctrl.c — Icon control-flow expression handlers (SR-10)
 *
 * AST kinds handled:
 *   AST_SEQ_EXPR   sequence of expressions; last value is result
 *   AST_IF         if/then/else
 *   AST_WHILE      while condition do body
 *   AST_UNTIL      until condition do body
 *   AST_REPEAT     repeat body (infinite loop)
 *   AST_LOOP_BREAK break [expr] — exit innermost loop
 *   AST_LOOP_NEXT  next — continue to next iteration
 *   AST_RETURN     return [expr] from procedure
 *   AST_PROC_FAIL  fail — procedure failure return
 *   AST_CASE       case E of { ... } — multi-arm dispatch
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"
#include "sm_interp.h"

/* ── AST_SEQ_EXPR ────────────────────────────────────────────── */

static void lower_seq_expr(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren == 0) { sm_emit(p, SM_PUSH_NULL); return; }
    for (int i = 0; i < e->nchildren; i++) {
        lower_expr(c, e->children[i]);
        if (i < e->nchildren - 1) sm_emit(p, SM_VOID_POP);
    }
}

/* ── AST_IF ──────────────────────────────────────────────────── */

static void lower_if(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(c, e->children[0]);              /* condition */
    int jf = sm_emit_i(p, SM_JUMP_F, 0);       /* jump-if-fail to else */
    /* Condition result left on stack; drain before entering then-body. */
    sm_emit(p, SM_VOID_POP);
    if (e->nchildren > 1) lower_expr(c, e->children[1]);
    else                  sm_emit(p, SM_PUSH_NULL);
    int jend = sm_emit_i(p, SM_JUMP, 0);
    int else_lbl = sm_label(p);
    sm_patch_jump(p, jf, else_lbl);
    /* Drain condition FAILDESCR on the else path too. */
    sm_emit(p, SM_VOID_POP);
    if (e->nchildren > 2) lower_expr(c, e->children[2]);
    else                  sm_emit(p, SM_PUSH_NULL);
    int end_lbl = sm_label(p);
    sm_patch_jump(p, jend, end_lbl);
}

/* ── AST_WHILE ───────────────────────────────────────────────── */

static void lower_while(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    int top_lbl = sm_label(p);
    if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(c, e->children[0]);
    int jf = sm_emit_i(p, SM_JUMP_F, 0);
    sm_emit(p, SM_VOID_POP);
    if (e->nchildren > 1) { lower_expr(c, e->children[1]); sm_emit(p, SM_VOID_POP); }
    sm_emit_i(p, SM_JUMP, top_lbl);
    int end_lbl = sm_label(p);
    sm_patch_jump(p, jf, end_lbl);
    sm_emit(p, SM_VOID_POP);   /* FAILDESCR left on stack by JUMP_F */
    sm_emit(p, SM_PUSH_NULL);
}

/* ── AST_UNTIL ───────────────────────────────────────────────── */

static void lower_until(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    int top_lbl = sm_label(p);
    if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
    lower_expr(c, e->children[0]);
    int js = sm_emit_i(p, SM_JUMP_S, 0);
    sm_emit(p, SM_VOID_POP);
    if (e->nchildren > 1) { lower_expr(c, e->children[1]); sm_emit(p, SM_VOID_POP); }
    sm_emit_i(p, SM_JUMP, top_lbl);
    int end_lbl = sm_label(p);
    sm_patch_jump(p, js, end_lbl);
    sm_emit(p, SM_VOID_POP);
    sm_emit(p, SM_PUSH_NULL);
}

/* ── AST_REPEAT ──────────────────────────────────────────────── */

static void lower_repeat(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    int top_lbl = sm_label(p);
    if (e->nchildren > 0) { lower_expr(c, e->children[0]); sm_emit(p, SM_VOID_POP); }
    sm_emit_i(p, SM_JUMP, top_lbl);
    sm_emit(p, SM_PUSH_NULL);
}

/* ── AST_LOOP_BREAK ──────────────────────────────────────────── */

static void lower_loop_break(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren > 0) lower_expr(c, e->children[0]);
    else sm_emit(p, SM_PUSH_NULL);
    /* SM_JUMP to self+1 signals break to the sm_interp loop handler. */
    sm_emit_i(p, SM_JUMP, p->count + 1);
}

/* ── AST_LOOP_NEXT ───────────────────────────────────────────── */

static void lower_loop_next(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    (void)e;
    sm_emit(p, SM_PUSH_NULL);
}

/* ── AST_RETURN ──────────────────────────────────────────────── */

static void lower_return(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren > 0) lower_expr(c, e->children[0]);
    else sm_emit(p, SM_PUSH_NULL);
    sm_emit(p, SM_RETURN);
}

/* ── AST_PROC_FAIL ───────────────────────────────────────────── */

static void lower_proc_fail(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    (void)e;
    sm_emit(p, SM_PUSH_NULL);
    sm_emit(p, SM_FRETURN);
}

/* ── AST_CASE ────────────────────────────────────────────────── */
/*
 * case E of { ... } — Icon pair layout and Raku triple layout.
 *
 * Icon pair layout: [topic, val0, body0, val1, body1, ..., [default]]
 *   Topic stored in NV temp __case_topic__; each arm compares via
 *   ICN_CASE_EQ, on match evaluates body and jumps to end.
 *
 * Raku triple layout: (nchildren-1) % 3 == 0 and child[1] is AST_ILIT
 *   or AST_NUL. Emits topic + per-arm (cmp_kind, val, body) expression
 *   chunks then SM_BB_PUMP_CASE.
 */
static void lower_case(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }

    int is_raku_layout = (e->nchildren >= 4 && (e->nchildren - 1) % 3 == 0 &&
        e->children[1] && (e->children[1]->kind == AST_ILIT || e->children[1]->kind == AST_NUL));

    if (!is_raku_layout) {
        int nc = e->nchildren - 1;
        int has_default = (nc % 2 != 0);
        int npairs = nc / 2;
        lower_expr(c, e->children[0]);
        sm_emit_s(p, SM_STORE_VAR, "__case_topic__");
        sm_emit(p, SM_VOID_POP);
        int end_jumps[64]; int nend = 0;
        for (int pair = 0; pair < npairs && pair < 32; pair++) {
            AST_t *val  = e->children[1 + pair*2];
            AST_t *body = e->children[2 + pair*2];
            sm_emit_s(p, SM_PUSH_VAR, "__case_topic__");
            lower_expr(c, val);
            sm_emit_si(p, SM_CALL_FN, "ICN_CASE_EQ", 2);
            int jf = sm_emit_i(p, SM_JUMP_F, 0);
            sm_emit(p, SM_VOID_POP);
            lower_expr(c, body);
            if (nend < 64) end_jumps[nend++] = sm_emit_i(p, SM_JUMP, 0);
            int next_lbl = sm_label(p);
            sm_patch_jump(p, jf, next_lbl);
            sm_emit(p, SM_VOID_POP);
        }
        if (has_default) lower_expr(c, e->children[e->nchildren - 1]);
        else             sm_emit(p, SM_PUSH_NULL);
        int end_lbl = sm_label(p);
        for (int j = 0; j < nend; j++) sm_patch_jump(p, end_jumps[j], end_lbl);
        return;
    }

    /* Raku triple layout */
    #define EMIT_CHUNK_OF(child_expr) do {                              \
        int _skip = sm_emit_i(p, SM_JUMP, 0);                           \
        int _entry = sm_label(p);                                       \
        lower_expr(c, (child_expr));                                    \
        sm_emit(p, SM_RETURN);                                          \
        int _after = sm_label(p);                                       \
        sm_patch_jump(p, _skip, _after);                                \
        sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)_entry, 0);         \
    } while (0)

    int total_triples = (e->nchildren - 1) / 3;
    int has_default   = 0;
    int default_idx   = -1;
    if (total_triples > 0) {
        int last_i = 1 + (total_triples - 1) * 3;
        AST_t *last_cmp = e->children[last_i];
        if (last_cmp && last_cmp->kind == AST_NUL) { has_default = 1; default_idx = total_triples - 1; }
    }
    int ncases = total_triples - (has_default ? 1 : 0);

    EMIT_CHUNK_OF(e->children[0]);
    for (int t = 0; t < total_triples; t++) {
        if (t == default_idx) continue;
        int base = 1 + t * 3;
        AST_t *cmpnode = e->children[base];
        int cmp_kind = (cmpnode && cmpnode->kind == AST_ILIT) ? (int)cmpnode->ival : (int)AST_EQ;
        sm_emit_i(p, SM_PUSH_LIT_I, (int64_t)cmp_kind);
        EMIT_CHUNK_OF(e->children[base + 1]);
        EMIT_CHUNK_OF(e->children[base + 2]);
    }
    if (has_default) { int base = 1 + default_idx * 3; EMIT_CHUNK_OF(e->children[base + 2]); }
    sm_emit_ii(p, SM_BB_PUMP_CASE, (int64_t)ncases, (int64_t)has_default);
    #undef EMIT_CHUNK_OF
}

/* ── Registration ─────────────────────────────────────────────── */

void lower_icn_ctrl_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_SEQ_EXPR]   = lower_seq_expr;
    tbl[AST_IF]         = lower_if;
    tbl[AST_WHILE]      = lower_while;
    tbl[AST_UNTIL]      = lower_until;
    tbl[AST_REPEAT]     = lower_repeat;
    tbl[AST_LOOP_BREAK] = lower_loop_break;
    tbl[AST_LOOP_NEXT]  = lower_loop_next;
    tbl[AST_RETURN]     = lower_return;
    tbl[AST_PROC_FAIL]  = lower_proc_fail;
    tbl[AST_CASE]       = lower_case;
}
