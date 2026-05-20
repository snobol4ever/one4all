/* emit_io.h — EC-UNI-11: Layer-3 string-builder primitives.
 *
 * The end-state design (GOAL-HEADQUARTERS.md → EC-UNI End-state design) makes these the single
 * funnel for all emitter output.  Templates at Layer 1 and helpers at Layer 2 call these instead
 * of fprintf(g_emit.out, ...) / fputc / fwrite directly.  Two private buffers:
 *
 *   g_text_buf   — text-mode output (GAS .s, JVM .j, NET .il, JS .js, WASM .wat).
 *                  Appended to by emit_text / emit_textf.
 *   g_bin_buf    — raw byte output (binary-wired x86, future binary modes).
 *                  Appended to by emit_byte / emit_bytes.
 *
 * Both buffers are module-static inside emit_io.c.  Single-threaded by construction (mirrors the
 * re-entrancy contract of g_emit).  At the end of emit_program, emit_io_flush() writes whichever
 * buffer holds data (or both, in order text-then-binary) to the FILE * sink and resets both
 * buffers for the next pass.
 *
 * EC-UNI-11 lands these primitives + a self-test that round-trips a synthetic byte stream.  No
 * template body changes yet — the templates still call fprintf(g_emit.out, ...) etc.  EC-UNI-12
 * does the mechanical sweep that replaces fprintf with emit_textf and fputc/fwrite with
 * emit_byte/emit_bytes, at which point this header becomes load-bearing for byte identity.
 *
 * Re-entrancy: NOT re-entrant.  If emit_program is ever called recursively (today it is via
 * sm_preamble's transient FILE* sinks — handled by save/restore of g_emit), the caller must
 * also save/restore the buffer state.  See emit_io_save / emit_io_restore. */
#ifndef EMIT_IO_H
#define EMIT_IO_H
#include <stddef.h>
#include <stdio.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Layer-3 primitives.  Append-only.  No backpressure; buffers grow geometrically. */
void emit_text  (const char * s);                  /* append C string (sans NUL) to text buffer */
void emit_textf (const char * fmt, ...)            /* printf-formatted append to text buffer */
                 __attribute__((format(printf, 1, 2)));
void emit_byte  (unsigned char b);                 /* append one byte to binary buffer */
void emit_bytes (const unsigned char * p, int n);  /* append run of bytes to binary buffer */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Flush hook — called from emit_program at completion.  Writes whichever buffer is non-empty to
 * `out` (text first if both, but binary-and-text in the same pass is not a supported pattern at
 * EC-UNI-11; EC-UNI-12 will narrow this further as silos delete).  Resets both buffers to empty.
 * Returns the total byte count written. */
size_t emit_io_flush(FILE * out);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Inspection helpers — used by the self-test and by callers that need to compute md5 / size before
 * deciding whether to commit the flush.  Pointers are valid until the next emit_text* / emit_byte*
 * call or until emit_io_flush()/emit_io_reset() runs. */
const char *          emit_io_text_ptr (void);  /* current text buffer base; NUL-terminated */
size_t                emit_io_text_len (void);  /* current text buffer length (excl. NUL) */
const unsigned char * emit_io_bin_ptr  (void);  /* current binary buffer base */
size_t                emit_io_bin_len  (void);  /* current binary buffer length */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Reset without writing (escape hatch for tests / aborted passes). */
void emit_io_reset(void);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Save / restore primitives for the (currently unused but contract-documented) nested-pass case.
 * Mirrors the save/restore of g_emit in emit_program. */
typedef struct {
    char *          text;
    size_t          text_len;
    size_t          text_cap;
    unsigned char * bin;
    size_t          bin_len;
    size_t          bin_cap;
} emit_io_saved_t;
emit_io_saved_t emit_io_save    (void);
void            emit_io_restore (emit_io_saved_t saved);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#endif
