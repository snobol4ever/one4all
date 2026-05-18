/* interp_ast_stubs.c — NO-AST stubs for symbols formerly in interp_exec.c (deleted CLI-3M-9).
 * interp_exec.c was the only truly pure-AST-walk file deleted; all others kept.
 * g_exec_prog: global formerly set in interp_exec.c, needed by interp_call.c.
 * execute_program_steps: stub — mode-1 AST-loop, unreachable from CLI.
 */
#include "snobol4.h"
#include "frontend/snobol4/scrip_cc.h"
#include <stdio.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
const tree_t *g_exec_prog = NULL;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void execute_program_steps(const tree_t *prog, int n) { (void)prog; (void)n; fprintf(stderr, "[NO-AST] execute_program_steps stub\n"); }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t interp_eval(tree_t *e) { (void)e; fprintf(stderr, "[NO-AST] interp_eval stub\n"); return FAILDESCR; }
