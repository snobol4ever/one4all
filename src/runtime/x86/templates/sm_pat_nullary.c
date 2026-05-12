/*
 * templates/sm_pat_nullary.c — nullary SM_PAT_* opcode templates.
 *
 * Covers all SM_PAT_* opcodes that take no arguments at the call site
 * (SM_TPL_NULLARY in g_sm_templates[]).  Each pops one or more DESCR_t
 * values from the value stack, calls a runtime pattern-constructor, and
 * pushes the result pattern back.  At mode-4 emit time these opcodes are
 * absorbed into a Phase-2 invariant blob; the three-column line in the
 * .s file is a NOP comment (see emit_sm_pat_baked in
 * sm_codegen_x64_emit.c).  At MACRO_DEF time this file is the source of
 * truth for each PAT_* macro body in sm_macros.s.
 *
 * Pattern: identical to sm_nullary_rt.c (SM_CONCAT / PUSH_NULL /
 * COERCE_NUM): t_macro_begin / t_call_sym_plt / t_macro_end /
 * t_pad_to_blob_size.  No mode-3 / mode-4 divergence.
 *
 * Opcodes covered:
 *   SM_PAT_EPS      PAT_EPS      rt_pat_eps      (epsilon / always-succeed)
 *   SM_PAT_ARB      PAT_ARB      rt_pat_arb      (ARB — greedy 0+)
 *   SM_PAT_REM      PAT_REM      rt_pat_rem      (REM — rest of subject)
 *   SM_PAT_FAIL     PAT_FAIL     rt_pat_fail     (FAIL — always fail)
 *   SM_PAT_SUCCEED  PAT_SUCCEED  rt_pat_succeed  (SUCCEED — always succeed)
 *   SM_PAT_ABORT    PAT_ABORT    rt_pat_abort    (ABORT — terminate match)
 *   SM_PAT_BAL      PAT_BAL      rt_pat_bal      (BAL — balanced string)
 *   SM_PAT_FENCE    PAT_FENCE    rt_pat_fence    (FENCE — no-backtrack gate)
 *   SM_PAT_FENCE1   PAT_FENCE1   rt_pat_fence1   (FENCE(p) — child pattern)
 *   SM_PAT_SPAN     PAT_SPAN     rt_pat_span     (SPAN(cs) — pop cs arg)
 *   SM_PAT_BREAK    PAT_BREAK    rt_pat_break    (BREAK(cs) — pop cs arg)
 *   SM_PAT_ANY      PAT_ANY      rt_pat_any      (ANY(cs)   — pop cs arg)
 *   SM_PAT_NOTANY   PAT_NOTANY   rt_pat_notany   (NOTANY(cs)— pop cs arg)
 *   SM_PAT_LEN      PAT_LEN      rt_pat_len      (LEN(n)    — pop n arg)
 *   SM_PAT_POS      PAT_POS      rt_pat_pos      (POS(n)    — pop n arg)
 *   SM_PAT_RPOS     PAT_RPOS     rt_pat_rpos     (RPOS(n)   — pop n arg)
 *   SM_PAT_TAB      PAT_TAB      rt_pat_tab      (TAB(n)    — pop n arg)
 *   SM_PAT_RTAB     PAT_RTAB     rt_pat_rtab     (RTAB(n)   — pop n arg)
 *   SM_PAT_ARBNO    PAT_ARBNO    rt_pat_arbno    (ARBNO(p)  — pop p arg)
 *   SM_PAT_CAT      PAT_CAT      rt_pat_cat      (p1 p2 cat — pop 2)
 *   SM_PAT_ALT      PAT_ALT      rt_pat_alt      (p1|p2 alt — pop 2)
 *   SM_PAT_DEREF    PAT_DEREF    rt_pat_deref    (*var deref — pop v)
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-r (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-r / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* unused emitter_t * param; caller compat */
#include "../bb_emit.h"

/* emit_sm_pat_nullary_rt — shared body for all nullary PAT_* templates.
 *   comment_str:  inline annotation, e.g. "SM_PAT_ARB"
 *   macro_name:   GAS macro name, e.g. "PAT_ARB"
 *   rt_sym:       PLT symbol, e.g. "rt_pat_arb"
 */
static void emit_sm_pat_nullary_rt(emitter_t *e,
                                    const char *comment_str,
                                    const char *macro_name,
                                    const char *rt_sym)
{
    (void)e;
    t_comment(comment_str);
    t_macro_begin(macro_name, NULL, 0);
    t_call_sym_plt(rt_sym, 0);
    t_macro_end();
    t_pad_to_blob_size();
}

void emit_sm_pat_eps(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_EPS — push epsilon pattern",
                           "PAT_EPS", "rt_pat_eps");
}

void emit_sm_pat_arb(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_ARB — push ARB (greedy 0+) pattern",
                           "PAT_ARB", "rt_pat_arb");
}

void emit_sm_pat_rem(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_REM — push REM (rest of subject) pattern",
                           "PAT_REM", "rt_pat_rem");
}

void emit_sm_pat_fail(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_FAIL — push FAIL (always fail) pattern",
                           "PAT_FAIL", "rt_pat_fail");
}

void emit_sm_pat_succeed(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_SUCCEED — push SUCCEED (always succeed) pattern",
                           "PAT_SUCCEED", "rt_pat_succeed");
}

void emit_sm_pat_abort(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_ABORT — push ABORT (terminate match) pattern",
                           "PAT_ABORT", "rt_pat_abort");
}

void emit_sm_pat_bal(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_BAL — push BAL (balanced string) pattern",
                           "PAT_BAL", "rt_pat_bal");
}

void emit_sm_pat_fence(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_FENCE — push FENCE (no-backtrack gate)",
                           "PAT_FENCE", "rt_pat_fence");
}

void emit_sm_pat_fence1(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_FENCE1 — pop child, push FENCE(child)",
                           "PAT_FENCE1", "rt_pat_fence1");
}

void emit_sm_pat_span(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_SPAN — pop charset, push SPAN(cs)",
                           "PAT_SPAN", "rt_pat_span");
}

void emit_sm_pat_break(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_BREAK — pop charset, push BREAK(cs)",
                           "PAT_BREAK", "rt_pat_break");
}

void emit_sm_pat_any(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_ANY — pop charset, push ANY(cs)",
                           "PAT_ANY", "rt_pat_any");
}

void emit_sm_pat_notany(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_NOTANY — pop charset, push NOTANY(cs)",
                           "PAT_NOTANY", "rt_pat_notany");
}

void emit_sm_pat_len(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_LEN — pop integer n, push LEN(n)",
                           "PAT_LEN", "rt_pat_len");
}

void emit_sm_pat_pos(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_POS — pop integer n, push POS(n)",
                           "PAT_POS", "rt_pat_pos");
}

void emit_sm_pat_rpos(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_RPOS — pop integer n, push RPOS(n)",
                           "PAT_RPOS", "rt_pat_rpos");
}

void emit_sm_pat_tab(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_TAB — pop integer n, push TAB(n)",
                           "PAT_TAB", "rt_pat_tab");
}

void emit_sm_pat_rtab(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_RTAB — pop integer n, push RTAB(n)",
                           "PAT_RTAB", "rt_pat_rtab");
}

void emit_sm_pat_arbno(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_ARBNO — pop child pattern, push ARBNO(child)",
                           "PAT_ARBNO", "rt_pat_arbno");
}

void emit_sm_pat_cat(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_CAT — pop right+left patterns, push CAT(l,r)",
                           "PAT_CAT", "rt_pat_cat");
}

void emit_sm_pat_alt(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_ALT — pop right+left patterns, push ALT(l,r)",
                           "PAT_ALT", "rt_pat_alt");
}

void emit_sm_pat_deref(emitter_t *e)
{
    emit_sm_pat_nullary_rt(e, "SM_PAT_DEREF — pop value, deref to pattern",
                           "PAT_DEREF", "rt_pat_deref");
}
