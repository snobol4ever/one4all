/*
 * bb_box.h — Dynamic Byrd Box Runtime Types (M-DYN-2)
 *
 * THE CANONICAL FORM — one C function per box, three-column layout:
 *
 *     LABEL:          ACTION                          GOTO
 *     ─────────────────────────────────────────────────────
 *     BIRD_α:         if (Σ[Δ+0] != 'B')             goto BIRD_ω;
 *                     BIRD = str(Σ+Δ, 4); Δ += 4;    goto BIRD_γ;
 *     BIRD_β:         Δ -= 4;                         goto BIRD_ω;
 *     BIRD_γ:         return BIRD;
 *     BIRD_ω:         return empty;
 *
 * Reference implementations: .github/test_sno_*.c, .github/test_icon.c
 *
 * Every box is:
 *   str_t BoxName(boxname_t **ζζ, int entry);
 *
 * entry == α (0): fresh entry (allocate state ζ, go to α port)
 * entry == β (1): re-entry for backtracking (go to β port)
 * return is_empty(): ω fired (failure)
 * return non-empty: γ fired (success, value = matched substring)
 *
 * Global match state (shared, like SNOBOL4's implicit subject):
 *   Σ  — subject string pointer
 *   Δ  — cursor (current match position)
 *   Ω  — subject length
 */

#ifndef BB_BOX_H
#define BB_BOX_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "descr.h"   /* DESCR_t, DTYPE_t, FAILDESCR, IS_FAIL_fn — U-5 */
#include "name_t.h"  /* NAME_t, NameKind_t, name_commit_value — SN-21 */

/* ── str_t — the universal value type ──────────────────────────────────── */
/*
 * str_t represents a substring of the subject (or the empty/fail sentinel).
 * σ = pointer into subject string, δ = length.
 * empty (σ == NULL) signals failure (ω port fired).
 * This matches test_sno_*.c exactly.
 */
typedef struct { const char *σ; int δ; } spec_t;

/* The failure sentinel */
static const spec_t spec_empty = { (const char *)0, 0 };

/* Construct a str_t from pointer and length */
static inline spec_t spec(const char *σ, int δ) { return (spec_t){ σ, δ }; }

/* Concatenate two substrings (they must be contiguous in the subject) */
static inline spec_t spec_cat(spec_t x, spec_t y)   { return (spec_t){ x.σ, x.δ + y.δ }; }

/* Test for failure sentinel */
static inline bool spec_is_empty(spec_t x)           { return x.σ == (const char *)0; }

/* ── entry ports ────────────────────────────────────────────────────────── */
static const int α = 0;   /* fresh entry */
static const int β = 1;   /* backtrack re-entry */
#define BB_ALPHA_DEFINED 1

/* ── global match state ─────────────────────────────────────────────────── */
/*
 * These are set by the statement executor (Phase 1: build subject) and
 * shared across all boxes during a single match.  Cursor Δ is mutated
 * by each box as it matches forward; restored on backtrack.
 */
/* Σ/Δ/Ω/Σlen are defined (non-static) in stmt_exec.c.
 * All bb_*.c files declare them extern here to resolve at link time. */
extern const char *Σ;   /* subject string */
extern int         Δ;   /* cursor */
extern int         Ω;   /* max scan-start position (= Σlen normally; clamped to 0 by kw_anchor) */
extern int         Σlen; /* true subject length — use for bounds checks inside box fns */

/* ── state allocator (from test_sno_3.c §enter) ─────────────────────────── */
/*
 * Each box that needs per-invocation state (cursor saves, loop vars)
 * gets a typed struct ζ allocated on first α entry and reused on β.
 * This matches the enter() pattern from test_sno_3.c exactly.
 */
static inline void *bb_enter(void **ζζ, size_t size) {
    void *ζ = *ζζ;
    if (size) {
        if (ζ) memset(ζ, 0, size);
        else   ζ = *ζζ = calloc(1, size);
    }
    return ζ;
}
#define BB_ENTER(ref, T)  ((T *)bb_enter((void **)(ref), sizeof(T)))

/* ── box function signature ──────────────────────────────────────────────── */
/*
 * Every box is called as:
 *   spec_t result = BoxName(&ζ, α);   // fresh
 *   spec_t result = BoxName(&ζ, β);   // backtrack
 *
 * Caller checks is_empty(result) to know if γ or ω fired.
 * The λ dispatch idiom (from test_sno_3.c):
 *
 *   BoxName_α:  BoxName = Box(&ζ->Box_ζ, α);     goto BoxName_λ;
 *   BoxName_β:  BoxName = Box(&ζ->Box_ζ, β);     goto BoxName_λ;
 *   BoxName_λ:  if (is_empty(BoxName))            goto BoxName_ω;
 *               else                              goto BoxName_γ;
 */
typedef DESCR_t (*bb_box_fn)(void *zeta, int entry);   /* U-5: was spec_t — now unified with icn_box_fn */

/* bb_node_t — a built box instance (fn + allocated state) */
typedef struct {
    bb_box_fn  fn;
    void      *ζ;
    size_t     ζ_size;
} bb_node_t;

/* ── box state typedefs — ONE definition, used by bb_*.c, stmt_exec.c, bb_*.s ─
 * Each box's private state struct lives here.  bb_build() in stmt_exec.c
 * allocates these; the box functions cast zeta to the appropriate type.
 * Named with _t suffix; the .s files use field offsets matching these layouts. */
typedef struct { const char *lit; int len; }          lit_t;
typedef struct { int n; }                              len_t;
typedef struct { const char *chars; int δ; }          span_t;
typedef struct { const char *chars; }                  any_t;
typedef struct { const char *chars; }                  notany_t;
typedef struct { const char *chars; int δ; }          brk_t;
typedef struct { const char *chars; int δ; }          brkx_t;
typedef struct { int count; int start; }              arb_t;
typedef struct { int dummy; }                          rem_t;
typedef struct { int dummy; }                          succeed_t;
typedef struct { int dummy; }                          fail_t;
typedef struct { int done; }                           eps_t;
typedef struct { int n; }                              pos_t;
typedef struct { int n; }                              rpos_t;
typedef struct { int n; int advance; }                tab_t;
typedef struct { int n; int advance; }                rtab_t;
typedef struct { int fired; }                          fence_t;
typedef struct { int dummy; }                          abort_t;
typedef struct { int δ; }                              bal_t;
typedef struct { int done; const char *varname; }     atp_t;
/* deferred_var_t needs bb_node_t — defined above */

/* cap_t — SN-21c unified capture state; SN-21d extended to NM_CALL;
 * SN-21e made canonical (all legacy siblings deleted).
 *
 * One struct, one box (bb_cap) handles XNME (.), XFNME ($), and XCALLCAP
 * for every lvalue kind.  The pre-SN-21 fracture — capture_t +
 * callcap_t with separate state machines — is gone.  NameKind_t
 * dispatch happens inside name_commit_value at commit time.
 *
 *   fn / state  — child box
 *   immediate   — 1 for XFNME ($): write at γ via name_commit_value
 *                 0 for XNME / XCALLCAP (.): push at γ via NAME_push;
 *                   commit at NAME_commit via name_commit_value
 *   name        — unified lvalue descriptor (NM_VAR / NM_PTR / NM_CALL
 *                   / NM_IDX reserved)
 *   pending / has_pending / registered — kept for statement-level
 *                 bookkeeping so clear_pending_flags() and the
 *                 capture-registry hooks stay correct across statements.
 *
 * SN-23d: nam_handle field deleted — bb_cap β/ω now drops the top of the
 * current NAM ctx via NAME_pop_top (pure LIFO; no handle required). */
typedef struct cap_s {
    bb_box_fn    fn;
    void        *state;
    int          immediate;
    NAME_t       name;
    spec_t       pending;
    int          has_pending;
    int          registered;
} cap_t;

/* External box-function + helper declarations — all live in bb_boxes.c
 * (single source across --ir-run, --sm-run, --jit-run). Callers: stmt_exec.c
 * bb_build dispatcher, bb_build.c JIT emitter. DESCR_t visible via descr.h above. */

extern DESCR_t bb_cap(void *zeta, int entry);
extern cap_t *bb_cap_new(bb_box_fn child_fn, void *child_state,
                         const char *varname, DESCR_t *var_ptr, int immediate);
/* SN-21d: NM_CALL constructor for `pat . *fn(args)` and `pat $ *fn(args)`.
 * Builds the embedded NAME_t via name_init_as_call.  Deferred (.) firings push
 * an NM_CALL entry onto the flat NAM stack; NAME_commit runs them through
 * name_commit_value, which invokes g_user_call_hook and writes matched text
 * into the returned DT_N cell. */
extern cap_t *bb_cap_new_call(bb_box_fn child_fn, void *child_state,
                              const char *fnc_name,
                              DESCR_t *fnc_args, int fnc_nargs,
                              char **fnc_arg_names, int fnc_n_arg_names,
                              int immediate);
extern void      flush_pending_captures(void);
extern void      reset_capture_registry(void);
extern void      clear_pending_flags(void);

extern DESCR_t bb_atp(void *zeta, int entry);
extern atp_t   *bb_atp_new(const char *varname);

/* bb_build: construct a live box node from a pattern tree node.
 * Defined in stmt_exec.c; declared here so bb_boxes.c can call it. */
struct _PATND_t;  /* forward declaration */
bb_node_t bb_build(struct _PATND_t *p);

/* U-1: universal box function type and broker mode ───────────────────── */
/*
 * univ_box_fn — the one true box signature for all five languages.
 * Returns DESCR_t (16 bytes, rax:rdx) — subsumes spec_t (also 16 bytes).
 * SNOBOL4 boxes currently return spec_t via bb_box_fn; they migrate to
 * univ_box_fn at step U-5. Icon boxes (icn_box_fn) already return DESCR_t.
 * DESCR_t defined in descr.h (included above).
 */
typedef DESCR_t (*univ_box_fn)(void *zeta, int entry);

/* BrokerMode — selects drive behaviour in bb_broker() (added U-3).
 *   BB_SCAN: SNOBOL4 mode — scan cursor positions 0..Ω, stop on first match.
 *   BB_PUMP: Icon mode    — pump all values via body_fn until ω.
 *   BB_ONCE: Prolog mode  — call α once, report γ/ω; OR-box handles retry.
 */
typedef enum { BB_SCAN, BB_PUMP, BB_ONCE } BrokerMode;

#endif /* BB_BOX_H */
