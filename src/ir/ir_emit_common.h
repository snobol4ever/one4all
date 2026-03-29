/*
 * ir_emit_common.h — Shared IR emit utilities (public API)
 *
 * Include this from any emitter that uses ir_nary_right_fold.
 * Requires sno2c.h (or ir/ir.h) to already be included for EXPR_t / EKind.
 *
 * Milestone: M-G4-SHARED-CONC-FOLD
 */

#ifndef IR_EMIT_COMMON_H
#define IR_EMIT_COMMON_H

#include "sno2c.h"   /* EXPR_t, EKind */

/* Right-fold an n-ary node (nchildren >= 3) into binary nodes of fold_kind.
 * Returns root. *out_nodes / *out_kids must be freed via ir_nary_right_fold_free. */
EXPR_t *ir_nary_right_fold(EXPR_t *node, EKind fold_kind,
                            EXPR_t ***out_nodes, EXPR_t ***out_kids);

/* Release allocations from ir_nary_right_fold. n = node->nchildren - 1. */
void ir_nary_right_fold_free(EXPR_t **nodes, EXPR_t **kids, int n);

#endif /* IR_EMIT_COMMON_H */
