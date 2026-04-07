/*
 * bb_pool.c — Dynamic Byrd Box Buffer Pool (M-DYN-0)
 *
 * One large mmap'd RW slab. Buffers carved bump-pointer style.
 * bb_seal() calls mprotect on the page(s) covering the buffer → RX.
 * bb_free() rewinds the bump pointer (LIFO) and mprotects back → RW.
 *
 * Design notes:
 *
 *   Why one big slab?
 *     Simple. No fragmentation. Pool is private to one statement's
 *     execution; it is reclaimed wholesale when the statement retires.
 *     4 MB is far more than any realistic pattern graph needs.
 *
 *   Why LIFO?
 *     Byrd box backtracking is naturally LIFO: we build a graph,
 *     drive it, and on failure discard the whole graph. The discard
 *     order matches allocation order in reverse. No GC needed.
 *
 *   Why mprotect per bb_seal / bb_free?
 *     mprotect is the I-cache fence on x86-64. The OS serialises at
 *     the mprotect syscall; after it returns, the bytes we wrote as
 *     data are visible to instruction fetch. We cannot skip this step
 *     and just cast a pointer — the CPU may have stale I-cache lines.
 *     mprotect back to RW on bb_free lets the next allocation reuse
 *     the same pages without a fresh mmap.
 *
 *   Alignment:
 *     All allocations are 16-byte aligned. x86-64 does not require
 *     instruction alignment, but 16-byte alignment keeps cache lines
 *     clean and avoids any future SIMD concerns.
 *
 *   mprotect granularity:
 *     mprotect operates on whole pages (4096 bytes on x86-64).
 *     bb_seal / bb_free round the address range to page boundaries.
 *     Since the pool is one contiguous slab from a single mmap call,
 *     all page addresses are valid targets.
 */

#include "bb_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* ── internal state ─────────────────────────────────────────────────────── */

static uint8_t *pool_base  = NULL;   /* start of mmap'd slab */
static uint8_t *pool_top   = NULL;   /* bump pointer (next free byte) */
static uint8_t *pool_limit = NULL;   /* one past end of slab */
static long     page_size  = 0;

/* ── helpers ────────────────────────────────────────────────────────────── */

/* Round p DOWN to nearest page boundary */
static uint8_t *page_floor(uint8_t *p) {
    uintptr_t u = (uintptr_t)p;
    return (uint8_t *)(u & ~(uintptr_t)(page_size - 1));
}

/* Round p UP to nearest page boundary */
static uint8_t *page_ceil(uint8_t *p) {
    uintptr_t u = (uintptr_t)p;
    uintptr_t ps = (uintptr_t)page_size;
    return (uint8_t *)((u + ps - 1) & ~(ps - 1));
}

/* ── public API ─────────────────────────────────────────────────────────── */

void bb_pool_init(void)
{
    if (pool_base) return;   /* idempotent */

    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) page_size = 4096;

    pool_base = mmap(NULL, BB_POOL_SIZE,
                     PROT_READ | PROT_WRITE,
                     MAP_ANON | MAP_PRIVATE,
                     -1, 0);
    if (pool_base == MAP_FAILED) {
        perror("bb_pool_init: mmap");
        abort();
    }
    pool_top   = pool_base;
    pool_limit = pool_base + BB_POOL_SIZE;
}

bb_buf_t bb_alloc(size_t size)
{
    if (!pool_base) {
        fprintf(stderr, "bb_alloc: pool not initialised\n");
        abort();
    }

    /*
     * Page-align the start of every allocation so each box owns its
     * pages exclusively. This is required for bb_seal correctness:
     * mprotect operates on whole pages, so two boxes sharing a page
     * would have the second box's write location sealed when the first
     * box is sealed. Page-aligning ensures no two live allocations
     * ever share a page.
     *
     * Memory cost: up to one page of padding per allocation.
     * At 4 MB pool / 4096 bytes per page = 1024 concurrent boxes max.
     * Far more than any realistic pattern graph needs.
     */
    uint8_t *start = page_ceil(pool_top);

    /* Round size up to whole pages */
    uintptr_t ps   = (uintptr_t)page_size;
    size_t    pages = ((size_t)size + (size_t)ps - 1) / (size_t)ps;
    size_t    alloc = pages * (size_t)ps;

    if (start + alloc > pool_limit) {
        fprintf(stderr, "bb_alloc: pool exhausted (need %zu, have %zu)\n",
                alloc, (size_t)(pool_limit - start));
        abort();
    }

    pool_top = start + alloc;
    return start;
}

void bb_seal(bb_buf_t buf, size_t size)
{
    uint8_t *lo = page_floor(buf);
    uint8_t *hi = page_ceil(buf + size);
    size_t   len = (size_t)(hi - lo);

    if (mprotect(lo, len, PROT_READ | PROT_EXEC) != 0) {
        perror("bb_seal: mprotect RW→RX");
        abort();
    }
}

void bb_free(bb_buf_t buf, size_t size)
{
    uintptr_t ps    = (uintptr_t)page_size;
    size_t    pages = ((size_t)size + (size_t)ps - 1) / (size_t)ps;
    size_t    alloc = pages * (size_t)ps;

    /* Validate LIFO order */
    if (buf + alloc != pool_top) {
        fprintf(stderr,
                "bb_free: LIFO violation — buf=%p + alloc=%zu != top=%p\n",
                (void *)buf, alloc, (void *)pool_top);
        abort();
    }

    /* Rewind bump pointer */
    pool_top = buf;

    /* Restore pages to RW for reuse */
    if (mprotect(buf, alloc, PROT_READ | PROT_WRITE) != 0) {
        perror("bb_free: mprotect RX→RW");
        abort();
    }
}

void bb_pool_destroy(void)
{
    if (!pool_base) return;
    munmap(pool_base, BB_POOL_SIZE);
    pool_base  = NULL;
    pool_top   = NULL;
    pool_limit = NULL;
}

size_t bb_pool_used(void)
{
    if (!pool_base) return 0;
    return (size_t)(pool_top - pool_base);
}
