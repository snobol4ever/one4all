/*============================================================================================================================
 * scan_builtins.h — RS-23-extra-prep: SCAN-context builtin dispatch.
 *
 * Single entry-point `scan_try_call_builtin` lifted out of interp_eval's
 * icn-frame AST_FNC switch.  See scan_builtins.c for the rationale.
 *==========================================================================================================================*/
#ifndef SCAN_BUILTINS_H
#define SCAN_BUILTINS_H

#include "../ast/ast.h"    /* AST_t */
#include "snobol4.h"        /* DESCR_t */

/* Returns 1 if `call` named a SCAN builtin and was handled (*out set).
 * Returns 0 if the name does not match — caller continues its own dispatch.
 *
 * `args` is the pre-evaluated argument vector (same convention as
 * icn_call_builtin); `args[0]` is the first user argument
 * (corresponding to call->c[1]).  nargs is the user-arg count
 * (call->n - 1).
 *
 * The dispatcher reads `scan_pos` / `scan_subj` (Icon scan keywords)
 * and may write `scan_pos` for builtins that advance position
 * (any/many/move/tab/match).  Inside-scan-only builtins fail when
 * scan_pos == 0 (no scan in flight) unless a string-arg overload is
 * provided. */
int scan_try_call_builtin(AST_t *call, DESCR_t *args, int nargs, DESCR_t *out);

#endif /* SCAN_BUILTINS_H */
