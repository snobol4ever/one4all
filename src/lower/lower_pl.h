#ifndef LOWER_PL_H
#define LOWER_PL_H
#include "IR.h"
struct tree_t;
/* lower_pl_predicate — build IR_block_t* for a single Prolog predicate (TT_CHOICE node).  */
/* Returns NULL when the predicate cannot be lowered yet — caller keeps g_dcg_table entry   */
/* with ir_body==NULL and falls back to legacy SM/AST path via pl_bb_dcg.                  */
IR_block_t *lower_pl_predicate(struct tree_t *choice);
#endif
