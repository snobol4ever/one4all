/* test_emit_io.c — EC-UNI-11 self-test for the Layer-3 string-builder primitives.
 *
 * Round-trips a synthetic mix of text and bytes:
 *   1. Drives the text path: emit_text + emit_textf with several format flavors and a length that
 *      forces the geometric growth path (well past the 4 KiB initial capacity).
 *   2. Drives the binary path: emit_byte + emit_bytes with a known stream.
 *   3. Inspects buffer state via emit_io_text_ptr/len + emit_io_bin_ptr/len.
 *   4. Flushes to a memstream FILE * and compares the captured bytes to the expected concatenation
 *      (text first, then binary — per emit_io_flush contract).
 *   5. Re-runs after emit_io_reset to confirm buffers are clean.
 *   6. Exercises emit_io_save / emit_io_restore once.
 *
 * Build/run:
 *   gcc -O0 -g -Wall -I src -I src/include -I src/emitter \
 *       src/emitter/emit_io.c src/emitter/test_emit_io.c -o /tmp/test_emit_io && \
 *   /tmp/test_emit_io
 *
 * Or, via the Makefile target `make test_emit_io`. */
#include "emit_io.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define ASSERT_EQ_SIZE(actual, expected, label) do {                                              \
    if ((size_t)(actual) != (size_t)(expected)) {                                                 \
        fprintf(stderr, "FAIL %s:%d %s: actual=%zu expected=%zu\n",                               \
                __FILE__, __LINE__, (label), (size_t)(actual), (size_t)(expected));               \
        return 1;                                                                                 \
    }                                                                                             \
} while (0)
#define ASSERT_BYTES_EQ(actual, expected, len, label) do {                                        \
    if (memcmp((actual), (expected), (size_t)(len)) != 0) {                                       \
        fprintf(stderr, "FAIL %s:%d %s: bytes differ over %zu bytes\n",                           \
                __FILE__, __LINE__, (label), (size_t)(len));                                      \
        for (size_t _i = 0; _i < (size_t)(len); _i++) {                                           \
            unsigned a = ((const unsigned char *)(actual))[_i];                                   \
            unsigned e = ((const unsigned char *)(expected))[_i];                                 \
            if (a != e) { fprintf(stderr, "  off %zu: %02x vs %02x\n", _i, a, e); if (_i > 16) break; } \
        }                                                                                         \
        return 1;                                                                                 \
    }                                                                                             \
} while (0)
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Test 1: text path basic shape. */
static int test_text_basic(void) {
    emit_io_reset();
    emit_text("hello ");
    emit_text("world");
    emit_textf("\n%d %s\n", 42, "answer");
    const char * exp = "hello world\n42 answer\n";
    ASSERT_EQ_SIZE(emit_io_text_len(), strlen(exp), "text_basic length");
    ASSERT_BYTES_EQ(emit_io_text_ptr(), exp, strlen(exp), "text_basic bytes");
    /* Binary buffer must still be empty. */
    ASSERT_EQ_SIZE(emit_io_bin_len(), 0, "text_basic binary side empty");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Test 2: text growth past the initial 4 KiB capacity. */
static int test_text_growth(void) {
    emit_io_reset();
    /* Emit 64 KiB of repeated 256-byte chunks to force several geometric doublings. */
    char chunk[256];
    for (int i = 0; i < 256; i++) chunk[i] = (char)('A' + (i % 26));
    /* Use emit_textf with a precision spec to write exactly 256 bytes without a NUL inside it. */
    for (int rep = 0; rep < 256; rep++) emit_textf("%.256s", chunk);
    ASSERT_EQ_SIZE(emit_io_text_len(), (size_t)(256 * 256), "text_growth length");
    /* Spot-check the start and the end. */
    ASSERT_BYTES_EQ(emit_io_text_ptr(),                    chunk, 256, "text_growth start");
    ASSERT_BYTES_EQ(emit_io_text_ptr() + 256 * 255,        chunk, 256, "text_growth end");
    /* Trailing NUL invariant. */
    if (emit_io_text_ptr()[256 * 256] != '\0') {
        fprintf(stderr, "FAIL text_growth: missing trailing NUL\n");
        return 1;
    }
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Test 3: binary path basic shape. */
static int test_bin_basic(void) {
    emit_io_reset();
    emit_byte(0x90);  /* NOP */
    emit_byte(0xC3);  /* RET */
    unsigned char run[] = { 0x48, 0x89, 0xE5, 0x5D };  /* mov rbp,rsp; pop rbp */
    emit_bytes(run, (int)sizeof(run));
    unsigned char exp[] = { 0x90, 0xC3, 0x48, 0x89, 0xE5, 0x5D };
    ASSERT_EQ_SIZE(emit_io_bin_len(), sizeof(exp), "bin_basic length");
    ASSERT_BYTES_EQ(emit_io_bin_ptr(), exp, sizeof(exp), "bin_basic bytes");
    /* Text buffer must still be empty. */
    ASSERT_EQ_SIZE(emit_io_text_len(), 0, "bin_basic text side empty");
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Test 4: flush to a memstream and verify ordering (text then binary) + reset. */
static int test_flush(void) {
    emit_io_reset();
    emit_text("PROLOGUE\n");
    emit_byte(0xDE);
    emit_byte(0xAD);
    emit_textf("after %d byte(s)\n", 2);  /* Text appended AFTER binary; flush still writes text first. */
    emit_bytes((const unsigned char *)"\xBE\xEF", 2);
    /* Capture flush output. */
    char *   captured = NULL;
    size_t   cap_len  = 0;
    FILE *   ms       = open_memstream(&captured, &cap_len);
    if (!ms) { fprintf(stderr, "FAIL test_flush: open_memstream failed\n"); return 1; }
    size_t flushed = emit_io_flush(ms);
    fflush(ms);
    fclose(ms);
    const char * exp_text = "PROLOGUE\nafter 2 byte(s)\n";
    size_t exp_text_len = strlen(exp_text);
    const unsigned char exp_bin[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    size_t exp_total = exp_text_len + sizeof(exp_bin);
    ASSERT_EQ_SIZE(flushed,  exp_total, "flush returned");
    ASSERT_EQ_SIZE(cap_len,  exp_total, "memstream length");
    ASSERT_BYTES_EQ(captured,                exp_text, exp_text_len, "flush text section");
    ASSERT_BYTES_EQ(captured + exp_text_len, exp_bin,  sizeof(exp_bin), "flush binary section");
    /* Buffers must be reset to empty. */
    ASSERT_EQ_SIZE(emit_io_text_len(), 0, "flush resets text");
    ASSERT_EQ_SIZE(emit_io_bin_len(),  0, "flush resets bin");
    free(captured);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Test 5: NULL/empty arg safety. */
static int test_empty_safe(void) {
    emit_io_reset();
    emit_text(NULL);
    emit_text("");
    /* GCC warns on the empty format string — intentional here; suppress just this call. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-zero-length"
    emit_textf("");
#pragma GCC diagnostic pop
    emit_bytes(NULL, 0);
    emit_bytes((const unsigned char *)"x", 0);
    ASSERT_EQ_SIZE(emit_io_text_len(), 0, "empty_safe text");
    ASSERT_EQ_SIZE(emit_io_bin_len(),  0, "empty_safe bin");
    /* Flushing nothing must produce zero output. */
    char *   captured = NULL;
    size_t   cap_len  = 0;
    FILE *   ms       = open_memstream(&captured, &cap_len);
    size_t flushed = emit_io_flush(ms);
    fflush(ms);
    fclose(ms);
    ASSERT_EQ_SIZE(flushed, 0, "empty flush");
    ASSERT_EQ_SIZE(cap_len, 0, "empty memstream");
    free(captured);
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Test 6: save / restore round-trip. */
static int test_save_restore(void) {
    emit_io_reset();
    emit_text("outer");
    emit_byte(0x11);
    emit_io_saved_t s = emit_io_save();
    /* Buffers now appear empty to the nested pass. */
    ASSERT_EQ_SIZE(emit_io_text_len(), 0, "save zeroes text");
    ASSERT_EQ_SIZE(emit_io_bin_len(),  0, "save zeroes bin");
    emit_text("inner");
    emit_byte(0x22);
    ASSERT_EQ_SIZE(emit_io_text_len(), strlen("inner"), "inner text len");
    ASSERT_EQ_SIZE(emit_io_bin_len(),  1,               "inner bin len");
    emit_io_restore(s);
    /* Outer state must be back. */
    ASSERT_EQ_SIZE(emit_io_text_len(), strlen("outer"), "restored text len");
    ASSERT_EQ_SIZE(emit_io_bin_len(),  1,               "restored bin len");
    ASSERT_BYTES_EQ(emit_io_text_ptr(), "outer", strlen("outer"), "restored text bytes");
    unsigned char exp_bin = 0x11;
    ASSERT_BYTES_EQ(emit_io_bin_ptr(),  &exp_bin, 1, "restored bin byte");
    emit_io_reset();
    return 0;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int main(void) {
    int rc = 0;
    rc |= test_text_basic();
    rc |= test_text_growth();
    rc |= test_bin_basic();
    rc |= test_flush();
    rc |= test_empty_safe();
    rc |= test_save_restore();
    if (rc) { fprintf(stderr, "test_emit_io: FAILED\n"); return 1; }
    printf("test_emit_io: PASS (6/6)\n");
    return 0;
}
