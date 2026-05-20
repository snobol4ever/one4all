#ifndef LOWER_H
#define LOWER_H
#include "SM.h"
#include "stage2.h"
#include "../../ast/ast.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* lower — Stage 1 → Stage 2.  Input: const tree_t *prog (the AST).  Output: stage2_t * (the baton).                                                                                                  */
/* Everything downstream needs (SM instrs, BB graphs, label_table, proc_table, pl_pred_table, module_registry, lang) lives inside the returned stage2_t.  See stage2.h.                                */
/* During the build, (&g_stage2) points at the in-progress s2 so legacy reader macros resolve correctly.  After lower() returns, (&g_stage2) still points at the returned s2.                */
stage2_t *lower(const tree_t *prog);
#endif
