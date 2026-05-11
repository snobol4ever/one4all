/*
 * sm_lower.c — IR → SM_Program compiler pass
 *
 * Walks a CODE_t* (linked list of STMT_t, each holding AST_t trees)
 * and emits a flat SM_Program instruction sequence.
 *
 * SNOBOL4 statement model:
 *   label:  subject  pattern = replacement  :(goto)
 *
 * SM lowering strategy per statement:
 *   1. Emit SM_LABEL for stmt->label (if present) → recorded in label_table
 *   2. Eval subject   → value on stack
 *   3. If pattern:    → emit SM_PAT_* tree; emit SM_EXEC_STMT
 *      Else if replacement only: emit subject eval + SM_STORE_VAR
 *   4. Gotos: SM_JUMP_S / SM_JUMP_F / SM_JUMP (patched after all stmts)
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */

#include "sm_lower.h"
#include "lower_ctx.h"
#include "sm_prog.h"
#include "sm_interp.h"

#include "../../frontend/snobol4/scrip_cc.h"
#include "../ast/ast.h"
#include "../../runtime/common/ast_clone.h"
#include "../../runtime/interp/coro_runtime.h"
#include "../../runtime/interp/pl_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <gc/gc.h>
#include "snobol4.h"

/* SR-3: expression_scope_walk, emit_goto, kw_canonicalize moved to lower_ctx.c */

/* ── SR-4: handler table (hybrid dispatcher) ────────────────────────────
 * Indexed by AST_e.  NULL → fall through to legacy switch in lower_expr.
 * Populated once at first call to lower_expr via init_handlers(). */
static LowerHandler g_handlers[AST_KIND_COUNT];
static int          g_handlers_initialized = 0;

static void init_handlers(void)
{
    cohort_literal_register(g_handlers);
    cohort_ref_register(g_handlers);
    cohort_arith_register(g_handlers);
    cohort_seq_register(g_handlers);
    cohort_pat_prim_register(g_handlers);
    cohort_capture_register(g_handlers);
    cohort_call_register(g_handlers);
    cohort_icn_relop_register(g_handlers);
    cohort_icn_cset_register(g_handlers);
    cohort_icn_unary_register(g_handlers);
    g_handlers_initialized = 1;
}

/* ── Expression lowering ────────────────────────────────────────────────── */

void lower_expr(LowerCtx *c, const AST_t *e);

/* lower_pat_expr and sm_pat_capture_fn_arg_names moved to lower_pat.c (SR-7). */

void lower_expr(LowerCtx *c, const AST_t *e)
{
    SM_Program *p = c->p;
    LabelTable *labtab = &c->labtab;
    if (!e) {
        sm_emit(p, SM_PUSH_NULL);
        return;
    }

    /* SR-4: handler table first; NULL → legacy switch below. */
    if (!g_handlers_initialized) init_handlers();
    if (e->kind >= 0 && e->kind < AST_KIND_COUNT && g_handlers[e->kind]) {
        g_handlers[e->kind](c, e);
        return;
    }

    switch (e->kind) {

    /* ── Literals ── */
    /* AST_QLIT, AST_CSET, AST_ILIT, AST_FLIT, AST_NUL → cohort_literal */
    /* AST_NULL → cohort_icn_unary (SR-9) */

    /* ── References ── */
    /* AST_VAR, AST_KEYWORD, AST_INDIRECT, AST_DEFER → cohort_ref */

    /* ── Arithmetic ── */
    /* AST_INTERROGATE, AST_NAME, AST_MNS, AST_PLS,
     * AST_ADD, AST_SUB, AST_MUL, AST_DIV, AST_MOD, AST_POW → cohort_arith */

    /* AST_VLIST, AST_CAT, AST_SEQ, AST_ALT, AST_OPSYN → cohort_seq */
    /* AST_ARB..AST_ARBNO (pattern primitives) → cohort_pat_prim */

    /* AST_ASSIGN, AST_FNC, AST_IDX, AST_SCAN, AST_SWAP → cohort_call */
    /* AST_CAPT_COND_ASGN, AST_CAPT_IMMED_ASGN, AST_CAPT_CURSOR → cohort_capture */

    /* AST_EQ/NE/LT/LE/GT/GE, AST_LLT/LLE/LGT/LGE/LEQ/LNE → cohort_icn_relop (SR-9) */
    /* AST_INTERROGATE, AST_NAME → cohort_arith */

    /* AST_SCAN, AST_SWAP → cohort_call (SR-8) */

    /* AST_OPSYN → cohort_seq */
    /* AST_ALT, AST_ARB..AST_ARBNO → cohort_pat_prim */
    /* AST_CAPT_COND_ASGN, AST_CAPT_IMMED_ASGN, AST_CAPT_CURSOR → cohort_capture (SR-8) */

    case AST_SEQ_EXPR:
        if (e->nchildren == 0) { sm_emit(p, SM_PUSH_NULL); return; }
        for (int i = 0; i < e->nchildren; i++) {
            lower_expr(c, e->children[i]);
            if (i < e->nchildren - 1) sm_emit(p, SM_VOID_POP);
        }
        return;

    case AST_IF: {
        if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }
        lower_expr(c, e->children[0]);              /* condition */
        int jf = sm_emit_i(p, SM_JUMP_F, 0);       /* jump-if-fail to else */
        /* Condition result left on stack (SM_JUMP_F reads last_ok, not TOS).
           Drain it before entering then-body. */
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
        return;
    }

    case AST_WHILE: {
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
        return;
    }

    case AST_UNTIL: {
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
        return;
    }

    case AST_REPEAT: {
        int top_lbl = sm_label(p);
        if (e->nchildren > 0) { lower_expr(c, e->children[0]); sm_emit(p, SM_VOID_POP); }
        sm_emit_i(p, SM_JUMP, top_lbl);
        sm_emit(p, SM_PUSH_NULL);
        return;
    }

    case AST_LOOP_BREAK:
        if (e->nchildren > 0) lower_expr(c, e->children[0]);
        else sm_emit(p, SM_PUSH_NULL);
        /* SM_JUMP to self+1 signals break to the sm_interp loop handler. */
        sm_emit_i(p, SM_JUMP, p->count + 1);
        return;

    case AST_LOOP_NEXT:
        sm_emit(p, SM_PUSH_NULL);
        return;

    case AST_RETURN:
        if (e->nchildren > 0) lower_expr(c, e->children[0]);
        else sm_emit(p, SM_PUSH_NULL);
        sm_emit(p, SM_RETURN);
        return;

    case AST_PROC_FAIL:
        sm_emit(p, SM_PUSH_NULL);
        sm_emit(p, SM_FRETURN);
        return;

    /* AST_NOT, AST_AUGOP, AST_SIZE, AST_NONNULL, AST_IDENTICAL → cohort_icn_unary (SR-9) */

    case AST_MAKELIST:
        for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
        sm_emit_si(p, SM_CALL_FN, "MAKELIST", (int64_t)e->nchildren);
        return;

    case AST_RECORD:
        sm_emit_s(p, SM_PUSH_LIT_S, e->sval ? e->sval : "");
        for (int i = 0; i < e->nchildren; i++) lower_expr(c, e->children[i]);
        sm_emit_si(p, SM_CALL_FN, "RECORD_MAKE", (int64_t)e->nchildren + 1);
        return;

    case AST_FIELD:
        lower_expr(c, e->nchildren > 0 ? e->children[0] : NULL);
        sm_emit_s(p, SM_PUSH_LIT_S, e->sval ? e->sval : "");
        sm_emit_si(p, SM_CALL_FN, "FIELD_GET", 2);
        return;

    case AST_GLOBAL:
        sm_emit(p, SM_PUSH_NULL);
        return;

    /* GOAL-ICON-BB-COMPLETE: AST_INITIAL once-flag fix.
     *
     * Icon `initial { ... }` runs its body the FIRST time the enclosing
     * procedure is called, then skips on every subsequent call.  In SM
     * mode the once-flag must persist across calls, so we use a per-AST
     * NV sentinel variable (named `__initial_<hex_ptr>__`).
     *
     * Emitted shape:
     *
     *   SM_PUSH_VAR  __initial_<ptr>__   ; sentinel — null on first call
     *   SM_CALL_FN   NONNULL 1           ; FAIL if null, succeed if set
     *   SM_JUMP_S    L_skip              ; sentinel set → skip body
     *   SM_VOID_POP                      ; drop FAILDESCR from NONNULL
     *   [ lower each child of initial ]  ; the assignments
     *   SM_PUSH_LIT_I 1                  ; mark sentinel
     *   SM_STORE_VAR __initial_<ptr>__   ; sentinel := 1
     *   SM_VOID_POP                      ; drop stored value
     *   SM_JUMP      L_done
     * L_skip:
     *   SM_VOID_POP                      ; drop NONNULL's value (sentinel)
     * L_done:
     *   SM_PUSH_NULL                     ; initial is a statement; result is null
     *
     * The vars assigned inside `initial` were already routed to NV by
     * expression_scope_walk's AST_INITIAL skip (in lower_ctx.c), so the
     * child assignments emit SM_STORE_VAR (persistent) not SM_STORE_FRAME
     * (per-call). */
    case AST_INITIAL: {
        char sentinel[64];
        snprintf(sentinel, sizeof(sentinel), "__initial_%lx__",
                 (unsigned long)(uintptr_t)e);

        sm_emit_s(p, SM_PUSH_VAR, sentinel);
        sm_emit_si(p, SM_CALL_FN, "NONNULL", 1);
        int skip_jump = sm_emit_i(p, SM_JUMP_S, 0);

        /* sentinel was null (or FAILDESCR is on stack) — drop the FAILDESCR. */
        sm_emit(p, SM_VOID_POP);

        /* Run the initial body. */
        for (int i = 0; i < e->nchildren; i++) {
            if (!e->children[i]) continue;
            lower_expr(c, e->children[i]);
            sm_emit(p, SM_VOID_POP);
        }

        /* Mark sentinel set. */
        sm_emit_i(p, SM_PUSH_LIT_I, 1);
        sm_emit_s(p, SM_STORE_VAR, sentinel);
        sm_emit(p, SM_VOID_POP);

        int done_jump = sm_emit_i(p, SM_JUMP, 0);

        /* skip-body landing: drop the sentinel value left by NONNULL. */
        int skip_pc = sm_label(p);
        sm_patch_jump(p, skip_jump, skip_pc);
        sm_emit(p, SM_VOID_POP);

        int done_pc = sm_label(p);
        sm_patch_jump(p, done_jump, done_pc);

        sm_emit(p, SM_PUSH_NULL);
        return;
    }

    /* ── Generator: integer range lo to hi (step 1) ── */
    case AST_TO: {
        /* glocal[0]=lo, glocal[1]=hi, glocal[2]=cur.
         * Loop: while cur <= hi, yield cur, cur++. */
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
        int exit_pc_here = sm_label(p);
        sm_patch_jump(p, exit_jump, exit_pc_here);
        sm_emit(p, SM_PUSH_NULL); sm_emit(p, SM_RETURN);
        int skip_pc = sm_label(p);
        sm_patch_jump(p, skip_jump, skip_pc);
        sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
        sm_emit(p, SM_BB_PUMP_SM);
        return;
    }

    /* ── Generator: integer range lo to hi by step ── */
    case AST_TO_BY: {
        /* glocal[0]=lo, glocal[1]=hi, glocal[2]=cur, glocal[3]=step.
         * step>0: exit when cur>hi; step<0: exit when cur<hi. */
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
        int exit_pc_here = sm_label(p);
        sm_patch_jump(p, exit_jump_pos, exit_pc_here);
        sm_patch_jump(p, exit_jump_neg, exit_pc_here);
        sm_emit(p, SM_PUSH_NULL); sm_emit(p, SM_RETURN);
        int skip_pc = sm_label(p);
        sm_patch_jump(p, skip_jump, skip_pc);
        sm_emit_ii(p, SM_PUSH_EXPRESSION, (int64_t)entry_pc, 0);
        sm_emit(p, SM_BB_PUMP_SM);
        return;
    }

    case AST_EVERY: {
        int every_id = every_table_register((AST_t *)e);
        sm_emit_i(p, SM_BB_PUMP_EVERY, (int64_t)every_id);
        return;
    }

    case AST_SUSPEND: {
        /* Yield value expression; run optional do-clause on resume.
         * If value fails, skip yield+do-clause and leave failed descriptor on stack.
         * On success, push NULVCL so the outer proc-body SM_VOID_POP balances. */
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
        return;
    }

    /* AST_LCONCAT → cohort_icn_cset (SR-9) */

    case AST_BANG_BINARY:
        sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)e));
        return;

    case AST_ITERATE:
        sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)e));
        return;

    case AST_ALTERNATE:
        sm_emit_i(p, SM_BB_PUMP_AST, (int64_t)ast_pump_table_register((AST_t *)e));
        return;

    case AST_SECTION:
        if (e->nchildren >= 3) {
            lower_expr(c, e->children[0]); lower_expr(c, e->children[1]); lower_expr(c, e->children[2]);
            sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_RANGE", 3);
        } else { sm_emit(p, SM_PUSH_NULL); }
        return;
    case AST_SECTION_PLUS:
        if (e->nchildren >= 3) {
            lower_expr(c, e->children[0]); lower_expr(c, e->children[1]); lower_expr(c, e->children[2]);
            sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_PLUS", 3);
        } else { sm_emit(p, SM_PUSH_NULL); }
        return;
    case AST_SECTION_MINUS:
        if (e->nchildren >= 3) {
            lower_expr(c, e->children[0]); lower_expr(c, e->children[1]); lower_expr(c, e->children[2]);
            sm_emit_si(p, SM_CALL_FN, "ICN_SECTION_MINUS", 3);
        } else { sm_emit(p, SM_PUSH_NULL); }
        return;

    case AST_LIMIT:
        emit_push_expr(c, e);
        sm_emit(p, SM_BB_PUMP);
        return;

    case AST_RANDOM:
        if (e->nchildren >= 1) {
            lower_expr(c, e->children[0]);
            sm_emit_si(p, SM_CALL_FN, "ICN_RANDOM", 1);
        } else { sm_emit(p, SM_PUSH_NULL); }
        return;

    /* ── Prolog backtracking nodes ── */
    case AST_CHOICE:
        if (e->sval) {
            const char *key = e->sval;
            int arity = 0;
            const char *sl = strrchr(key, '/');
            if (sl) arity = atoi(sl + 1);
            sm_emit_si(p, SM_BB_ONCE_PROC, key, (int64_t)arity);
        } else {
            emit_push_expr(c, e);
            sm_emit(p, SM_BB_ONCE);
        }
        return;
    case AST_CLAUSE:
    case AST_CUT:
    case AST_UNIFY:
    case AST_TRAIL_MARK:
    case AST_TRAIL_UNWIND:
        /* Children of AST_CHOICE walked by the broker; rarely lowered standalone. */
        emit_push_expr(c, e);
        sm_emit(p, SM_BB_ONCE);
        return;

    /* ── Raku CASE dispatch ── */
    case AST_CASE: {
        if (e->nchildren < 1) { sm_emit(p, SM_PUSH_NULL); return; }

        /* Raku triple layout: (nchildren-1) % 3 == 0 and child[1] is AST_ILIT or AST_NUL. */
        int is_raku_layout = (e->nchildren >= 4 && (e->nchildren - 1) % 3 == 0 &&
            e->children[1] && (e->children[1]->kind == AST_ILIT || e->children[1]->kind == AST_NUL));

        if (!is_raku_layout) {
            /* Icon pair layout: [topic, val0, body0, val1, body1, ..., [default]].
             * Topic stored in NV temp; each arm compares, on match evals body and jumps end. */
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

        /* Raku triple layout: emit topic + per-arm (cmp_kind, val, body) chunks
         * then optional default body chunk, then SM_BB_PUMP_CASE. */
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
        return;
    }

    default:
        if (!c->expression_body_lowering)
            fprintf(stderr, "sm_lower: unhandled expr kind %d\n", (int)e->kind);
        sm_emit(p, SM_PUSH_NULL);
        return;
    }
}

/* ── Statement lowering ─────────────────────────────────────────────────── */

static void lower_stmt(LowerCtx *c, const STMT_t *s)
{
    SM_Program *p = c->p;
    LabelTable *labtab = &c->labtab;

    /* Blank source line — emit nothing; the next non-blank stmt's SM_STNO fires. */
    if (!s->is_end
        && (!s->label || !s->label[0])
        && !s->subject && !s->pattern && !s->replacement
        && !s->goto_u && !s->goto_u_expr && !s->goto_s && !s->goto_s_expr && !s->goto_f && !s->goto_f_expr) {
        return;
    }

    /* Label emitted before SM_STNO so backward branches land on the STNO. */
    if (s->label && s->label[0]) {
        int lbl_idx = sm_label_named(p, s->label);
        labtab_define(labtab, s->label, lbl_idx);
        /* Tag DEFINE'd function entry labels; mode-3 emits a call prologue for them. */
        if (FUNC_IS_ENTRY_LABEL(s->label)) {
            p->instrs[p->count - 1].a[2].i = 1;
            sm_emit(p, SM_DEFINE_ENTRY);
        }
    }

    sm_emit_ii(p, SM_STNO, (int64_t)s->stno, (int64_t)s->lineno);

    if (s->is_end) { sm_emit(p, SM_HALT); return; }

    /* Icon proc/global/record defs are registered by polyglot_init; nothing to emit per-def. */
    if (s->lang == LANG_ICN) return;

    if (s->lang == LANG_PL) {
        if (s->subject && s->subject->kind == AST_CHOICE && s->subject->sval) {
            const char *key = s->subject->sval;
            int arity = 0;
            const char *sl = strrchr(key, '/');
            if (sl) arity = atoi(sl + 1);
            sm_emit_si(p, SM_BB_ONCE_PROC, key, (int64_t)arity);
        } else {
            if (s->subject) lower_expr(c, s->subject);
            else            sm_emit(p, SM_PUSH_NULL);
            sm_emit(p, SM_BB_ONCE);
        }
        goto emit_gotos;
    }

    /*
     * Pattern match statement:  subject  pattern  [= replacement]  :(goto)
     *
     * Pattern tree is emitted first so its parameterised-op args (e.g. SM_PAT_LEN)
     * are consumed from the value stack before the subject is pushed.
     */
    if (s->pattern) {
        lower_pat_expr(c, s->pattern);
        if (s->subject) lower_expr(c, s->subject);
        else            sm_emit(p, SM_PUSH_NULL);
        if (s->has_eq && s->replacement)
            lower_expr(c, s->replacement);
        else if (s->has_eq)
            sm_emit_si(p, SM_PUSH_LIT_S, "", 0);
        else
            sm_emit_i(p, SM_PUSH_LIT_I, 0);
        /* a[0].s = subject variable name for write-back; a[1].i = has_eq.
         * GC_strdup the sval — the IR may be freed before the SM_Program is used. */
        {
            const char *sname = NULL;
            if (s->subject && (s->subject->kind == AST_VAR || s->subject->kind == AST_KEYWORD))
                sname = s->subject->sval;
            sm_emit_si(p, SM_EXEC_STMT, sname, (int64_t)s->has_eq);
        }
        goto emit_gotos;
    }

    /*
     * Pure assignment or expression statement:
     *   label:  expr = value   :(goto)
     *   label:  expr           :(goto)
     */
    if (s->subject) {
        if (s->has_eq) {
            if (s->replacement) lower_expr(c, s->replacement);
            else                sm_emit(p, SM_PUSH_NULL);

            if (s->subject->kind == AST_VAR || s->subject->kind == AST_KEYWORD) {
                sm_emit_s(p, SM_STORE_VAR, s->subject->sval ? s->subject->sval : "");
            } else if (s->subject->kind == AST_INDIRECT) {
                lower_expr(c, s->subject->nchildren > 0 ? s->subject->children[0] : NULL);
                sm_emit_si(p, SM_CALL_FN, "ASGN_INDIR", 2);
            } else if (s->subject->kind == AST_IDX) {
                int nc = s->subject->nchildren;
                for (int ci = 0; ci < nc; ci++) lower_expr(c, s->subject->children[ci]);
                sm_emit_si(p, SM_CALL_FN, "IDX_SET", (int64_t)(nc + 1));
            } else if (s->subject->kind == AST_FNC && s->subject->sval) {
                if (s->subject->nchildren == 0) {
                    /* Zero-arg LHS: NRETURN path — fn returns DT_N, we write through. */
                    sm_emit_si(p, SM_CALL_FN, "NRETURN_ASGN", 1);
                    p->instrs[p->count - 1].a[1].s = GC_strdup(s->subject->sval);
                } else {
                    if (strcasecmp(s->subject->sval, "ITEM") == 0) {
                        int nc = s->subject->nchildren;
                        for (int ci = 0; ci < nc; ci++) lower_expr(c, s->subject->children[ci]);
                        sm_emit_si(p, SM_CALL_FN, "ITEM_SET", (int64_t)(nc + 1));
                    } else {
                        lower_expr(c, s->subject->nchildren > 0 ? s->subject->children[0] : NULL);
                        char _setname[256];
                        snprintf(_setname, sizeof(_setname), "%s_SET", s->subject->sval);
                        sm_emit_si(p, SM_CALL_FN, _setname, 2);
                    }
                }
            } else {
                lower_expr(c, s->subject);
                sm_emit_si(p, SM_CALL_FN, "ASGN", 2);
            }
        } else {
            /* Bare expression statement.
             * SNOBOL4 special case: bare RETURN / FRETURN / NRETURN with no
             * assignment is equivalent to :(RETURN) — emit the return opcode. */
            if (s->subject->kind == AST_VAR && s->subject->sval) {
                if (strcasecmp(s->subject->sval, "RETURN") == 0)  { sm_emit(p, SM_RETURN);  goto emit_gotos; }
                if (strcasecmp(s->subject->sval, "FRETURN") == 0) { sm_emit(p, SM_FRETURN); goto emit_gotos; }
                if (strcasecmp(s->subject->sval, "NRETURN") == 0) { sm_emit(p, SM_NRETURN); goto emit_gotos; }
            }
            lower_expr(c, s->subject);
            sm_emit(p, SM_VOID_POP);
        }
    }

emit_gotos: {
    if (!s->goto_u && !s->goto_u_expr && !s->goto_s && !s->goto_s_expr && !s->goto_f && !s->goto_f_expr) return;
    if (s->goto_u && s->goto_u[0]) { emit_goto(c, SM_JUMP, s->goto_u); return; }
    if (s->goto_u_expr) {
        sm_emit_s(p, SM_PUSH_LIT_S, "(computed-goto)");
        sm_emit(p, SM_JUMP_INDIR);
        return;
    }
    if (s->goto_s && s->goto_s[0]) emit_goto(c, SM_JUMP_S, s->goto_s);
    if (s->goto_f && s->goto_f[0]) emit_goto(c, SM_JUMP_F, s->goto_f);
    }
}

/* ── Public entry point ─────────────────────────────────────────────────── */

SM_Program *sm_lower(const CODE_t *prog)
{
    if (!prog) return NULL;

    LowerCtx ctx;
    ctx.p                        = sm_prog_new();
    ctx.expression_body_lowering = 0;
    ctx.expression_scope         = NULL;
    labtab_init(&ctx.labtab);

    LowerCtx   *c      = &ctx;
    SM_Program *p      = ctx.p;
    LabelTable *labtab = &ctx.labtab;

    /* Emit named-expression skeletons for every Icon/Raku proc.
     *
     * Each proc body is lowered with expression_body_lowering=1, which
     * silences "unhandled kind" warnings (proc bodies are currently
     * unreachable for generator kinds via the SM path) and causes
     * AST_VAR / AST_ASSIGN to emit SM_LOAD_FRAME / SM_STORE_FRAME for
     * in-scope names via the per-proc IcnScope built below.
     *
     * Slot order matches icn_scope_patch: params first, then
     * AST_GLOBAL-decl names, then AST_VARs encountered in body order. */
    for (int pi = 0; pi < proc_count; pi++) {
        const char *nm = proc_table[pi].name;
        if (!nm || !*nm) continue;
        AST_t *proc = proc_table[pi].proc;

        int skip_jump = sm_emit_i(p, SM_JUMP, 0);
        sm_label_named(p, nm);

        if (proc) {
            int nparams    = (int)proc->ival;
            int body_start = 1 + nparams;

            IcnScope expression_sc; expression_sc.n = 0;
            for (int i = 0; i < nparams && i < FRAME_SLOT_MAX; i++) {
                AST_t *pn = proc->children[1+i];
                if (pn && pn->sval) scope_add(&expression_sc, pn->sval);
            }
            for (int bi = body_start; bi < proc->nchildren; bi++)
                expression_scope_walk(&expression_sc, proc->children[bi]);

            /* GOAL-ICON-BB-COMPLETE: AST_INITIAL once-flag fix (part 2).
             *
             * Variables assigned inside `initial { ... }` MUST be stored
             * in NV (persistent), not frame slots (reset each call).
             * The expression_scope_walk skip in lower_ctx.c prevented the
             * init subtree from contributing slots, but a var like `x`
             * also appears outside the initial block (e.g. `x := x + 1`)
             * — that outer use added a frame slot.  Remove those names
             * from expression_sc so all uses route to NV.
             *
             * Walk every AST_INITIAL child's AST_ASSIGN LHS; if LHS is an
             * AST_VAR, remove its name from expression_sc by compacting
             * the array. */
            for (int bi = body_start; bi < proc->nchildren; bi++) {
                AST_t *child = proc->children[bi];
                if (!child || child->kind != AST_INITIAL) continue;
                for (int ai = 0; ai < child->nchildren; ai++) {
                    AST_t *as = child->children[ai];
                    if (!as || as->kind != AST_ASSIGN || as->nchildren < 1) continue;
                    AST_t *lhs = as->children[0];
                    if (!lhs || lhs->kind != AST_VAR || !lhs->sval) continue;
                    const char *nm = lhs->sval;
                    int w = 0;
                    for (int r = 0; r < expression_sc.n; r++) {
                        if (expression_sc.e[r].name &&
                            strcmp(expression_sc.e[r].name, nm) == 0)
                            continue;  /* drop this entry */
                        if (w != r) expression_sc.e[w] = expression_sc.e[r];
                        w++;
                    }
                    /* Reassign slot numbers densely so SM_LOAD_FRAME indices
                     * remain valid (slot field must match position).  Frame
                     * env_n is set at call time from scope.n, so densifying
                     * is required. */
                    expression_sc.n = w;
                    for (int s = 0; s < expression_sc.n; s++)
                        expression_sc.e[s].slot = s;
                }
            }

            c->expression_scope         = &expression_sc;
            c->expression_body_lowering = 1;
            for (int bi = body_start; bi < proc->nchildren; bi++) {
                AST_t *body_expr = proc->children[bi];
                if (!body_expr) continue;
                lower_expr(c, body_expr);
                sm_emit(p, SM_VOID_POP);
            }
            c->expression_body_lowering = 0;
            c->expression_scope         = NULL;
        }

        sm_emit(p, SM_RETURN);
        int skip_lbl = sm_label(p);
        sm_patch_jump(p, skip_jump, skip_lbl);
    }

    /* Emit named-expression skeletons for every Prolog predicate.
     * Symmetrical to the Icon/Raku loop; body lowering is deferred. */
    for (int b = 0; b < PL_PRED_TABLE_SIZE_FWD; b++) {
        for (Pl_PredEntry *e = g_pl_pred_table.buckets[b]; e; e = e->next) {
            if (!e->key || !*e->key) continue;
            int skip_jump_pl = sm_emit_i(p, SM_JUMP, 0);
            sm_label_named(p, e->key);
            sm_emit(p, SM_RETURN);
            int skip_lbl_pl  = sm_label(p);
            sm_patch_jump(p, skip_jump_pl, skip_lbl_pl);
        }
    }

    int stno = 0;
    int has_icn = 0;
    for (const STMT_t *s = prog->head; s; s = s->next) {
        if (s->lang == LANG_ICN) {
            has_icn = 1;
            sm_stno_label_record(p, ++stno, NULL);
            continue;
        }
        lower_stmt(c, s);
        sm_stno_label_record(p, ++stno, (s->label && s->label[0]) ? s->label : NULL);
    }

    /* Icon programs: synthesise top-level main() pump. */
    if (has_icn) sm_emit_si(p, SM_BB_PUMP_PROC, "main", 0);

    if (p->count == 0 || p->instrs[p->count - 1].op != SM_HALT)
        sm_emit(p, SM_HALT);

    labtab_resolve(labtab, p);
    labtab_free(labtab);
    return p;
}
