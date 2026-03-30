#ifndef EMIT_WASM_ICON_H
#define EMIT_WASM_ICON_H
/*
 * emit_wasm_icon.h — public interface for Icon × WASM emitter
 *
 * Called from emit_wasm.c when it encounters an ICN_* node.
 * All other frontends (SNOBOL4, Prolog) are unaffected.
 */
#include "icon_ast.h"
#include <stdio.h>

/* Set the output file handle (called before any emit function). */
void emit_wasm_icon_set_out(FILE *f);

/* Emit WAT (global …) declarations for all per-node value globals.
 * Call once, before the (func …) section. */
void emit_wasm_icon_globals(FILE *out);

/* Dispatch: emit WAT for one ICN_* node and its sub-tree.
 * Returns 1 if handled, 0 if unknown kind. */
int emit_wasm_icon_node(const IcnNode *n, FILE *out);

/* True if kind is an ICN_* node handled by this emitter. */
int is_icon_node(int kind);

#endif /* EMIT_WASM_ICON_H */
