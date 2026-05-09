/*
 * raku_driver.c — Tiny-Raku compiler pipeline driver
 *
 * FI-3: parse → CODE_t* directly, no intermediate RakuNode AST.
 * raku_compile() calls raku_parse_string() which runs the Bison parser;
 * grammar actions build AST_t/STMT_t inline and populate raku_prog_result.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */
#include "raku_driver.h"
#include "../snobol4/scrip_cc.h"
#include <stdio.h>
#include <stdlib.h>

/* Declared in raku.y %code / raku.tab.h */
extern CODE_t *raku_prog_result;
extern CODE_t *raku_parse_string(const char *src);   /* defined below — wraps yyparse */

/*============================================================
 * raku_compile — public entry point (scrip.c calls this)
 *
 * Signature unchanged from pre-FI-3.
 * Returns CODE_t* with LANG_RAKU stmts, or NULL on parse error.
 *============================================================*/
CODE_t *raku_compile(const char *src, const char *filename) {
    if (!filename) filename = "<stdin>";
    /* Case policy is a frontend concern (cf. commit 8aa5803b for DATATYPE).
     * Raku preserves identifier spelling; tell the shared runtime to stop
     * folding at name-ingest sites. No user --case-sensitive flag required. */
    sno_set_case_sensitive(1);
    raku_prog_result = NULL;   /* reset global before each parse */
    CODE_t *prog = raku_parse_string(src);
    if (!prog) {
        fprintf(stderr, "raku: parse error in %s\n", filename);
        return NULL;
    }
    return prog;
}
