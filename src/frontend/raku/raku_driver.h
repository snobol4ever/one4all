/*
 * raku_driver.h — Tiny-Raku compiler entry point
 *
 * FI-3: direct IR, no intermediate RakuNode AST.
 *
 * AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6
 */
#ifndef RAKU_DRIVER_H
#define RAKU_DRIVER_H

#include "../snobol4/scrip_cc.h"   /* CODE_t */
#include "../../ast/ast.h"

/* Full pipeline: parse → CODE_t* directly (no lower step).
 * Sets st->lang = LANG_RAKU on each STMT_t.
 * SI-5: out_ast receives AST_PROGRAM; pass NULL to discard.
 * Returns NULL on parse failure. */
CODE_t *raku_compile(const char *src, const char *filename, AST_t **out_ast);

#endif /* RAKU_DRIVER_H */
