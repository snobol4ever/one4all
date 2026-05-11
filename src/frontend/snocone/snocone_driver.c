/*
 * snocone_driver.c — Snocone frontend pipeline driver
 *
 * snocone_compile(source, filename) → CODE_t*
 *
 * LS-4.j: delegates directly to snocone_parse_program() — the Bison
 * parser + FSM lexer (snocone_parse.tab.c / snocone_lex.c).
 *
 * Prior to LS-4.j this called snocone_control_compile() (the legacy
 * Sprint-SC1 shunting-yard pipeline via archive/snocone_parse.c and
 * archive/snocone_lex.c).  LS-4.k moved snocone_lower.c / snocone_control.c
 * to archive/ and removed all archive/snocone entries from FRONTEND_SNOCONE.
 */

#include "snocone_driver.h"
#include <stdio.h>

/* Forward declaration — defined in snocone_parse.tab.c */
CODE_t *snocone_parse_program(const char *src, const char *filename);

CODE_t *snocone_compile(const char *source, const char *filename, AST_t **out_ast)
{
    if (!filename) filename = "<stdin>";
    if (out_ast) *out_ast = NULL;
    CODE_t *prog = snocone_parse_program(source, filename);
    /* SI-5: build AST_PROGRAM from CODE_t so sm_preamble uses native tree. */
    if (out_ast && prog) *out_ast = code_to_ast(prog);
    return prog;
}
