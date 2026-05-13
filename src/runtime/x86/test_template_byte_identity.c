/*
 * test_template_byte_identity.c — EM-MODE4-IS-MODE3-DUMP-c unit test.
 *
 * Calls the SM_HALT template (emit_sm_halt in templates/sm_halt.c)
 * through a binary emitter targeting a local buffer.  Compares the
 * resulting bytes to the expected sequence (41 ff 45 14 c3 — the
 * legacy emit_halt_blob's output).
 *
 * Stronger than the behavioral gate (test_gate_em_template_byte_identity.sh)
 * because it directly compares the emitted byte sequence — a future
 * regression that produces equivalent-but-different bytes (e.g.
 * different encoding of the same operation) would be caught here.
 *
 * Self-contained: links against the same .o files the scrip binary
 * uses.  Built by scripts/build_test_template_byte_identity.sh.
 *
 * Authors: Lon Jones Cherryholmes · Claude Opus 4.7
 * Sprint:  EM-MODE4-IS-MODE3-DUMP-c / GOAL-MODE4-EMIT
 */

#include "emit.h"
#include "emit_templates.h"
#include "emit.h"
#include <stdio.h>
#include <string.h>

/* Reference: the bytes the legacy emit_halt_blob in sm_codegen.c
 * produced before EM-MODE4-IS-MODE3-DUMP-c.  Match these byte-for-byte
 * or fail the test. */
static const unsigned char EXPECTED_BYTES[] = {
    0x41, 0xff, 0x45, 0x14,   /* inc dword [r13 + 20] */
    0xc3,                      /* ret                  */
};
static const int EXPECTED_LEN = (int)sizeof(EXPECTED_BYTES);

int main(void)
{
    unsigned char buf[16];
    memset(buf, 0xAB, sizeof(buf));   /* pre-fill with sentinel to catch
                                       * accidental short writes */

    bb_buf_t capture = buf;
    emitter_init_binary(capture, (int)sizeof(buf));
    if (0) {  /* emitter_init_binary cannot fail */
        fprintf(stderr, "FAIL: emitter_binary_new returned NULL\n");
        return 1;
    }

    emit_sm_halt();

    int n = emitter_end();
    

    if (n != EXPECTED_LEN) {
        fprintf(stderr, "FAIL: emit_sm_halt produced %d bytes, expected %d\n",
                n, EXPECTED_LEN);
        return 1;
    }
    if (memcmp(buf, EXPECTED_BYTES, EXPECTED_LEN) != 0) {
        fprintf(stderr, "FAIL: byte mismatch.\n  got:     ");
        for (int i = 0; i < n; i++) fprintf(stderr, "%02x ", buf[i]);
        fprintf(stderr, "\n  expected:");
        for (int i = 0; i < EXPECTED_LEN; i++) fprintf(stderr, "%02x ", EXPECTED_BYTES[i]);
        fprintf(stderr, "\n");
        return 1;
    }

    /* Sentinel preservation: bytes 5..15 must still be 0xAB. */
    for (int i = EXPECTED_LEN; i < (int)sizeof(buf); i++) {
        if (buf[i] != 0xAB) {
            fprintf(stderr,
                    "FAIL: sentinel at buf[%d] = 0x%02x (expected 0xAB) — "
                    "template wrote past its 5-byte boundary\n",
                    i, buf[i]);
            return 1;
        }
    }

    printf("PASS: emit_sm_halt produced 5 bytes matching legacy emit_halt_blob\n");
    return 0;
}
