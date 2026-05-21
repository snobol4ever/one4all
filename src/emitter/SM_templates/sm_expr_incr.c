/* sm_expr_incr.c — SM_templates for the last 5 opcodes still in emit_walk_codegen's legacy switch.
 *
 * EC-UNI-14(c)(4): bring the SCRIP-expression family (SM_PUSH_EXPR, SM_PUSH_EXPRESSION,
 * SM_CALL_EXPRESSION) and the legacy Icon-style increment opcodes (SM_INCR, SM_DECR) under the
 * unified dispatcher.  Each template's IS_X86 arm calls the pre-existing emit_sm_<op>_dispatch
 * in emit_sm.c (now un-staticed); non-x86 arms preserve the prior walker behavior:
 *   - SM_PUSH_EXPR / SM_CALL_EXPRESSION : silent no-op for JVM/JS/NET/WASM (the walkers had no
 *     special case for these opcodes — they fell through to the legacy switch's default arm).
 *   - SM_PUSH_EXPRESSION : JS-only override (rt.push_null()) preserved verbatim; the JVM/NET/WASM
 *     no-handling behavior preserved.  (This used to live in emit_js_from_sm at line ~1983; the
 *     override was an explicit `instr->op == SM_PUSH_EXPRESSION` branch.  It moves here in this
 *     commit and the JS walker drops its branch.)
 *   - SM_INCR / SM_DECR : WASM aliases SM_INCR -> sm_add and SM_DECR -> sm_sub (preserved here);
 *     JVM/JS/NET have no handling — preserved as no-ops.  These two opcodes are emitted only by
 *     unit tests (sm_interp_test.c) — no live frontend lowers to INCR/DECR today.
 *
 * Coverage after this commit: emit_sm_dispatch handles every opcode emit_walk_codegen's legacy
 * switch handles.  The legacy switch can now be deleted (EC-UNI-14(c)(5)), then dispatch_one_x86
 * collapses into the walker (EC-UNI-14(c)(6)), then SCRIP_UNIFIED_DISPATCH flag deletes
 * (EC-UNI-14(c)(7)).  See GOAL-HEADQUARTERS.md EC-UNI-14 proper. */
#include "sm_template_common.h"
#include "emit_sm.h"
#include "sm_templates.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_push_expr(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_push_expr_dispatch(out, instr, 0); return; }
    /* IS_JVM / IS_JS / IS_NET / IS_WASM: silent no-op preserved from legacy walker.
     * SM_PUSH_EXPR carries a frozen tree_t* (Prolog interactive consult path);
     * non-x86 backends have no equivalent runtime hook today. */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_push_expression(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_push_expression_dispatch(out, instr, 0); return; }
    if (IS_JS)  { emit_textf("rt.push_null(); "); return; }              /* preserved from emit_js_from_sm */
    /* IS_JVM / IS_NET / IS_WASM: silent no-op (walker fall-through preserved). */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_call_expression(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86) { emit_sm_call_expression_dispatch(out, instr, 0); return; }
    /* IS_JVM / IS_JS / IS_NET / IS_WASM: silent no-op preserved from legacy walker. */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_incr(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86)  { emit_sm_incr_dispatch(out, instr, 0); return; }
    if (IS_WASM) { sm_add(); return; }                                   /* alias preserved from emit_wasm_from_sm */
    /* IS_JVM / IS_JS / IS_NET: silent no-op (no live frontend lowers SM_INCR today). */
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void sm_decr(void) {
    const SM_t * instr = g_emit.instr; FILE * out = g_emit.out;
    if (IS_X86)  { emit_sm_decr_dispatch(out, instr, 0); return; }
    if (IS_WASM) { sm_sub(); return; }                                   /* alias preserved from emit_wasm_from_sm */
    /* IS_JVM / IS_JS / IS_NET: silent no-op (no live frontend lowers SM_DECR today). */
}
