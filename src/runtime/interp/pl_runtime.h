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
#define PL_PRED_TABLE_SIZE_FWD 256
#define PL_SCOPE_SLOT_MAX       64
#define PL_BB_TABLE_MAX       256
typedef struct Pl_PredEntry_t {
    const char *key; tree_t *choice; struct Pl_PredEntry_t *next;
    int entry_pc;
} Pl_PredEntry;
typedef struct { Pl_PredEntry *buckets[PL_PRED_TABLE_SIZE_FWD]; } Pl_PredTable;
typedef struct { const char *name; int slot; } PlScopeEnt;
typedef struct { PlScopeEnt e[PL_SCOPE_SLOT_MAX]; int n; } PlScope;
/* IR-CONSOLIDATE-DCG step 1: see icn_runtime.h for the parallel IcnProcEntry change. */
typedef struct { const char *name; int arity; BB_graph_t *ir_body; int bb_idx; PlScope lower_sc; } Pl_PredEntry_BB;
extern Pl_PredEntry_BB g_pl_bb_table[PL_BB_TABLE_MAX];
extern int             g_pl_bb_count;
/* IR-CONSOLIDATE-DCG step 3: strangler helper, see icn_runtime.h bb_graph_of_proc. */
static inline BB_graph_t *bb_graph_of_pred(const Pl_PredEntry_BB *e)
{
    if (!e) return NULL;
    if (g_current_SM_seq && e->bb_idx >= 0 && e->bb_idx < g_current_SM_seq->bb_count)
        return g_current_SM_seq->bb_table[e->bb_idx];
    return e->ir_body;
}
extern Pl_PredTable  g_pl_pred_table;
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
Pl_PredEntry_BB *pl_bb_register(const char *name, int arity, BB_graph_t *ir_body);
bb_node_t pl_bb_once_proc_by_name(const char *name, int arity);
void pl_bb_env_push(int nslots);
void pl_bb_env_pop(Term **saved);
#endif
