/*============================================================================================================================
 * raku_builtins.h — RS-23a-raku: Raku-builtin dispatch.
 *
 * Single entry-point `raku_try_call_builtin` lifted out of interp_eval's
 * icn-frame TT_FNC switch.  See raku_builtins.c for the rationale.
 *==========================================================================================================================*/
#ifndef RAKU_BUILTINS_H
#define RAKU_BUILTINS_H

#include "../ast/ast.h"    /* tree_t */
#include "snobol4.h"        /* DESCR_t — same header icn_value.h uses */

/* Returns 1 if `call` named a Raku builtin and was handled (*out set).
 * Returns 0 if the name does not match — caller continues its own dispatch.
 * Internal recursions use bb_eval_value, not interp_eval (RS-20 isolation). */
int raku_try_call_builtin(tree_t *call, DESCR_t *out);

#endif /* RAKU_BUILTINS_H */
