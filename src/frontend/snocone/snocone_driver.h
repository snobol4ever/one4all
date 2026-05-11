/*
 * sc_driver.h — Snocone frontend pipeline driver  (Sprint SC3)
 *
 * snocone_compile(src, filename) → CODE_t*
 *
 * Runs the full Snocone pipeline on a NUL-terminated source string:
 *
 *   snocone_lex()  → ScTokenArray
 *   per-stmt: snocone_parse() → ScParseResult (postfix tokens)
 *   snocone_lower() on combined postfix stream → ScLowerResult → CODE_t*
 *
 * The per-stmt split is identical to the pipeline() helper that was
 * proven in test/frontend/snocone/sc_lower_test.c (50/50 PASS).
 *
 * Returns NULL on lex/parse/lower error (errors already printed to stderr).
 * The returned CODE_t* is heap-allocated; caller does NOT free it (it
 * lives until process exit, consistent with snoc_parse() convention).
 */

#ifndef SNOCONE_DRIVER_H
#define SNOCONE_DRIVER_H

#include "../snobol4/scrip_cc.h"   /* CODE_t */
#include "../../ast/ast.h"

/*
 * snocone_compile(source, filename)
 *   source   — complete NUL-terminated Snocone source text
 *   filename — used in error messages (may be NULL → "<stdin>")
 *   SI-5: out_ast receives AST_PROGRAM; pass NULL to discard.
 *
 * Returns the compiled CODE_t*, or NULL on error.
 */
CODE_t *snocone_compile(const char *source, const char *filename, AST_t **out_ast);

#endif /* SNOCONE_DRIVER_H */
