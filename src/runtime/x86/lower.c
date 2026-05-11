/*
 * lower.c — IR → SM_Program compiler pass
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

#include "lower.h"
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
    lower_literal_register(g_handlers);
    lower_ref_register(g_handlers);
    lower_arith_register(g_handlers);
    lower_seq_register(g_handlers);
    lower_pat_prim_register(g_handlers);
    lower_capture_register(g_handlers);
    lower_call_register(g_handlers);
    lower_icn_relop_register(g_handlers);
    lower_icn_cset_register(g_handlers);
    lower_icn_unary_register(g_handlers);
    lower_icn_ctrl_register(g_handlers);
    lower_icn_data_register(g_handlers);
    lower_icn_sect_register(g_handlers);
    lower_icn_gen_register(g_handlers);
    lower_prolog_register(g_handlers);
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

    /* AST_SEQ_EXPR → lower_icn_ctrl (SR-10) */

    /* AST_IF, AST_WHILE, AST_UNTIL, AST_REPEAT, AST_LOOP_BREAK, AST_LOOP_NEXT,
     * AST_RETURN, AST_PROC_FAIL → lower_icn_ctrl (SR-10) */

    /* AST_NOT, AST_AUGOP, AST_SIZE, AST_NONNULL, AST_IDENTICAL → cohort_icn_unary (SR-9) */

    /* AST_MAKELIST, AST_RECORD, AST_FIELD, AST_GLOBAL, AST_INITIAL → lower_icn_data (SR-10) */

    /* AST_TO, AST_TO_BY, AST_EVERY, AST_SUSPEND,
     * AST_ITERATE, AST_ALTERNATE, AST_LIMIT → lower_icn_gen (SR-11) */
    /* AST_CHOICE, AST_CLAUSE, AST_CUT, AST_UNIFY,
     * AST_TRAIL_MARK, AST_TRAIL_UNWIND → lower_prolog (SR-11) */

    /* AST_CASE → lower_icn_ctrl (SR-10) */

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

SM_Program *lower(const CODE_t *prog)
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
