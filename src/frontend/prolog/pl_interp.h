#ifndef PL_INTERP_H
#define PL_INTERP_H
/*======================================================================================================================
 * pl_interp.h — Prolog interpreter internals exposed for pl_broker.c  (GOAL-PROLOG-BB-BYRD)
 *
 * Symbols promoted from static in scrip.c so that pl_broker.c can implement
 * S-BB-4 (pl_box_clause) and S-BB-5 (pl_box_choice) without duplicating logic.
 *
 * Rules:
 *   - Only add to this header what pl_broker.c actually needs.
 *   - All symbols declared here are defined in scrip.c (non-static).
 *   - Do NOT include this from any file other than pl_broker.c.
 *====================================================================================================================*/

#include "term.h"
#include "prolog_runtime.h"   /* Trail */
#include "scrip_cc.h"         /* EXPR_t */

/*----------------------------------------------------------------------------------------------------------------------
 * g_pl_trail — global Prolog trail (defined in scrip.c)
 * pl_broker.c uses trail_mark() / trail_unwind() directly on this.
 *--------------------------------------------------------------------------------------------------------------------*/
extern Trail g_pl_trail;

/*----------------------------------------------------------------------------------------------------------------------
 * g_pl_cut_flag — set to 1 when ! fires; checked by OR-box (S-BB-5)
 *--------------------------------------------------------------------------------------------------------------------*/
extern int g_pl_cut_flag;

/*----------------------------------------------------------------------------------------------------------------------
 * pl_env_new(n) — allocate a fresh clause env frame of n unbound TT_VAR slots.
 * Caller owns the returned array; free() when done.
 *--------------------------------------------------------------------------------------------------------------------*/
Term **pl_env_new(int n);

/*----------------------------------------------------------------------------------------------------------------------
 * pl_unified_term_from_expr(e, env) — convert an IR term node to a live Term*
 * using env[] for variable slots.  Used in head-unify and body-arg building.
 *--------------------------------------------------------------------------------------------------------------------*/
Term *pl_unified_term_from_expr(EXPR_t *e, Term **env);

/*----------------------------------------------------------------------------------------------------------------------
 * pl_pred_table_lookup_global(key) — look up a predicate by "functor/arity" key
 * in the interpreter's global predicate table.  Returns the E_CHOICE node or NULL.
 * Wraps the static g_pl_pred_table + pl_pred_table_lookup in scrip.c.
 *--------------------------------------------------------------------------------------------------------------------*/
EXPR_t *pl_pred_table_lookup_global(const char *key);

#endif /* PL_INTERP_H */
