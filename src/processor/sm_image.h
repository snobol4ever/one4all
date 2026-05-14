/*
 * sm_image.h — Unified SCRIP In-Memory CODE_t Image (M-SCRIP-U1)
 *
 * Manages the 5 mmap'd segments that make up a running SCRIP program:
 *
 *   SEG_STUBS   — runtime C function pointer table (NV_GET_fn, stmt_exec_dyn...)
 *   SEG_DISPATCH — SM instruction dispatch blobs (one per SM opcode, shared)
 *   SEG_CODE    — program body: SM_Program lowered to concatenated x86 blobs
 *   SEG_BYRD    — Byrd box pool (per-statement; managed by bb_pool.c separately)
 *   SEG_DATA    — string literals, constants, GC heap root (RW, never sealed)
 *
 * Each segment is an independent mmap(MAP_ANON|MAP_PRIVATE) slab.
 * SEG_STUBS / SEG_DISPATCH / SEG_CODE are sealed RX after population.
 * SEG_DATA stays RW throughout.
 * SEG_BYRD is managed by bb_pool.c — not in this API.
 *
 * Design principle: same primitives as bb_pool / bb_emit but named,
 * fixed-slot, not LIFO — program image is built once and stays live.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-04-06 (M-SCRIP-U1)
 */

#ifndef SCRIP_IMAGE_H
#define SCRIP_IMAGE_H

#include <stddef.h>
#include <stdint.h>

/* ── Segment identifiers ─────────────────────────────────────────────── */

typedef enum {
    SEG_STUBS    = 0,   /* C runtime fn ptr table — sealed RX */
    SEG_DISPATCH = 1,   /* SM opcode dispatch blobs — sealed RX */
    SEG_CODE     = 2,   /* program body blobs — sealed RX */
    SEG_DATA     = 3,   /* literals/constants — stays RW */
    SEG_COUNT    = 4
} scrip_seg_id;

/* ── Default sizes (can be overridden at init) ──────────────────────── */

#define SCRIP_SEG_STUBS_SIZE    (  64 * 1024)   /*  64 KB: ~8000 fn ptrs */
#define SCRIP_SEG_DISPATCH_SIZE ( 256 * 1024)   /* 256 KB: ~200 SM opcodes × ~1KB each */
#define SCRIP_SEG_CODE_SIZE     (  16 * 1024 * 1024)  /* 16 MB: large programs */
#define SCRIP_SEG_DATA_SIZE     (   4 * 1024 * 1024)  /*  4 MB: string pool */

/* ── Segment descriptor ──────────────────────────────────────────────── */

typedef struct {
    uint8_t  *base;     /* mmap'd slab start */
    uint8_t  *top;      /* bump pointer (next free byte) */
    uint8_t  *limit;    /* one past end */
    int       sealed;   /* 1 after seg_seal() — writes forbidden */
    scrip_seg_id id;
} scrip_seg_t;

/* ── Global image (one per process) ────────────────────────────────── */

extern scrip_seg_t scrip_segs[SEG_COUNT];

/* ── Lifecycle ──────────────────────────────────────────────────────── */

/*
 * Allocate all segments. Must be called once before any seg_*() calls.
 * Returns 0 on success, -1 on mmap failure (prints errno to stderr).
 */
int sm_image_init(void);

/*
 * Release all segment slabs. Call once at shutdown.
 */
void sm_image_destroy(void);

/* ── Per-segment operations ─────────────────────────────────────────── */

/*
 * Append `size` bytes into segment `id`, bump-pointer style.
 * Returns pointer to the allocated region (16-byte aligned).
 * Aborts with message if segment is full or already sealed.
 */
uint8_t *seg_alloc(scrip_seg_id id, size_t size);

/*
 * Append a single byte into segment `id`.
 */
void seg_byte(scrip_seg_id id, uint8_t b);

/*
 * Append a 4-byte little-endian value into segment `id`.
 */
void seg_u32(scrip_seg_id id, uint32_t v);

/*
 * Append an 8-byte little-endian value into segment `id`.
 */
void seg_u64(scrip_seg_id id, uint64_t v);

/*
 * Current write offset (bytes from segment base). Used for label values.
 */
size_t seg_offset(scrip_seg_id id);

/*
 * Patch a 4-byte little-endian word at byte offset `off` in segment `id`.
 * Used to fix up forward jump targets after labels are resolved.
 * Segment must NOT be sealed yet.
 */
void seg_patch_u32(scrip_seg_id id, size_t off, uint32_t v);

/*
 * Seal segment as PROT_READ|PROT_EXEC (I-cache fence via mprotect).
 * After this call, seg_alloc/seg_byte/seg_u32/seg_u64 on this segment abort.
 * SEG_DATA must not be sealed — call only for SEG_STUBS/SEG_DISPATCH/SEG_CODE.
 */
void seg_seal(scrip_seg_id id);

/*
 * Diagnostic: bytes used in segment.
 */
size_t seg_used(scrip_seg_id id);

/* ── Stub table helpers (SEG_STUBS) ────────────────────────────────── */

/*
 * Append a 64-bit absolute pointer to SEG_STUBS.
 * Returns the offset of the slot (use to read ptr back via SEG_STUBS base+off).
 * Called at startup to populate the C runtime function pointer table.
 */
size_t seg_stubs_add_ptr(void *fn);

/*
 * Retrieve the address of a stub slot by its offset (as returned by seg_stubs_add_ptr).
 * Code in SEG_CODE uses this address as an imm64 → indirect call target.
 */
void **seg_stubs_slot(size_t off);

#endif /* SCRIP_IMAGE_H */
