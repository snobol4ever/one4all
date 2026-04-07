/*
 * bb_pool.h — Dynamic Byrd Box Buffer Pool
 *
 * Manages mmap'd executable memory in a LIFO stack of slabs.
 * Each slab is one or more pages. Buffers are allocated bump-pointer
 * style from the current slab. Seal transitions RW→RX (I-cache fence).
 * Free is LIFO — backtracking discards the top allocation.
 *
 * Lifecycle:
 *   bb_pool_init()          — call once at startup
 *   buf = bb_alloc(size)    — carve RW buffer from pool (bump pointer)
 *   bb_seal(buf, size)      — transition buf's page(s) RW→RX
 *   bb_free(buf)            — LIFO reclaim (must be last bb_alloc result)
 *   bb_pool_destroy()       — call once at shutdown
 *
 * Constraints:
 *   - bb_free() must be called in reverse allocation order (LIFO).
 *   - bb_seal() must be called before the buffer is jumped into.
 *   - After bb_seal(), the buffer is read-execute; do not write to it.
 *   - size passed to bb_free() must equal size passed to bb_alloc().
 *
 * Thread safety: none. Single-threaded use only.
 */

#ifndef BB_POOL_H
#define BB_POOL_H

#include <stddef.h>
#include <stdint.h>

/* Opaque buffer handle — just a pointer to the RW/RX memory */
typedef uint8_t * bb_buf_t;

/* Pool capacity: total mmap'd bytes available */
#define BB_POOL_SIZE   (4 * 1024 * 1024)   /* 4 MB — plenty for deep patterns */

/* Initialise the pool. Must be called before any bb_alloc(). */
void bb_pool_init(void);

/*
 * Allocate a RW buffer of at least `size` bytes from the pool.
 * Returns pointer to writable memory, aligned to 16 bytes.
 * Aborts if pool is exhausted (should never happen in practice).
 */
bb_buf_t bb_alloc(size_t size);

/*
 * Seal the page(s) containing buf[0..size-1] as PROT_READ|PROT_EXEC.
 * This is the I-cache fence on x86-64.
 * After this call the buffer must not be written to.
 */
void bb_seal(bb_buf_t buf, size_t size);

/*
 * Free the most-recently-allocated buffer (LIFO).
 * buf must equal the return value of the most recent bb_alloc().
 * size must equal the size passed to that bb_alloc().
 * On backtrack: the pool bump pointer is rewound; pages return to RW.
 */
void bb_free(bb_buf_t buf, size_t size);

/* Release all pool memory. Call once at shutdown. */
void bb_pool_destroy(void);

/* Diagnostic: bytes currently in use */
size_t bb_pool_used(void);

#endif /* BB_POOL_H */
