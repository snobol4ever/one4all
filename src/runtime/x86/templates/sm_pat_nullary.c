#include "../emitter.h"
#include "../bb_emit.h"

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
