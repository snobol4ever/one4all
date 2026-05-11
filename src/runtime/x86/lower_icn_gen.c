/*
 * lower_icn_gen.c — Icon generator and suspension handlers (SR-11)
 *
 * AST kinds handled:
 *   AST_TO        lo to hi       integer range generator (step 1)
 *   AST_TO_BY     lo to hi by s  integer range generator (explicit step)
 *   AST_EVERY     every E [do B] generative iteration
 *   AST_SUSPEND   suspend E [do B]  coroutine suspend/resume
 *   AST_ITERATE   !E              generative application (list/set)
 *   AST_ALTERNATE E | E           generative alternation
 *   AST_LIMIT     E \ n           limit generator to n results
 *
 * AST_TO and AST_TO_BY are the only kinds that emit inline SM bytecode
 * for their loop bodies.  The others route through the BB pump machinery.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "lower_ctx.h"
#include "sm_interp.h"

/* ── AST_TO ──────────────────────────────────────────────────── */
/*
 * Emits an SM coroutine that yields integers lo..hi (inclusive).
 * glocal[0]=lo, glocal[1]=hi, glocal[2]=cur.
 * Shape: JUMP skip / entry: RESUME / init glocals / loop: cur>hi?exit /
 *        LOAD cur / SUSPEND / cur++ / JUMP loop / exit: NULL RETURN /
 *        skip: PUSH_EXPRESSION entry / BB_PUMP_SM
 */
static void lower_to(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    const AST_t *lo_expr = (e->nchildren > 0) ? e->children[0] : NULL;
    const AST_t *hi_expr = (e->nchildren > 1) ? e->children[1] : NULL;
    int skip_jump = sm_emit_i(p, SM_JUMP, 0);
    int entry_pc  = sm_label(p);
    sm_emit(p, SM_RESUME);
    if (lo_expr) lower_expr(c, lo_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 0); sm_emit(p, SM_VOID_POP);
    if (hi_expr) lower_expr(c, hi_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 1); sm_emit(p, SM_VOID_POP);
    sm_emit_i(p, SM_LOAD_GLOCAL, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 2); sm_emit(p, SM_VOID_POP);
    int loop_pc = sm_label(p);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 1);
    sm_emit(p, SM_ICMP_GT);
    int exit_jump = sm_emit_i(p, SM_JUMP_S, 0);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2);
    sm_emit(p, SM_SUSPEND);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_INCR, 1);
    sm_emit_i(p, SM_STORE_GLOCAL, 2); sm_emit(p, SM_VOID_POP);
    sm_emit_i(p, SM_JUMP, loop_pc);
    int exit_pc = sm_label(p);
    sm_patch_jump(p, exit_jump, exit_pc);
    sm_emit(p, SM_PUSH_NULL); sm_emit(p, SM_RETURN);
    int skip_pc = sm_label(p);
    sm_patch_jump(p, skip_jump, skip_pc);
    sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
    sm_emit(p, SM_BB_PUMP_SM);
}

/* ── AST_TO_BY ───────────────────────────────────────────────── */
/*
 * Like AST_TO but with an explicit step.
 * glocal[0]=lo, glocal[1]=hi, glocal[2]=cur, glocal[3]=step.
 * step>0: exit when cur>hi; step<0: exit when cur<hi.
 */
static void lower_to_by(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    const AST_t *lo_expr   = (e->nchildren > 0) ? e->children[0] : NULL;
    const AST_t *hi_expr   = (e->nchildren > 1) ? e->children[1] : NULL;
    const AST_t *step_expr = (e->nchildren > 2) ? e->children[2] : NULL;
    int skip_jump = sm_emit_i(p, SM_JUMP, 0);
    int entry_pc  = sm_label(p);
    sm_emit(p, SM_RESUME);
    if (lo_expr) lower_expr(c, lo_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 0); sm_emit(p, SM_VOID_POP);
    if (hi_expr) lower_expr(c, hi_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 1); sm_emit(p, SM_VOID_POP);
    if (step_expr) lower_expr(c, step_expr); else sm_emit_i(p, SM_PUSH_LIT_I, 1);
    sm_emit_i(p, SM_STORE_GLOCAL, 3); sm_emit(p, SM_VOID_POP);
    sm_emit_i(p, SM_LOAD_GLOCAL, 0);
    sm_emit_i(p, SM_STORE_GLOCAL, 2); sm_emit(p, SM_VOID_POP);
    int loop_pc = sm_label(p);
    sm_emit_i(p, SM_LOAD_GLOCAL, 3); sm_emit_i(p, SM_PUSH_LIT_I, 0);
    sm_emit(p, SM_ICMP_LT);
    int neg_branch = sm_emit_i(p, SM_JUMP_S, 0);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 1);
    sm_emit(p, SM_ICMP_GT);
    int exit_jump_pos = sm_emit_i(p, SM_JUMP_S, 0);
    int body_jump = sm_emit_i(p, SM_JUMP, 0);
    int neg_pc = sm_label(p);
    sm_patch_jump(p, neg_branch, neg_pc);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 1);
    sm_emit(p, SM_ICMP_LT);
    int exit_jump_neg = sm_emit_i(p, SM_JUMP_S, 0);
    int body_pc = sm_label(p);
    sm_patch_jump(p, body_jump, body_pc);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2);
    sm_emit(p, SM_SUSPEND);
    sm_emit_i(p, SM_LOAD_GLOCAL, 2); sm_emit_i(p, SM_LOAD_GLOCAL, 3);
    sm_emit(p, SM_ADD);
    sm_emit_i(p, SM_STORE_GLOCAL, 2); sm_emit(p, SM_VOID_POP);
    sm_emit_i(p, SM_JUMP, loop_pc);
    int exit_pc = sm_label(p);
    sm_patch_jump(p, exit_jump_pos, exit_pc);
    sm_patch_jump(p, exit_jump_neg, exit_pc);
    sm_emit(p, SM_PUSH_NULL); sm_emit(p, SM_RETURN);
    int skip_pc = sm_label(p);
    sm_patch_jump(p, skip_jump, skip_pc);
    sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
    sm_emit(p, SM_BB_PUMP_SM);
}

/* ── AST_EVERY ───────────────────────────────────────────────── */

static void lower_every(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    int every_id = every_table_register((AST_t *)e);
    sm_emit_i(p, SM_BB_PUMP_EVERY, (int64_t)every_id);
}

/* ── AST_SUSPEND ─────────────────────────────────────────────── */
/*
 * Yield value expression; run optional do-clause on resume.
 * If value fails, skip yield+do-clause and leave failed descriptor on stack.
 * On success, push NULVCL so the outer proc-body SM_VOID_POP balances.
 */
static void lower_suspend(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    if (e->nchildren > 0 && e->children[0]) lower_expr(c, e->children[0]);
    else sm_emit(p, SM_PUSH_NULL);
    int j_end = sm_emit_i(p, SM_JUMP_F, 0);
    sm_emit(p, SM_SUSPEND_VALUE);
    if (e->nchildren > 1 && e->children[1]) {
        lower_expr(c, e->children[1]);
        sm_emit(p, SM_VOID_POP);
    }
    sm_emit(p, SM_PUSH_NULL);
    int j_done = sm_emit_i(p, SM_JUMP, 0);
    int lbl_end = sm_label(p);
    sm_patch_jump(p, j_end, lbl_end);
    int lbl_finally = sm_label(p);
    sm_patch_jump(p, j_done, lbl_finally);
}

/* ── AST_ITERATE / AST_ALTERNATE / AST_LIMIT ─────────────────── */

static void lower_bb_pump_ast(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)e));
}

static void lower_limit(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    emit_push_expr(c, e);
    sm_emit(p, SM_BB_PUMP);
}

/* ── Registration ─────────────────────────────────────────────── */

void lower_icn_gen_register(LowerHandler tbl[AST_KIND_COUNT])
{
    tbl[AST_TO]        = lower_to;
    tbl[AST_TO_BY]     = lower_to_by;
    tbl[AST_EVERY]     = lower_every;
    tbl[AST_SUSPEND]   = lower_suspend;
    tbl[AST_ITERATE]   = lower_bb_pump_ast;
    tbl[AST_ALTERNATE] = lower_bb_pump_ast;
    tbl[AST_LIMIT]     = lower_limit;
}
