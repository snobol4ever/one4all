#include "icn_stmt.h"
#include "icn_value.h"
#include "icn_runtime.h"
#include "snobol4.h"
#include <stdio.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* DAI (IJ-DEL-ICN-AST): bb_exec_stmt body removed. The Icon-specific tree_t* AST walker is being */
/* amputated. Mode-1 (--ir-run / --ast-run) is no longer a valid Icon execution path. Use         */
/* --sm-run / --jit-run / --sm-native (modes 2/3/4) for Icon programs.                            */
/* This stub is a "loud bomb" — if any code path still reaches bb_exec_stmt, it prints the AST    */
/* tag that tried to fire and exits non-zero so the regression is impossible to miss.             */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void bb_exec_stmt(tree_t *e)
{
    fprintf(stderr, "[DAI-BOMB] bb_exec_stmt called (mode-1 Icon AST walker is amputated). "
                    "tree tag=%d. Use --sm-run/--jit-run/--sm-native instead.\n",
                    e ? (int)e->t : -1);
    exit(78);  /* EX_CONFIG — wrong configuration; mode-1 invalid for Icon. */
}
