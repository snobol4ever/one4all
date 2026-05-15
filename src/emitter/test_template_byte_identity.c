#include "emit.h"
#include "emit_templates.h"
#include "emit.h"
#include <stdio.h>
#include <string.h>
static const unsigned char EXPECTED_BYTES[] = {
    0x41, 0xff, 0x45, 0x14,
    0xc3,
};
static const int EXPECTED_LEN = (int)sizeof(EXPECTED_BYTES);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int main(void)
{
    unsigned char buf[16];
    memset(buf, 0xAB, sizeof(buf));   /* pre-fill with sentinel to catch
                                       * accidental short writes */
    bb_buf_t capture = buf;
    emitter_init_binary(capture, (int)sizeof(buf));
    if (0) {
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
