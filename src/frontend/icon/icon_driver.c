#include "icon_driver.h"
#include "icon_lex.h"
#include "icon_parse.h"
#include "../snobol4/scrip_cc.h"
#include <stdio.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void icon_compile(const char *source, const char *filename, tree_t **out_ast) {
    if (!filename) filename = "<stdin>";
    if (out_ast) *out_ast = NULL;
    IcnLexer lx;
    icn_lex_init(&lx, source);
    IcnParser parser;
    icn_parse_init(&parser, &lx);
    CODE_t *prog = icn_parse_file(&parser, out_ast);
    if (parser.had_error) {
        fprintf(stderr, "icon: parse error in %s: %s\n", filename, parser.errmsg);
        free(prog);
        return;
    }
    (void)prog;
}
