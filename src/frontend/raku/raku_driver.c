/*
 * raku_driver.c — Tiny-Raku compiler pipeline driver
 *
 * raku_compile(src, filename, out_ast) — sets *out_ast to TT_PROGRAM.
 * Grammar actions build tree_t/Stmt_t inline; code_to_ast() wraps into
 * TT_PROGRAM.  Direct emission deferred to GOAL-SNOCONE-SM-LOWER.
 */
#include "raku_driver.h"
#include "../snobol4/scrip_cc.h"
#include <stdio.h>
#include <stdlib.h>

/* Declared in raku.y */
extern CODE_t *raku_prog_result;
extern CODE_t *raku_parse_string(const char *src);

void raku_compile(const char *src, const char *filename, tree_t **out_ast) {
    if (!filename) filename = "<stdin>";
    if (out_ast) *out_ast = NULL;
    sno_set_case_sensitive(1);
    raku_prog_result = NULL;
    CODE_t *prog = raku_parse_string(src);
    if (!prog) {
        fprintf(stderr, "raku: parse error in %s\n", filename);
        return;
    }
    if (out_ast) *out_ast = code_to_ast(prog);
}
