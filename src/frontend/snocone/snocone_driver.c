#include "snocone_driver.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include <stdio.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
CODE_t *snocone_parse_program(const char *src, const char *filename);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void snocone_compile(const char *source, const char *filename, tree_t **out_ast)
{
    if (!filename) filename = "<stdin>";
    if (out_ast) *out_ast = NULL;
    CODE_t *prog = snocone_parse_program(source, filename);
    if (out_ast && prog) *out_ast = code_to_ast(prog);
}
