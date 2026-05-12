/* bb_boxes.c — All Byrd box C implementations, consolidated
 * Generated from per-box sources. One function per box.
 * AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6
 */
#include "bb_box.h"
#include "bb_convert.h"
#include "snobol4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ───── lit ───── */
/* _XCHR     LIT         literal string match */


lit_t *bb_lit_new(const char *lit, int len)
{ lit_t *ζ=calloc(1,sizeof(lit_t)); ζ->lit=lit; ζ->len=len; return ζ; }

/* ───── seq ───── */
/* _XCAT     SEQ         concatenation: left then right; β retries right then left */

#pragma GCC diagnostic ignored "-Wmisleading-indentation"
typedef struct { bb_box_fn fn; void *state; } bb_child_t;
typedef struct { bb_child_t left; bb_child_t right; spec_t matched; } seq_t;

seq_t *bb_seq_new(bb_box_fn lf, void *ls, bb_box_fn rf, void *rs)
{ seq_t *ζ=calloc(1,sizeof(seq_t)); ζ->left.fn=lf; ζ->left.state=ls; ζ->right.fn=rf; ζ->right.state=rs; return ζ; }

/* ───── alt ───── */
/* _XOR      ALT         alternation: try each child on α; β retries same child only */

#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#define BB_ALT_INIT 4
typedef struct { bb_box_fn fn; void *state; } bb_altchild_t;
typedef struct { int n; int cap; bb_altchild_t *children; int current; int position; spec_t result; } alt_t;

alt_t *bb_alt_new(int n, bb_box_fn *fns)
{
    alt_t *ζ = calloc(1, sizeof(alt_t));
    ζ->cap      = n > BB_ALT_INIT ? n : BB_ALT_INIT;
    ζ->children = malloc(ζ->cap * sizeof(bb_altchild_t));
    ζ->n = n;
    for (int i = 0; i < n; i++) ζ->children[i].fn = fns[i];
    return ζ;
}

/* ───── arb ───── */
/* _XFARB    ARB         match 0..n chars lazily; β extends by 1 */


arb_t *bb_arb_new(void)
{ return calloc(1,sizeof(arb_t)); }

/* ───── arbno ───── */
/* _XARBN    ARBNO       zero-or-more greedy; zero-advance guard; β unwinds stack */

#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#define ARBNO_INIT 8
typedef struct { spec_t matched; int start; } arbno_frame_t;
typedef struct { bb_box_fn fn; void *state; int depth; int cap; arbno_frame_t *stack; } arbno_t;

arbno_t *bb_arbno_new(bb_box_fn fn, void *state)
{
    arbno_t *ζ = calloc(1, sizeof(arbno_t));
    ζ->fn    = fn;
    ζ->state = state;
    ζ->cap   = ARBNO_INIT;
    ζ->stack = malloc(ζ->cap * sizeof(arbno_frame_t));
    return ζ;
}

/* ───── any ───── */
/* _XANYC    ANY         match one char if in set */


any_t *bb_any_new(const char *chars)
{ any_t *ζ=calloc(1,sizeof(any_t)); ζ->chars=chars; return ζ; }

/* ───── notany ───── */
/* _XNNYC    NOTANY      match one char if NOT in set */


notany_t *bb_notany_new(const char *chars)
{ notany_t *ζ=calloc(1,sizeof(notany_t)); ζ->chars=chars; return ζ; }

/* ───── span ───── */
/* _XSPNC    SPAN        longest prefix of chars in set (≥1) */


span_t *bb_span_new(const char *chars)
{ span_t *ζ=calloc(1,sizeof(span_t)); ζ->chars=chars; return ζ; }

/* ───── brk ───── */
/* _XBRKC    BRK         scan to first char in set (may be zero-width) */


brk_t *bb_brk_new(const char *chars)
{ brk_t *ζ=calloc(1,sizeof(brk_t)); ζ->chars=chars; return ζ; }

/* ───── breakx ───── */
/* _XBRKX    BREAKX      like BRK but fails on zero advance */


brkx_t *bb_breakx_new(const char *chars)
{ brkx_t *ζ=calloc(1,sizeof(brkx_t)); ζ->chars=chars; return ζ; }

/* ───── len ───── */
/* _XLNTH    LEN         match exactly n characters */


len_t *bb_len_new(int n)
{ len_t *ζ=calloc(1,sizeof(len_t)); ζ->n=n; return ζ; }

/* ───── pos ───── */
/* _XPOSI    POS         assert cursor == n (zero-width) */


pos_t *bb_pos_new(int n)
{ pos_t *ζ=calloc(1,sizeof(pos_t)); ζ->n=n; return ζ; }

/* ───── tab ───── */
/* _XTB      TAB         advance cursor TO absolute position n */


tab_t *bb_tab_new(int n)
{ tab_t *ζ=calloc(1,sizeof(tab_t)); ζ->n=n; return ζ; }

/* ───── rem ───── */
/* _XSTAR    REM         match entire remainder; no backtrack */


rem_t *bb_rem_new(void)
{ return calloc(1,sizeof(rem_t)); }

/* ───── eps ───── */
/* _XEPS     EPS         zero-width success once; done flag prevents double-γ */


eps_t *bb_eps_new(void)
{ return calloc(1,sizeof(eps_t)); }

/* ───── bal ───── */
/* _XBAL     BAL         balanced parens — matches a "balanced" string:
 * zero or more chars that are not ( or ), or a ( followed by a BAL string
 * followed by ), all concatenated.  Equivalent to the SNOBOL4 primitive BAL.
 * On α: scan from Δ consuming the maximal balanced prefix (may be zero-width).
 * On β: undo the match and fail (no shorter alternative — BAL is deterministic). */

bal_t *bb_bal_new(void)
{ return calloc(1,sizeof(bal_t)); }

/* ───── abort ───── */
/* _XABRT    ABORT       always ω — force match failure */


abort_t *bb_abort_new(void)
{ return calloc(1,sizeof(abort_t)); }

/* ───── not ───── */
/* _XNOT     NOT         \X — succeed iff X fails; β always ω (no retry) */

/* o$nta/b/c three-entry semantics mapped to two-entry BB:
 *   α: run child with α; if child γ → NOT_ω (child succeeded → we fail);
 *                         if child ω → NOT_γ zero-width (child failed → we succeed)
 *   β: unconditional NOT_ω — negation succeeds at most once per position */
typedef struct { bb_box_fn fn; void *state; int start; } not_t;

not_t *bb_not_new(bb_box_fn fn, void *state)
{ not_t *ζ=calloc(1,sizeof(not_t)); ζ->fn=fn; ζ->state=state; return ζ; }

/* ───── interr ───── */
/* _XINT     INTERR      ?X — null result if X succeeds; ω if X fails (o$int) */

/* o$int: replace operand with null, continue.
 * In BB terms: run child; if child γ → discard match, return zero-width at
 * the *original* cursor (null string); if child ω → propagate ω.
 * β: unconditional ω — interrogation succeeds at most once. */
typedef struct { bb_box_fn fn; void *state; int start; } interr_t;

interr_t *bb_interr_new(bb_box_fn fn, void *state)
{ interr_t *ζ=calloc(1,sizeof(interr_t)); ζ->fn=fn; ζ->state=state; return ζ; }

/* ───── capture ───── */
/* _XNME/_XFNME  CAPTURE     $ writes on every γ; . buffers for Phase-5 commit
 *
 * UNIFIED 2026-04-19 (SN-20 session 17): previously had two copies
 * (bb_boxes.c and stmt_exec.c). Consolidated here as the single source
 * of truth across --ir-run, --sm-run, and --jit-run.
 *
 * Fields:
 *   varname   — DT_S target name (write via NV_SET_fn — fires I/O hooks for OUTPUT/PUNCH)
 *   var_ptr   — DT_N target pointer (write directly through ptr — SIL NAME semantics)
 *   immediate — 1 for XFNME ($): write now on every γ.  0 for XNME (.): defer via NAM_push.
 *
 * SN-20 (self-unwinding): every γ that pushes to the NAM list saves the
 * returned handle; β (retry) and ω (failure exit) call NAME_pop to undo.
 * No external combinator-level NAME_mark / NAME_rollback_to required — the
 * box is symmetric in its own right.
 *
 * cap_t definition now in bb_box.h so the stmt_exec.c dispatcher can
 * allocate state directly (mirrors other box struct exposure pattern).
 */

/* forward decl — used in bb_cap body below */
static void register_capture(cap_t *c);

/* SN-21c: bb_cap — unified (.) / ($) capture box.
 *
 * State is cap_t with an embedded NAME_t; immediate ($) writes route through
 * name_commit_value, deferred (.) writes push via NAME_push at γ and are
 * popped by NAME_pop on β / ω so the flat NAM stack self-unwinds.
 *
 * Registry (g_capture_list) and has_pending bookkeeping are retained until
 * SN-21e cleanup — they keep statement-boundary resets correct regardless
 * of whether a box ever completed its γ/β/ω handshake. */

/* Capture registry (moved from stmt_exec.c — used by exec_stmt for Phase-5 reset).
 * MAX_CAPTURES raised from 64 to 256 to match stmt_exec.c's original value. */
#define MAX_CAPTURES 256
static cap_t *g_capture_list[MAX_CAPTURES];
static int    g_capture_count = 0;

/* Called from bb_cap CAP_α whenever a conditional (.) capture fires.
 * Also callable from bb_build for eager registration if desired. */
static void register_capture(cap_t *c)
{
    for (int i = 0; i < g_capture_count; i++)
        if (g_capture_list[i] == c) return;
    if (g_capture_count < MAX_CAPTURES)
        g_capture_list[g_capture_count++] = c;
}

/* Reset pending flags after Phase 3 success.
 * RT-4: NAME_commit() now owns all conditional (.) capture writes.
 * This function only clears has_pending bookkeeping so the scan-loop
 * reset logic stays correct on subsequent statements.
 * Exported for exec_stmt (see external decl in bb_box.h). */
void flush_pending_captures(void)
{
    for (int i = 0; i < g_capture_count; i++)
        g_capture_list[i]->has_pending = 0;
    g_capture_count = 0;
}

/* Called at start of exec_stmt to clear the registry for a fresh statement. */
void reset_capture_registry(void)
{
    g_capture_count = 0;
}

/* Called before the scan sweep to clear stale has_pending flags without
 * emptying the registry itself. RT-4 equivalent of the inline loop that
 * previously lived in exec_stmt. */
void clear_pending_flags(void)
{
    for (int i = 0; i < g_capture_count; i++)
        g_capture_list[i]->has_pending = 0;
}

/* Unified constructor — external linkage; called from stmt_exec.c bb_build
 * dispatcher and from bb_build.c JIT emitter.  Signature matches the
 * cap_t_bin mirror in bb_build.c (var_ptr is void* there, DESCR_t* here).
 * Builds the embedded NAME_t via name_init_as_{ptr,var} so no call site
 * constructs a NAME_t by hand. */
cap_t *bb_cap_new(bb_box_fn child_fn, void *child_state,
                  const char *varname, DESCR_t *var_ptr, int immediate)
{
    cap_t *ζ = calloc(1, sizeof(cap_t));
    if (!ζ) return NULL;
    ζ->fn        = child_fn;
    ζ->state     = child_state;
    ζ->immediate = immediate;
    if (var_ptr)            name_init_as_ptr(&ζ->name, var_ptr);
    else if (varname)       name_init_as_var(&ζ->name, varname);
    /* else: name.t==NM_VAR, var_name==NULL — name_commit_value / push are
     * safe no-ops on empty names (mirrors previous varname==NULL behaviour). */
    return ζ;
}

/* SN-21d: NM_CALL constructor for `pat . *fn(args)` (XCALLCAP).
 *
 * Same bb_cap state machine as NM_VAR / NM_PTR — the only difference is the
 * embedded NAME_t's kind, which routes the commit through name_commit_value's
 * NM_CALL branch.  No separate box function, no separate registry, no
 * per-firing cc_event bookkeeping: the flat NAM stack already supplies all of
 * that via NAME_push / NAME_pop γ / β / ω self-unwinding.
 *
 * Deferred (.) flow at commit time:
 *   NAME_commit walks live slots → name_commit_value(NM_CALL) →
 *   g_user_call_hook(fnc_name, args, nargs) → DT_N cell → store matched text.
 *
 * Immediate ($) flow at γ:
 *   name_commit_value(NM_CALL, matched_text) — fires the hook on every γ.
 *   (SPITBOL semantics: immediate assignment fires on every γ, even if the
 *   outer match later fails.) */
cap_t *bb_cap_new_call(bb_box_fn child_fn, void *child_state,
                       const char *fnc_name,
                       DESCR_t *fnc_args, int fnc_nargs,
                       char **fnc_arg_names, int fnc_n_arg_names,
                       int immediate)
{
    cap_t *ζ = calloc(1, sizeof(cap_t));
    if (!ζ) return NULL;
    ζ->fn        = child_fn;
    ζ->state     = child_state;
    ζ->immediate = immediate;
    name_init_as_call(&ζ->name, fnc_name,
                      fnc_args, fnc_nargs,
                      fnc_arg_names, fnc_n_arg_names);
    return ζ;
}

/* ───── atp ───── */
/* _XATP     ATP         @var — write cursor Δ as DT_I into varname; no backtrack */


atp_t *bb_atp_new(const char *varname)
{ atp_t *ζ=calloc(1,sizeof(atp_t)); ζ->varname=varname; return ζ; }

/* ───── dvar ───── */
/* ───── fence ───── */
/* _XFNCE    FENCE       succeed once; β cuts (no retry) */


fence_t *bb_fence_new(void)
{ return calloc(1,sizeof(fence_t)); }

/* ───── fail ───── */
/* _XFAIL    FAIL        always ω — force backtrack */


fail_t *bb_fail_new(void)
{ return calloc(1,sizeof(fail_t)); }

/* ───── rpos ───── */
/* _XRPSI    RPOS        assert cursor == Σlen-n (zero-width) */


rpos_t *bb_rpos_new(int n)
{ rpos_t *ζ=calloc(1,sizeof(rpos_t)); ζ->n=n; return ζ; }

/* ───── rtab ───── */
/* _XRTB     RTAB        advance cursor TO position Σlen-n */


rtab_t *bb_rtab_new(int n)
{ rtab_t *ζ=calloc(1,sizeof(rtab_t)); ζ->n=n; return ζ; }

/* ───── succeed ───── */
/* _XSUCF    SUCCEED     always γ zero-width; outer loop retries */


succeed_t *bb_succeed_new(void)
{ return calloc(1,sizeof(succeed_t)); }

/* ───── Icon range (to / to-by) ───── */
/* Allocators — zeta is a pointer, allocation site doesn't matter to the box. */

#include "../../frontend/icon/icon_gen.h"

icn_to_state_t *icon_to_new(void)
{ return calloc(1, sizeof(icn_to_state_t)); }

icn_to_by_state_t *icon_to_by_new(void)
{ return calloc(1, sizeof(icn_to_by_state_t)); }

/* ───── Icon iterate (!E) ───── */
icn_iterate_state_t     *icon_iterate_new(void)        { return calloc(1, sizeof(icn_iterate_state_t)); }
icn_list_iterate_state_t *icon_list_iterate_new(void)  { return calloc(1, sizeof(icn_list_iterate_state_t)); }
icn_tbl_iterate_state_t  *icon_tbl_iterate_new(void)   { return calloc(1, sizeof(icn_tbl_iterate_state_t)); }
icn_record_iterate_state_t *icon_record_iterate_new(void) { return calloc(1, sizeof(icn_record_iterate_state_t)); }

/* ───── Icon alternate (A|B) ───── */
icn_alternate_state_t *icon_alt_new(void) { return calloc(1, sizeof(icn_alternate_state_t)); }

/* ───── Icon every / limit / bang / lconcat / seq ───── */
icn_every_state_t       *icon_every_new(void)   { return calloc(1, sizeof(icn_every_state_t)); }
icn_limit_state_t       *icon_limit_new(void)   { return calloc(1, sizeof(icn_limit_state_t)); }
icn_bang_binary_state_t *icon_bang_new(void)    { return calloc(1, sizeof(icn_bang_binary_state_t)); }
void                    *icon_lconcat_new(void) { return calloc(1, sizeof(icn_cat_gen_state_t)); }
icn_seq_state_t         *icon_seq_new(void)     { return calloc(1, sizeof(icn_seq_state_t)); }
