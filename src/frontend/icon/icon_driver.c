/*
 * icon_driver.c -- Icon frontend pipeline driver
 *
 * icon_compile(source, filename) -> CODE_t*
 *
 * Pipeline (FI-2: direct IR, no intermediate AST):
 *   icn_lex_init() -> IcnLexer
 *   icn_parse_file() -> CODE_t*   (AST_t/STMT_t built inline)
 *
 * Authors: Claude Sonnet 4.6 (FI-2, 2026-04-14)
 */

#include "icon_driver.h"
#include "icon_lex.h"
#include "icon_parse.h"

#include <stdio.h>
#include <stdlib.h>

/* SI-5: out_ast receives an AST_PROGRAM built in parallel with CODE_t.
 * Pass NULL to discard (polyglot path or callers that don't need AST yet). */
CODE_t *icon_compile(const char *source, const char *filename, AST_t **out_ast) {
    if (!filename) filename = "<stdin>";
    if (out_ast) *out_ast = NULL;

    /* Case policy is a frontend concern (cf. commit 8aa5803b for DATATYPE).
     * Icon preserves identifier spelling; tell the shared runtime to stop
     * folding at name-ingest sites. No user --case-sensitive flag required. */
    sno_set_case_sensitive(1);

    IcnLexer lx;
    icn_lex_init(&lx, source);

    IcnParser parser;
    icn_parse_init(&parser, &lx);
    CODE_t *prog = icn_parse_file(&parser, out_ast);
    if (parser.had_error) {
        fprintf(stderr, "icon: parse error in %s: %s\n", filename, parser.errmsg);
        free(prog);
        return NULL;
    }
    return prog;
}
