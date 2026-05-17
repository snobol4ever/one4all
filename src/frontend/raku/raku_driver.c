#include "raku_driver.h"
#include "../../ast/ast.h"
#include "../snobol4/scrip_cc.h"
#include <stdio.h>
#include <stdlib.h>
extern tree_t *raku_prog_result;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
extern tree_t *raku_parse_string(const char *src);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void raku_compile(const char *src, const char *filename, tree_t **out_ast) {
    if (!filename) filename = "<stdin>";
    if (out_ast) *out_ast = NULL;
    raku_prog_result = NULL;
    tree_t *prog = raku_parse_string(src);
    if (!prog) {
        fprintf(stderr, "raku: parse error in %s\n", filename);
        return;
    }
    if (out_ast) *out_ast = prog;
}
