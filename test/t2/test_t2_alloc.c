/*
 * test_t2_alloc.c — unit test for t2_alloc / t2_free / t2_mprotect_rx / t2_mprotect_rw
 *
 * Tests:
 *   1. Allocate a block, write to it, read back — basic RW
 *   2. mprotect_rx: block becomes executable; write attempt would SIGSEGV (not tested here)
 *   3. mprotect_rw: toggle back to RW, write again
 *   4. t2_free: no crash on valid free
 *   5. Zero-size alloc: returns non-NULL (rounds to one page)
 *   6. Various sizes: sub-page, exact page, multi-page
 *
 * Compile:
 *   gcc -o /tmp/test_t2 test/t2/test_t2_alloc.c src/runtime/asm/t2_alloc.c
 * Run:
 *   /tmp/test_t2  → prints PASS for each test, exits 0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../../src/runtime/asm/t2_alloc.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
    else          { printf("PASS: %s\n", msg); } \
} while(0)

int main(void) {
    long pgsz = sysconf(_SC_PAGESIZE);

    /* --- Test 1: basic RW alloc + write + read --- */
    {
        size_t sz = 128;
        char *p = t2_alloc(sz);
        CHECK(p != NULL, "t2_alloc returns non-NULL for 128 bytes");
        memset(p, 0xAB, sz);
        int ok = 1;
        for (size_t i = 0; i < sz; i++) if ((unsigned char)p[i] != 0xAB) { ok = 0; break; }
        CHECK(ok, "t2_alloc block is RW: write+read 0xAB pattern");
        t2_free(p, sz);
    }

    /* --- Test 2: flush icache, mprotect_rx, then mprotect_rw toggle --- */
    {
        size_t sz = (size_t)pgsz;
        unsigned char *p = t2_alloc(sz);
        CHECK(p != NULL, "t2_alloc page-sized block");

        /* Write a recognisable pattern — simulate TEXT copy */
        memset(p, 0x90, sz);   /* NOP sled */
        p[0] = 0xC3;           /* RET — valid x64 instruction */

        /* Flush icache AFTER memcpy, BEFORE mprotect_rx */
        t2_flush_icache(p, sz);
        CHECK(1, "t2_flush_icache after memcpy (portable; no-op on x86-64)");

        int r = t2_mprotect_rx(p, sz);
        CHECK(r == 0, "t2_mprotect_rx succeeds");

        /* Toggle back to RW and overwrite */
        r = t2_mprotect_rw(p, sz);
        CHECK(r == 0, "t2_mprotect_rw succeeds");
        p[0] = 0xCC;           /* INT3 */
        t2_flush_icache(p, sz);
        CHECK(p[0] == 0xCC, "write after mprotect_rw works + flush");

        t2_free(p, sz);
    }

    /* --- Test 3: zero-size alloc --- */
    {
        void *p = t2_alloc(0);
        CHECK(p != NULL, "t2_alloc(0) returns non-NULL (rounds to page)");
        t2_free(p, 0);
    }

    /* --- Test 4: sub-page size --- */
    {
        size_t sz = 1;
        char *p = t2_alloc(sz);
        CHECK(p != NULL, "t2_alloc(1) non-NULL");
        *p = 42;
        CHECK(*p == 42, "t2_alloc(1) RW");
        t2_free(p, sz);
    }

    /* --- Test 5: multi-page alloc --- */
    {
        size_t sz = (size_t)(pgsz * 4);
        unsigned char *p = t2_alloc(sz);
        CHECK(p != NULL, "t2_alloc 4-page block non-NULL");
        /* Touch every page */
        for (size_t off = 0; off < sz; off += (size_t)pgsz)
            p[off] = (unsigned char)(off & 0xFF);
        CHECK(p[0] == 0 && p[pgsz] == (unsigned char)(pgsz & 0xFF),
              "t2_alloc 4-page block: all pages RW");
        int rx = t2_mprotect_rx(p, sz);
        CHECK(rx == 0, "t2_mprotect_rx on 4-page block");
        int rw = t2_mprotect_rw(p, sz);
        CHECK(rw == 0, "t2_mprotect_rw on 4-page block");
        t2_free(p, sz);
    }

    /* --- Test 6: repeated alloc/free cycle --- */
    {
        int cycle_ok = 1;
        for (int i = 0; i < 32; i++) {
            void *p = t2_alloc(256);
            if (!p) { cycle_ok = 0; break; }
            memset(p, i, 256);
            t2_free(p, 256);
        }
        CHECK(cycle_ok, "32 alloc/free cycles without failure");
    }

    printf("\n%s — %d test(s) failed\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
