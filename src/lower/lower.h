#ifndef LOWER_H
#define LOWER_H
#include "SM.h"
#include "../../ast/ast.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* lower — the parse->lower boundary carries exactly one thing: the AST.                                                                                                                              */
/* Pre-lower sidecar build (label_table, proc_table, g_pl_pred_table) happens at the top of lower() via polyglot_init.  Callers do not invoke label_table_build / prescan_defines / polyglot_init.    */
SM_sequence_t *lower(const tree_t *prog);
#endif
