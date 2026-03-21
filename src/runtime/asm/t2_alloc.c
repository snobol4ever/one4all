/*
 * t2_alloc.c — Technique 2 dynamic memory allocation and protection
 *
 * Provides mmap/munmap-based RW/RX memory for T2 pattern box instances.
 * Each pattern box invocation gets its own TEXT+DATA block pair:
 *   TEXT: allocated RW, filled, then mprotect'd RX before jump
 *   DATA: allocated RW, stays RW throughout invocation lifetime
 *
 * API:
 *   void *t2_alloc(size_t sz)           — mmap anonymous RW page(s)
 *   void  t2_free(void *p, size_t sz)   — munmap
 *   int   t2_mprotect_rx(void *p, size_t sz) — RW → RX (ready to execute)
 *   int   t2_mprotect_rw(void *p, size_t sz) — RX → RW (for patching)
 *
 * All sizes are rounded up to the system page size internally.
 */

#include "t2_alloc.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#include <errno.h>

/* Round sz up to the next multiple of the system page size. */
static size_t page_round(size_t sz) {
    size_t pgsz = (size_t)sysconf(_SC_PAGESIZE);
    return (sz + pgsz - 1) & ~(pgsz - 1);
}

/*
 * t2_alloc — allocate sz bytes of anonymous RW memory.
 * Returns pointer on success, NULL on failure (sets errno).
 */
void *t2_alloc(size_t sz) {
    if (sz == 0) sz = 1;
    void *p = mmap(NULL, page_round(sz),
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
    if (p == MAP_FAILED) return NULL;
    return p;
}

/*
 * t2_free — release memory previously obtained from t2_alloc.
 * sz must match the value passed to t2_alloc.
 */
void t2_free(void *p, size_t sz) {
    if (!p || sz == 0) return;
    munmap(p, page_round(sz));
}

/*
 * t2_mprotect_rx — make a region read+execute (no write).
 * Call after copying TEXT into the block and before jumping to it.
 * Returns 0 on success, -1 on failure (errno set).
 */
int t2_mprotect_rx(void *p, size_t sz) {
    return mprotect(p, page_round(sz), PROT_READ | PROT_EXEC);
}

/*
 * t2_mprotect_rw — make a region read+write (no execute).
 * Call before patching relocations into a TEXT block.
 * Returns 0 on success, -1 on failure (errno set).
 */
int t2_mprotect_rw(void *p, size_t sz) {
    return mprotect(p, page_round(sz), PROT_READ | PROT_WRITE);
}
