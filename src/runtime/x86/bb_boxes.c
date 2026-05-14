/* bb_boxes.c — All Byrd box C implementations, consolidated
 * Generated from per-box sources. One function per box.
 * AUTHORS: Lon Jones Cherryholmes · Jeffrey Cooper M.D. · Claude Sonnet 4.6
 */
#include "bb_box.h"
/* bb_convert.h removed EST-4 */
#include "snobol4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>







/* ───── arbno ───── */
/* _XARBN    ARBNO       zero-or-more greedy; zero-advance guard; β unwinds stack */

#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#define ARBNO_INIT 8
typedef struct { DESCR_t matched; int start; } arbno_frame_t;
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









/* ───── abort ───── */
/* _XABRT    ABORT       always ω — force match failure */


abort_t *bb_abort_new(void)
{ return calloc(1,sizeof(abort_t)); }





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





/* ───── succeed ───── */
/* _XSUCF    SUCCEED     always γ zero-width; outer loop retries */


succeed_t *bb_succeed_new(void)
{ return calloc(1,sizeof(succeed_t)); }

/* ───── Icon range (to / to-by) ───── */
#include "../../frontend/icon/icon_gen.h"

/* ───── Icon iterate (!E) ───── */
icn_list_iterate_state_t *icon_list_iterate_new(void)  { return calloc(1, sizeof(icn_list_iterate_state_t)); }
icn_tbl_iterate_state_t  *icon_tbl_iterate_new(void)   { return calloc(1, sizeof(icn_tbl_iterate_state_t)); }
icn_record_iterate_state_t *icon_record_iterate_new(void) { return calloc(1, sizeof(icn_record_iterate_state_t)); }
