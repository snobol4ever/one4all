/*
 * emit_wasm.h — Shared WASM emitter API for sibling frontend emitters
 *
 * emit_wasm.c owns the string literal table and output stream.
 * Prolog (emit_wasm_prolog.c) and Icon (emit_wasm_icon.c) call these
 * helpers to intern atom/string literals and share the same data segment.
 *
 * Usage in a sibling emitter:
 *   #include "emit_wasm.h"
 *   ...
 *   emit_wasm_set_out(my_out_file);         // share output stream
 *   int idx = emit_wasm_strlit_intern("hello");
 *   int off = emit_wasm_strlit_abs(idx);
 *   int len = emit_wasm_strlit_len(idx);
 *   emit_wasm_data_segment();               // emit (data ...) block
 *
 * Created: PW-2 M-PW-HELLO (2026-03-30)
 */

#ifndef EMIT_WASM_H
#define EMIT_WASM_H

#include <stdio.h>

/* Share the output stream — call before any W() output from a sibling emitter */
void emit_wasm_set_out(FILE *f);

/* Intern a string literal into the shared string table.
 * Returns the index; safe to call multiple times with same string (deduped). */
int  emit_wasm_strlit_intern(const char *s);

/* Absolute memory offset of string literal at index idx (STR_DATA_BASE + offset) */
int  emit_wasm_strlit_abs(int idx);

/* Byte length of string literal at index idx */
int  emit_wasm_strlit_len(int idx);

/* Emit the (data ...) segment for all interned literals.
 * Called once from the sibling emitter's module preamble. */
void emit_wasm_data_segment(void);

/* Reset the string table (called at start of each emit pass) */
void emit_wasm_strlit_reset(void);

/* Number of interned string literals (used to emit per-literal globals) */
int  emit_wasm_strlit_count(void);

#endif /* EMIT_WASM_H */
