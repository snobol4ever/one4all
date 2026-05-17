#include "prolog_driver.h"
#include "prolog_parse.h"
#include "prolog_lower.h"
#include "../../frontend/snobol4/scrip_cc.h"
#include <stdio.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void prolog_compile(const char *source, const char *filename, tree_t **out_ast)
{
    if (!filename) filename = "<stdin>";
    if (out_ast) *out_ast = NULL;
    PlProgram *pl = prolog_parse(source, filename);
    if (!pl) { fprintf(stderr, "prolog_compile: parse failed for %s\n", filename); return; }
    CODE_t *prog = prolog_lower(pl);
    if (out_ast && prog) *out_ast = code_to_ast(prog);
}
