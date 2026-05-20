#ifndef LOWER_PL_H
#define LOWER_PL_H
#include "BB.h"
struct tree_t;
/* lower_pl_predicate — build BB_graph_t* for a single Prolog predicate (TT_CHOICE node).  */
/* Returns NULL when the predicate cannot be lowered yet — caller keeps g_dcg_table entry   */
/* with ir_body==NULL and falls back to legacy SM/AST path via pl_bb_dcg.                  */
BB_graph_t *lower_pl_predicate(struct tree_t *choice);
#endif
