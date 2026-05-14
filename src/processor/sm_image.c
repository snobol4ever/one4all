/*
 * scrip_image.c — Unified SCRIP In-Memory CODE_t Image (M-SCRIP-U1)
 *
 * See sm_image.h for design notes.
 *
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6
 * Date: 2026-04-06 (M-SCRIP-U1)
 */

#include "sm_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

/* ── Global image ───────────────────────────────────────────────────── */

scrip_seg_t scrip_segs[SEG_COUNT];

/* ── Internal helpers ───────────────────────────────────────────────── */

static long page_sz = 0;

static uint8_t *page_floor(uint8_t *p) {
    uintptr_t u = (uintptr_t)p;
    return (uint8_t *)(u & ~(uintptr_t)(page_sz - 1));
}

static uint8_t *page_ceil(uint8_t *p) {
    uintptr_t u = (uintptr_t)p + (uintptr_t)(page_sz - 1);
    return (uint8_t *)(u & ~(uintptr_t)(page_sz - 1));
}

static uint8_t *align16(uint8_t *p) {
    uintptr_t u = ((uintptr_t)p + 15) & ~(uintptr_t)15;
    return (uint8_t *)u;
}

/* ── Lifecycle ──────────────────────────────────────────────────────── */

static size_t default_sizes[SEG_COUNT] = {
    SCRIP_SEG_STUBS_SIZE,
    SCRIP_SEG_DISPATCH_SIZE,
    SCRIP_SEG_CODE_SIZE,
    SCRIP_SEG_DATA_SIZE,
};

static const char *seg_names[SEG_COUNT] = {
    "SEG_STUBS", "SEG_DISPATCH", "SEG_CODE", "SEG_DATA"
};

int sm_image_init(void)
{
    page_sz = sysconf(_SC_PAGESIZE);
    if (page_sz <= 0) page_sz = 4096;

    for (int i = 0; i < SEG_COUNT; i++) {
        size_t sz = default_sizes[i];
        /* round up to page boundary */
        sz = (sz + (size_t)(page_sz - 1)) & ~(size_t)(page_sz - 1);

        void *m = mmap(NULL, sz,
                       PROT_READ | PROT_WRITE,
                       MAP_ANON | MAP_PRIVATE, -1, 0);
        if (m == MAP_FAILED) {
            fprintf(stderr, "sm_image_init: mmap %s (%zu bytes) failed: %s\n",
                    seg_names[i], sz, strerror(errno));
            /* roll back already-mapped segments */
            for (int j = 0; j < i; j++) {
                munmap(scrip_segs[j].base,
                       (size_t)(scrip_segs[j].limit - scrip_segs[j].base));
                memset(&scrip_segs[j], 0, sizeof scrip_segs[j]);
            }
            return -1;
        }

        scrip_segs[i].base   = (uint8_t *)m;
        scrip_segs[i].top    = (uint8_t *)m;
        scrip_segs[i].limit  = (uint8_t *)m + sz;
        scrip_segs[i].sealed = 0;
        scrip_segs[i].id     = (scrip_seg_id)i;
    }
    return 0;
}

void sm_image_destroy(void)
{
    for (int i = 0; i < SEG_COUNT; i++) {
        if (scrip_segs[i].base) {
            size_t sz = (size_t)(scrip_segs[i].limit - scrip_segs[i].base);
            munmap(scrip_segs[i].base, sz);
            memset(&scrip_segs[i], 0, sizeof scrip_segs[i]);
        }
    }
}

/* ── Per-segment operations ─────────────────────────────────────────── */

uint8_t *seg_alloc(scrip_seg_id id, size_t size)
{
    scrip_seg_t *s = &scrip_segs[id];
    if (s->sealed) {
        fprintf(stderr, "seg_alloc: %s is sealed — cannot write\n", seg_names[id]);
        abort();
    }
    uint8_t *p = align16(s->top);
    if (p + size > s->limit) {
        fprintf(stderr, "seg_alloc: %s exhausted (used=%zu limit=%zu requested=%zu)\n",
                seg_names[id],
                (size_t)(p - s->base),
                (size_t)(s->limit - s->base),
                size);
        abort();
    }
    s->top = p + size;
    return p;
}

void seg_byte(scrip_seg_id id, uint8_t b)
{
    scrip_seg_t *s = &scrip_segs[id];
    if (s->sealed) { fprintf(stderr, "seg_byte: %s sealed\n", seg_names[id]); abort(); }
    if (s->top >= s->limit) { fprintf(stderr, "seg_byte: %s full\n", seg_names[id]); abort(); }
    *s->top++ = b;
}

void seg_u32(scrip_seg_id id, uint32_t v)
{
    seg_byte(id, (uint8_t)(v));
    seg_byte(id, (uint8_t)(v >> 8));
    seg_byte(id, (uint8_t)(v >> 16));
    seg_byte(id, (uint8_t)(v >> 24));
}

void seg_u64(scrip_seg_id id, uint64_t v)
{
    seg_u32(id, (uint32_t)v);
    seg_u32(id, (uint32_t)(v >> 32));
}

size_t seg_offset(scrip_seg_id id)
{
    return (size_t)(scrip_segs[id].top - scrip_segs[id].base);
}

void seg_patch_u32(scrip_seg_id id, size_t off, uint32_t v)
{
    scrip_seg_t *s = &scrip_segs[id];
    if (s->sealed) {
        fprintf(stderr, "seg_patch_u32: %s is sealed\n", seg_names[id]);
        abort();
    }
    if (off + 4 > (size_t)(s->top - s->base)) {
        fprintf(stderr, "seg_patch_u32: %s offset %zu out of range (used=%zu)\n",
                seg_names[id], off, (size_t)(s->top - s->base));
        abort();
    }
    uint8_t *p = s->base + off;
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

void seg_seal(scrip_seg_id id)
{
    scrip_seg_t *s = &scrip_segs[id];
    if (s->sealed) return;  /* idempotent */
    if (id == SEG_DATA) {
        fprintf(stderr, "seg_seal: SEG_DATA must not be sealed (stays RW)\n");
        abort();
    }

    /* Round to page boundaries */
    uint8_t *lo = page_floor(s->base);
    uint8_t *hi = page_ceil(s->top);
    if (hi == lo) hi = lo + page_sz;  /* at least one page */

    if (mprotect(lo, (size_t)(hi - lo), PROT_READ | PROT_EXEC) != 0) {
        fprintf(stderr, "seg_seal: mprotect %s failed: %s\n",
                seg_names[id], strerror(errno));
        abort();
    }
    s->sealed = 1;
}

size_t seg_used(scrip_seg_id id)
{
    return (size_t)(scrip_segs[id].top - scrip_segs[id].base);
}

/* ── Stub table helpers ─────────────────────────────────────────────── */

size_t seg_stubs_add_ptr(void *fn)
{
    size_t off = seg_offset(SEG_STUBS);
    seg_u64(SEG_STUBS, (uint64_t)(uintptr_t)fn);
    return off;
}

void **seg_stubs_slot(size_t off)
{
    return (void **)(scrip_segs[SEG_STUBS].base + off);
}
