/*
 * snocone_driver.c — Snocone frontend pipeline driver
 *
 * snocone_compile(source, filename, out_ast) — sets *out_ast to AST_PROGRAM.
 *
 * Internally uses the Bison/FSM parser (snocone_parse.tab.c / snocone_lex.c)
 * which still builds a Prog_t (linked Stmt_t list); code_to_ast() wraps that
 * into AST_PROGRAM.  Direct AST emission deferred to GOAL-SNOCONE-SM-LOWER.
 */

#include "snocone_driver.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include <stdio.h>

/* Forward declaration — defined in snocone_parse.tab.c */
CODE_t *snocone_parse_program(const char *src, const char *filename);

void snocone_compile(const char *source, const char *filename, tree_t **out_ast)
{
    if (!filename) filename = "<stdin>";
    if (out_ast) *out_ast = NULL;
    CODE_t *prog = snocone_parse_program(source, filename);
    if (out_ast && prog) *out_ast = code_to_ast(prog);
}
