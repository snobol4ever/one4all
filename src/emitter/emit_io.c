/* emit_io.c — EC-UNI-11: Layer-3 string-builder primitives.  Header documents shape. */
#include "emit_io.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Two private buffers.  Module-static.  Single-threaded by construction. */
static char *          g_text_buf = NULL;
static size_t          g_text_len = 0;
static size_t          g_text_cap = 0;
static unsigned char * g_bin_buf  = NULL;
static size_t          g_bin_len  = 0;
static size_t          g_bin_cap  = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Geometric growth.  Initial cap 4 KiB; double on need. */
#define EMIT_IO_INIT_CAP 4096
static void text_grow_to(size_t need) {
    if (need <= g_text_cap) return;
    size_t cap = g_text_cap ? g_text_cap : EMIT_IO_INIT_CAP;
    while (cap < need) cap *= 2;
    char * b = (char *)realloc(g_text_buf, cap);
    if (!b) { perror("emit_io: text realloc"); abort(); }
    g_text_buf = b;
    g_text_cap = cap;
}
static void bin_grow_to(size_t need) {
    if (need <= g_bin_cap) return;
    size_t cap = g_bin_cap ? g_bin_cap : EMIT_IO_INIT_CAP;
    while (cap < need) cap *= 2;
    unsigned char * b = (unsigned char *)realloc(g_bin_buf, cap);
    if (!b) { perror("emit_io: bin realloc"); abort(); }
    g_bin_buf = b;
    g_bin_cap = cap;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Primitives. */
void emit_text(const char * s) {
    if (!s) return;
    size_t n = strlen(s);
    if (n == 0) return;
    text_grow_to(g_text_len + n + 1);  /* +1 for trailing NUL we maintain for emit_io_text_ptr() */
    memcpy(g_text_buf + g_text_len, s, n);
    g_text_len += n;
    g_text_buf[g_text_len] = '\0';
}
void emit_textf(const char * fmt, ...) {
    if (!fmt) return;
    va_list ap;
    /* First pass: ask vsnprintf how many bytes it would write into 0 bytes. */
    va_start(ap, fmt);
    int need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need <= 0) return;
    text_grow_to(g_text_len + (size_t)need + 1);
    /* Second pass: write into the grown buffer. */
    va_start(ap, fmt);
    int wrote = vsnprintf(g_text_buf + g_text_len, g_text_cap - g_text_len, fmt, ap);
    va_end(ap);
    if (wrote > 0) g_text_len += (size_t)wrote;
    /* vsnprintf NUL-terminates; we maintain the NUL invariant.  Defensive: re-terminate. */
    g_text_buf[g_text_len] = '\0';
}
void emit_byte(unsigned char b) {
    bin_grow_to(g_bin_len + 1);
    g_bin_buf[g_bin_len++] = b;
}
void emit_bytes(const unsigned char * p, int n) {
    if (!p || n <= 0) return;
    bin_grow_to(g_bin_len + (size_t)n);
    memcpy(g_bin_buf + g_bin_len, p, (size_t)n);
    g_bin_len += (size_t)n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Flush.  Writes text-then-binary (order matters only if a single pass uses both; today no pass
 * does — EC-UNI-12 will assert that more strictly).  Resets both buffers. */
size_t emit_io_flush(FILE * out) {
    if (!out) { emit_io_reset(); return 0; }
    size_t total = 0;
    if (g_text_len > 0) {
        size_t w = fwrite(g_text_buf, 1, g_text_len, out);
        total += w;
    }
    if (g_bin_len > 0) {
        size_t w = fwrite(g_bin_buf, 1, g_bin_len, out);
        total += w;
    }
    emit_io_reset();
    return total;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Inspection. */
const char *          emit_io_text_ptr(void) { return g_text_buf ? g_text_buf : ""; }
size_t                emit_io_text_len(void) { return g_text_len; }
const unsigned char * emit_io_bin_ptr (void) { return g_bin_buf; }
size_t                emit_io_bin_len (void) { return g_bin_len; }
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Reset (no write). */
void emit_io_reset(void) {
    g_text_len = 0;
    g_bin_len  = 0;
    if (g_text_buf && g_text_cap > 0) g_text_buf[0] = '\0';
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Save / restore.  Hands ownership of the current buffers to the caller; installs fresh empty
 * buffers for the nested pass.  Restore frees whatever the nested pass left and re-installs the
 * caller's saved state.  Currently unused — contract-documented for the eventual nested-pass case. */
emit_io_saved_t emit_io_save(void) {
    emit_io_saved_t s = { g_text_buf, g_text_len, g_text_cap, g_bin_buf, g_bin_len, g_bin_cap };
    g_text_buf = NULL; g_text_len = 0; g_text_cap = 0;
    g_bin_buf  = NULL; g_bin_len  = 0; g_bin_cap  = 0;
    return s;
}
void emit_io_restore(emit_io_saved_t saved) {
    free(g_text_buf);
    free(g_bin_buf);
    g_text_buf = saved.text;  g_text_len = saved.text_len;  g_text_cap = saved.text_cap;
    g_bin_buf  = saved.bin;   g_bin_len  = saved.bin_len;   g_bin_cap  = saved.bin_cap;
}
