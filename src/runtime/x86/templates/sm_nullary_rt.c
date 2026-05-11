/*
 * templates/sm_nullary_rt.c — nullary RT-call SM opcode templates.
 *
 * Covers SM_CONCAT, SM_PUSH_NULL, SM_COERCE_NUM — all SM_TPL_NULLARY.
 * Pattern identical to sm_void_pop.c: t_macro_begin / t_call_sym_plt /
 * t_macro_end / t_pad_to_blob_size.  No mode-3/mode-4 divergence.
 *
 * SM_CONCAT    — pop right, pop left, push CONCAT result → rt_concat()
 * SM_PUSH_NULL — push null (empty) descriptor              → rt_push_null()
 * SM_COERCE_NUM — unary +; coerce TOS string→num in place  → rt_coerce_num()
 *
 * Sub-rung: EM-MODE4-IS-MODE3-DUMP-n (GOAL-MODE4-EMIT).
 * Session:  2026-05-11, Claude Sonnet 4.6.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-n / GOAL-MODE4-EMIT
 */

#include "../emitter.h"   /* unused emitter_t * param; caller compat */
#include "../bb_emit.h"

/* emit_sm_nullary_rt — shared body for all NULLARY rt-call templates.
 *   comment_str:  inline annotation (col 3), e.g. "SM_CONCAT"
 *   macro_name:   GAS macro name, e.g. "CONCAT"
 *   rt_sym:       PLT symbol, e.g. "rt_concat"
 */
static void emit_sm_nullary_rt(emitter_t *e,
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

void emit_sm_concat(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_CONCAT — pop right+left, push concat result",
                       "CONCAT", "rt_concat");
}

void emit_sm_push_null(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_PUSH_NULL — push null descriptor",
                       "PUSH_NULL", "rt_push_null");
}

void emit_sm_coerce_num(emitter_t *e)
{
    emit_sm_nullary_rt(e, "SM_COERCE_NUM — coerce TOS string to number",
                       "COERCE_NUM", "rt_coerce_num");
}
