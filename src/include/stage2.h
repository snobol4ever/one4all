#ifndef STAGE2_H
#define STAGE2_H
/*══════════════════════════════════════════════════════════════════════════════════════════════════
 * stage2.h — Stage 2 handoff (lower → interp / emit)
 *
 *   parser → tree_t * → lower → stage2_t * → interp | emit
 *           [stage 1]          [stage 2]
 *
 * stage2_t is the single output of lower().  It carries everything the
 * downstream consumers need:
 *
 *   1. The opcode + BB-graph stream (originally SM_sequence_t).
 *   2. Every category-B table that lower's pre-pass builds:
 *        - label_table  + label_count        (SNO statement labels)
 *        - proc_table   + proc_count         (Icon/Raku procedures)
 *        - pl_pred_table                     (Prolog predicate clauses)
 *        - module_registry                   (per-language module spans)
 *        - lang                              (program-level primary language)
 *
 *   These were process globals before ST2-1.  Now they are FIELDS of
 *   stage2_t.  Lower writes them into the struct from the top of lower();
 *   readers reach the same storage through legacy macro shims (see below).
 *
 * What does NOT live in stage2_t
 *   - Lower's internal pass state (g_p, g_labtab, g_in_proc_body, …):
 *     stays file-scope static in lower.c.  Per SR-15c (LowerCtx rollback),
 *     do not parameter-thread these.
 *   - Runtime-mutable state (g_icn_root, frame_stack, scan_pos, g_pl_cut_flag,
 *     g_pl_env, g_pl_active, hook fn-pointers): owned by the runtime, not by
 *     lower's output.  A separate runtime-state struct may absorb these later.
 *
 * Legacy reader shim
 *   To keep the diff small and focused on the writer side, the old global
 *   names (label_table, proc_table, g_pl_pred_table, g_registry) are
 *   redefined as macros that resolve to fields of (&g_stage2) — see the
 *   relevant runtime headers (interp_private.h, icn_runtime.h, pl_runtime.h,
 *   interp.h).  These shims are temporary; ST2-1b sweeps every reader to
 *   take `const stage2_t *s2` explicitly, then deletes the macros.
 *
 *   For the shim to work, exactly one stage2_t is "current" — the one lower
 *   is building or the one a runner is running.  (&g_stage2) is set by
 *   lower() at the top of the build pass and by sm_preamble after lower
 *   returns; both happen on the same single-threaded path.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.7
 *═════════════════════════════════════════════════════════════════════════════════════════════════*/
#include <stdint.h>
#include "SM.h"
#include "../ast/ast.h"

#define STAGE2_LABEL_MAX           4096
#define STAGE2_PROC_TABLE_MAX       256
#define STAGE2_PL_PRED_TABLE_SIZE   256
#define STAGE2_MOD_MAX               64
#define STAGE2_FRAME_SLOT_MAX        64

/*── Sidecar entry types.  These are the canonical, single-definition forms.
 *   interp_private.h / icn_runtime.h / pl_runtime.h / interp.h re-export the
 *   short names (LabelEntry, IcnProcEntry, …) by typedef alias so existing
 *   readers compile unchanged.                                              */

typedef struct LabelEntry {
    const char    *name;
    const tree_t  *stmt;
} LabelEntry;

typedef struct IcnScopeEnt { const char *name; int slot; } IcnScopeEnt;
typedef struct IcnScope    { IcnScopeEnt e[STAGE2_FRAME_SLOT_MAX]; int n; } IcnScope;

struct BB_graph_t;
typedef struct IcnProcEntry {
    const char         *name;
    tree_t             *proc;
    int                 entry_pc;
    int                 nparams;
    IcnScope            lower_sc;
    int                 bb_idx;
    int                 is_generator;
} IcnProcEntry;

struct Pl_PredEntry_t;
typedef struct Pl_PredTable {
    struct Pl_PredEntry_t *buckets[STAGE2_PL_PRED_TABLE_SIZE];
} Pl_PredTable;

typedef struct ScripModule {
    int           lang;
    const char   *name;
    const tree_t *first;
    const tree_t *last;
    int           nstmts;
    int           sno_label_start;
    int           sno_label_count;
    int           icn_proc_start;
    int           nprocs;        /* renamed from proc_count to avoid macro collision with the shim */
} ScripModule;

typedef struct ScripModuleRegistry {
    ScripModule mods[STAGE2_MOD_MAX];
    int         nmod;
    int         main_mod;
} ScripModuleRegistry;

/*── The handoff struct ────────────────────────────────────────────────────
 * stage2_t is the baton lower() hands to interp/emit.  Defined once as the
 * global variable g_stage2 below — there is one Stage 2 build target at a
 * time (lower() is not reentrant; see SR-15c).
 *
 * Fields:
 *   sm                       — the SM_sequence_t (the opcode array + BB graphs)
 *   label_table/count/cap    — SNO statement labels (dynamic, grows via _grow)
 *   proc_table/count/cap     — Icon/Raku procedures (dynamic, grows via _grow)
 *   pl_pred_table            — Prolog predicate clauses (hash table)
 *   module_registry          — per-language module spans
 *   lang                     — program-level primary language
 *
 * ST2-1c (2026-05-20): label_table and proc_table are dynamically-grown.
 * `STAGE2_LABEL_MAX` / `STAGE2_PROC_TABLE_MAX` are kept as initial-capacity
 * hints (sm_seq_init / stage2_reset pattern) — no longer hard limits.  The
 * `_grow` pattern mirrors `SM_sequence_t.instrs` (sm_prog.c:_grow).
 *
 * Reader sites all use `[i]` indexing, so the pointer-vs-array change is
 * transparent at every call site.  Writer sites that previously checked
 * `< LABEL_MAX` / `< PROC_TABLE_MAX` now call grow helpers instead.        */
typedef struct stage2_t {
    SM_sequence_t        sm;                                            /* the SM opcode array (and what the array needs) */
    LabelEntry          *label_table;                                   /* SNO statement labels (dynamic; see ST2-1c) */
    int                  label_count;
    int                  label_cap;
    IcnProcEntry        *proc_table;                                    /* Icon/Raku procedures (dynamic; see ST2-1c) */
    int                  proc_count;
    int                  proc_cap;
    Pl_PredTable         pl_pred_table;                                 /* Prolog predicate clauses */
    ScripModuleRegistry  module_registry;                               /* per-language module spans */
    int                  lang;                                          /* program-level primary language */
} stage2_t;

/*── The one and only Stage 2 baton.  Reader shims (interp.h,
 *   interp_private.h, icn_runtime.h, pl_runtime.h) resolve legacy names
 *   (label_table, proc_table, g_pl_pred_table, g_registry) into fields of
 *   g_stage2.  Single-threaded path; no locking needed.                     */
extern stage2_t g_stage2;

/*── Reset the build target to a clean state (called by lower() at the top
 *   of the pass).  Clears the embedded SM_sequence_t's dynamic arrays and
 *   zeros all sidecar counts.                                               */
void stage2_reset(void);

/*── Grow helpers for the dynamic sidecar arrays.  Both return the index of
 *   the newly-reserved slot; the slot is zero-initialized.  Callers fill in
 *   the fields after the call.  ST2-1c: replaces the hard-capped append-
 *   guards (`if (s2->proc_count < PROC_TABLE_MAX)` etc.) at writer sites.  */
int  stage2_label_grow(stage2_t *s2);
int  stage2_proc_grow (stage2_t *s2);

#endif
