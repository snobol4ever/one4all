/*
 * bb_pool_test.c — Unit test for bb_pool (M-DYN-0 gate)
 *
 * Build: gcc -o bb_pool_test src/runtime/asm/bb_pool.c
 *                             src/runtime/asm/bb_pool_test.c
 * Gate:  all cases PASS, exits 0
 */

#include "bb_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>

static int failures = 0;

#define PASS(msg)  printf("  PASS  %s\n", msg)
#define FAIL(msg)  do { printf("  FAIL  %s\n", msg); failures++; } while(0)
#define CHECK(cond, msg)  do { if (cond) PASS(msg); else FAIL(msg); } while(0)

/* ── helpers ────────────────────────────────────────────────────────────── */

/* Write a simple ret instruction into buf, seal, call it — should return */
typedef void (*void_fn)(void);

static int try_execute(bb_buf_t buf, size_t size)
{
    /* emit: mov eax,0 / ret */
    buf[0] = 0xB8;
    buf[1] = 0x00; buf[2] = 0x00; buf[3] = 0x00; buf[4] = 0x00;
    buf[5] = 0xC3;
    bb_seal(buf, size);
    void_fn fn = (void_fn)buf;
    fn();   /* should not crash */
    return 1;
}

/* ── test cases ─────────────────────────────────────────────────────────── */

static void test_init_destroy(void)
{
    printf("\n[1] init / destroy\n");
    bb_pool_init();
    CHECK(bb_pool_used() == 0, "used == 0 after init");
    bb_pool_destroy();
    bb_pool_init();   /* re-init after destroy */
    CHECK(bb_pool_used() == 0, "used == 0 after re-init");
    PASS("double init/destroy cycle");
}

static void test_alloc_basic(void)
{
    printf("\n[2] basic alloc\n");
    size_t before = bb_pool_used();

    bb_buf_t b1 = bb_alloc(64);
    CHECK(b1 != NULL, "bb_alloc(64) returns non-NULL");
    CHECK(((uintptr_t)b1 & 15) == 0, "allocation is 16-byte aligned");
    CHECK(bb_pool_used() >= before + 64, "pool_used grew by >= 64");

    bb_buf_t b2 = bb_alloc(128);
    CHECK(b2 != NULL, "bb_alloc(128) non-NULL");
    CHECK(b2 > b1, "second alloc is above first");
    CHECK(((uintptr_t)b2 & 15) == 0, "second alloc 16-byte aligned");

    /* free in LIFO order */
    bb_free(b2, 128);
    bb_free(b1, 64);
    CHECK(bb_pool_used() == before, "used restored after LIFO free");
    PASS("basic alloc/free cycle");
}

static void test_write_and_read(void)
{
    printf("\n[3] write and read back\n");
    bb_buf_t buf = bb_alloc(256);
    memset(buf, 0xAA, 256);
    int ok = 1;
    for (int i = 0; i < 256; i++) if (buf[i] != 0xAA) { ok = 0; break; }
    CHECK(ok, "wrote and read back 256 bytes of 0xAA");
    bb_free(buf, 256);
}

static void test_seal_executable(void)
{
    printf("\n[4] seal → executable\n");
    bb_buf_t buf = bb_alloc(64);
    int ok = try_execute(buf, 64);
    CHECK(ok, "sealed buffer is callable (mov eax,0 / ret)");
    bb_free(buf, 64);
}

static void test_seal_write_protection(void)
{
    printf("\n[5] sealed buffer is write-protected\n");
    /*
     * Verify via /proc/self/maps that the sealed pages have 'r-x' permissions,
     * not 'rw-'. We don't attempt an actual write (that would crash the test
     * process). The mprotect in bb_seal is the guarantee; we verify the OS
     * accepted it by checking the permission bits in /proc.
     */
    bb_buf_t buf = bb_alloc(64);
    buf[0] = 0xC3;
    bb_seal(buf, 64);

    /* Read /proc/self/maps and find the entry covering buf */
    FILE *maps = fopen("/proc/self/maps", "r");
    int found = 0, is_rx = 0;
    if (maps) {
        char line[256];
        while (fgets(line, sizeof(line), maps)) {
            uintptr_t lo, hi;
            char perms[8];
            if (sscanf(line, "%lx-%lx %7s", &lo, &hi, perms) == 3) {
                if ((uintptr_t)buf >= lo && (uintptr_t)buf < hi) {
                    found = 1;
                    /* perms[0]='r', perms[1]='-' (not 'w'), perms[2]='x' */
                    is_rx = (perms[0] == 'r' && perms[1] == '-' && perms[2] == 'x');
                    break;
                }
            }
        }
        fclose(maps);
    }
    CHECK(found,  "sealed buffer address found in /proc/self/maps");
    CHECK(is_rx,  "sealed buffer has r-x permissions (not rw-)");

    bb_free(buf, 64);
}

static void test_free_restores_writability(void)
{
    printf("\n[6] free restores writability\n");
    bb_buf_t buf = bb_alloc(64);
    buf[0] = 0xC3;
    bb_seal(buf, 64);
    bb_free(buf, 64);

    /* reallocate — should be writable again (pool top rewound) */
    bb_buf_t buf2 = bb_alloc(64);
    /* buf2 may or may not equal buf depending on page alignment of pool start,
     * but it must be in the same region and must be writable */
    CHECK(buf2 != NULL, "realloc after free returns non-NULL");
    buf2[0] = 0x90;   /* write without fault */
    CHECK(buf2[0] == 0x90, "write after free+realloc succeeded");
    bb_free(buf2, 64);
    PASS("free restores writability for reuse");
}

static void test_multiple_boxes(void)
{
    printf("\n[7] multiple boxes — allocate N, execute all, free LIFO\n");
    #define NBOXES 8
    bb_buf_t boxes[NBOXES];
    size_t   sizes[NBOXES];

    /* allocate boxes of varying sizes */
    for (int i = 0; i < NBOXES; i++) {
        sizes[i] = 64 + (size_t)(i * 16);
        boxes[i] = bb_alloc(sizes[i]);
        /* write: mov eax,i / ret */
        boxes[i][0] = 0xB8;
        boxes[i][1] = (uint8_t)i;
        boxes[i][2] = 0x00; boxes[i][3] = 0x00; boxes[i][4] = 0x00;
        boxes[i][5] = 0xC3;
        bb_seal(boxes[i], sizes[i]);
    }

    /* execute all */
    int exec_ok = 1;
    for (int i = 0; i < NBOXES; i++) {
        void_fn fn = (void_fn)boxes[i];
        fn();   /* if any crash, test fails hard */
    }
    CHECK(exec_ok, "all 8 boxes executed without fault");

    /* free LIFO */
    size_t before_free = bb_pool_used();
    (void)before_free;
    for (int i = NBOXES - 1; i >= 0; i--) {
        bb_free(boxes[i], sizes[i]);
    }
    CHECK(bb_pool_used() == 0, "pool fully reclaimed after LIFO free of 8 boxes");
    #undef NBOXES
}

static void test_large_alloc(void)
{
    printf("\n[8] large allocation (1 MB)\n");
    size_t sz = 1024 * 1024;
    bb_buf_t buf = bb_alloc(sz);
    CHECK(buf != NULL, "1 MB alloc succeeds");
    CHECK(bb_pool_used() >= sz, "pool_used reflects 1 MB");
    memset(buf, 0x55, sz);
    CHECK(buf[0] == 0x55 && buf[sz-1] == 0x55, "1 MB buffer writable end-to-end");
    bb_free(buf, sz);
    CHECK(bb_pool_used() == 0, "pool empty after 1 MB free");
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("bb_pool_test — M-DYN-0 unit test\n");
    printf("page size: %ld bytes\n", sysconf(_SC_PAGESIZE));

    bb_pool_init();

    test_init_destroy();
    test_alloc_basic();
    test_write_and_read();
    test_seal_executable();
    test_seal_write_protection();
    test_free_restores_writability();
    test_multiple_boxes();
    test_large_alloc();

    bb_pool_destroy();

    printf("\n%s  (%d failure%s)\n",
           failures == 0 ? "PASS" : "FAIL",
           failures,
           failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
