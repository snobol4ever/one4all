#ifndef DRIVER_PL_RUNTIME_H
#define DRIVER_PL_RUNTIME_H
#include "../ast/ast.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include "../../frontend/prolog/prolog_driver.h"
#include "../../frontend/prolog/term.h"
#include "../../frontend/prolog/prolog_runtime.h"
#include "bb_broker.h"
#include "BB.h"
#include "SM.h"
#include "stage2.h"
#define PL_PRED_TABLE_SIZE_FWD STAGE2_PL_PRED_TABLE_SIZE
#define PL_SCOPE_SLOT_MAX       64
#define PL_BB_TABLE_MAX       256
typedef struct Pl_PredEntry_t {
    const char *key; tree_t *choice; struct Pl_PredEntry_t *next;
    int entry_pc;
} Pl_PredEntry;
/* Pl_PredTable is defined canonically in stage2.h.  Its single instance lives
 * inside the active stage2_t (s2_pl_pred_table).  See reader shim below.    */
typedef struct { const char *name; int slot; } PlScopeEnt;
typedef struct { PlScopeEnt e[PL_SCOPE_SLOT_MAX]; int n; } PlScope;
/* IR-CONSOLIDATE-DCG step 5: ir_body field deleted (2026-05-20).  The BB graph is reached
 * via bb_idx into g_stage2.sm.bb_table.  Mode-4 standalone-binary path (rt_pl_b_end_register)
 * lazy-inits g_stage2.sm and gets a real bb_idx, so there is no fallback. */
typedef struct { const char *name; int arity; int bb_idx; PlScope lower_sc; } Pl_PredEntry_BB;
extern Pl_PredEntry_BB g_pl_bb_table[PL_BB_TABLE_MAX];
extern int             g_pl_bb_count;
/* IR-CONSOLIDATE-DCG step 5 (2026-05-20): single-structure lookup, no fallback. */
static inline BB_graph_t *bb_graph_of_pred(const Pl_PredEntry_BB *e)
{
    if (!e) return NULL;
    if (e->bb_idx >= 0 && e->bb_idx < g_stage2.sm.bb_count)
        return g_stage2.sm.bb_table[e->bb_idx];
    return NULL;
}
/* ST2-1b (2026-05-20): g_pl_pred_table shim macro deleted.  polyglot_init writes through
 * s2->pl_pred_table; pl_runtime.c / scrip_sm.c / interp_hooks.c / lower.c readers use
 * g_stage2.pl_pred_table literally (deep dispatch-loop sites don't carry an s2 parameter). */
extern Trail         g_pl_trail;
extern int           g_pl_cut_flag;
extern Term        **g_pl_env;
extern int           g_pl_active;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *pl_pred_table_lookup(Pl_PredTable *pt, const char *key);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void    pl_pred_table_insert(Pl_PredTable *pt, const char *key, tree_t *choice);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
tree_t *pl_pred_table_lookup_global(const char *key);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Pl_PredEntry *pl_pred_entry_lookup(const char *key);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Term  **pl_env_new(int n);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Term   *pl_unified_term_from_expr(tree_t *e, Term **env);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int     is_pl_user_call(tree_t *goal);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int     interp_exec_pl_builtin(tree_t *goal, Term **env);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void    pl_execute_program_unified(CODE_t *prog);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t pl_bb_dcg(void *zeta, int entry);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Pl_PredEntry_BB *pl_bb_lookup(const char *name, int arity);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Pl_PredEntry_BB *pl_bb_register(const char *name, int arity, int bb_idx);
bb_node_t pl_bb_once_proc_by_name(const char *name, int arity);
void pl_bb_env_push(int nslots);
void pl_bb_env_pop(Term **saved);
#endif
