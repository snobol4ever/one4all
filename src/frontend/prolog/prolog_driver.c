#include "prolog_driver.h"
#include "prolog_parse.h"
#include "prolog_lower.h"
#include <stdio.h>
#include <stdlib.h>

CODE_t *prolog_compile(const char *source, const char *filename, AST_t **out_ast)
{
    if (!filename) filename = "<stdin>";
    if (out_ast) *out_ast = NULL;
    /* Case policy is a frontend concern (cf. commit 8aa5803b for DATATYPE).
     * Prolog preserves identifier spelling (atom names are case-significant);
     * tell the shared runtime to stop folding at name-ingest sites. No user
     * --case-sensitive flag required for .pl files. */
    sno_set_case_sensitive(1);
    PlProgram *pl = prolog_parse(source, filename);
    if (!pl) { fprintf(stderr, "prolog_compile: parse failed for %s\n", filename); return NULL; }
    CODE_t *prog = prolog_lower(pl);
    /* SI-5: build AST_PROGRAM from CODE_t so sm_preamble uses native tree. */
    if (out_ast && prog) *out_ast = code_to_ast(prog);
    return prog;
}
