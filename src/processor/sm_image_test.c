/*
 * scrip_image_test.c — M-SCRIP-U1 unit test
 *
 * Gate: alloc / write / seal / execute / destroy cycle for each segment.
 * Compile: gcc -O0 -g -I. -I.. scrip_image.c scrip_image_test.c -o scrip_image_test
 * Run:     ./scrip_image_test
 */

#include "sm_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ── helpers ─────────────────────────────────────────────────────────── */

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
    else          { fprintf(stdout, "PASS: %s\n", msg); } \
} while(0)

/* ── tests ───────────────────────────────────────────────────────────── */

static void test_init_destroy(void)
{
    int r = sm_image_init();
    CHECK(r == 0, "sm_image_init returns 0");

    for (int i = 0; i < SEG_COUNT; i++) {
        CHECK(scrip_segs[i].base  != NULL, "segment base non-null");
        CHECK(scrip_segs[i].top   == scrip_segs[i].base, "segment starts empty");
        CHECK(scrip_segs[i].limit >  scrip_segs[i].base, "segment has space");
        CHECK(scrip_segs[i].sealed == 0, "segment starts unsealed");
    }

    sm_image_destroy();
    CHECK(scrip_segs[0].base == NULL, "destroy clears base");
}

static void test_alloc_and_write(void)
{
    sm_image_init();

    /* seg_byte into SEG_DATA */
    size_t before = seg_used(SEG_DATA);
    seg_byte(SEG_DATA, 0xAB);
    seg_byte(SEG_DATA, 0xCD);
    size_t after = seg_used(SEG_DATA);
    CHECK(after == before + 2, "seg_byte advances top by 2");

    /* seg_u32 into SEG_DATA */
    size_t off32 = seg_offset(SEG_DATA);
    seg_u32(SEG_DATA, 0xDEADBEEF);
    uint8_t *b = scrip_segs[SEG_DATA].base + off32;
    CHECK(b[0] == 0xEF && b[1] == 0xBE && b[2] == 0xAD && b[3] == 0xDE,
          "seg_u32 little-endian");

    /* seg_u64 into SEG_DATA */
    size_t off64 = seg_offset(SEG_DATA);
    seg_u64(SEG_DATA, 0x0123456789ABCDEFULL);
    uint8_t *b64 = scrip_segs[SEG_DATA].base + off64;
    CHECK(b64[0] == 0xEF && b64[7] == 0x01, "seg_u64 little-endian");

    /* seg_alloc alignment */
    seg_byte(SEG_DATA, 0x01);  /* misalign top intentionally */
    uint8_t *p = seg_alloc(SEG_DATA, 8);
    CHECK(((uintptr_t)p & 15) == 0, "seg_alloc returns 16-byte aligned pointer");

    /* seg_patch_u32 */
    seg_u32(SEG_DATA, 0x00000000);
    size_t patch_off = seg_offset(SEG_DATA) - 4;
    seg_patch_u32(SEG_DATA, patch_off, 0x12345678);
    uint8_t *pb = scrip_segs[SEG_DATA].base + patch_off;
    CHECK(pb[0] == 0x78 && pb[3] == 0x12, "seg_patch_u32 writes correctly");

    sm_image_destroy();
}

static void test_seal_and_execute(void)
{
    sm_image_init();

    /*
     * Emit a trivial x86-64 function into SEG_CODE:
     *   mov eax, 42      ; B8 2A 00 00 00
     *   ret              ; C3
     * Then seal, cast to fn ptr, call, verify return value.
     */
    size_t fn_off = seg_offset(SEG_CODE);
    seg_byte(SEG_CODE, 0xB8); seg_u32(SEG_CODE, 42); /* mov eax, 42 */
    seg_byte(SEG_CODE, 0xC3);                          /* ret */

    seg_seal(SEG_CODE);
    CHECK(scrip_segs[SEG_CODE].sealed == 1, "SEG_CODE sealed");

    typedef int (*int_fn_t)(void);
    int_fn_t fn = (int_fn_t)(scrip_segs[SEG_CODE].base + fn_off);
    int result = fn();
    CHECK(result == 42, "sealed SEG_CODE fn returns 42");

    sm_image_destroy();
}

static void test_stubs(void)
{
    sm_image_init();

    /* Register two fake fn ptrs in stub table */
    void *fake_a = (void*)0xAAAAAAAAAAAAAAAAULL;
    void *fake_b = (void*)0xBBBBBBBBBBBBBBBBULL;
    size_t off_a = seg_stubs_add_ptr(fake_a);
    size_t off_b = seg_stubs_add_ptr(fake_b);

    CHECK(*seg_stubs_slot(off_a) == fake_a, "stub slot A reads back correctly");
    CHECK(*seg_stubs_slot(off_b) == fake_b, "stub slot B reads back correctly");
    CHECK(off_b == off_a + 8, "stub slots are 8 bytes apart");

    sm_image_destroy();
}

static void test_seg_offset(void)
{
    sm_image_init();

    size_t o0 = seg_offset(SEG_DATA);
    seg_byte(SEG_DATA, 0xFF);
    size_t o1 = seg_offset(SEG_DATA);
    CHECK(o1 == o0 + 1, "seg_offset tracks bytes written");

    sm_image_destroy();
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== scrip_image unit tests (M-SCRIP-U1) ===\n");
    test_init_destroy();
    test_alloc_and_write();
    test_seal_and_execute();
    test_stubs();
    test_seg_offset();

    if (failures == 0)
        printf("\nAll tests PASSED.\n");
    else
        printf("\n%d test(s) FAILED.\n", failures);
    return failures ? 1 : 0;
}
