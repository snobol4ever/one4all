/* runtime.c — SNOBOL4-tiny static runtime implementation */

#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>

void sno_output(str_t s) {
    fwrite(s.ptr, 1, (size_t)s.len, stdout);
    fputc('\n', stdout);
}

void sno_output_cstr(const char *s) {
    fputs(s, stdout);
    fputc('\n', stdout);
}

/* Simple heap frame allocator for recursive patterns.
 * Pushes a new frame; frame_ptr always points to the current frame.
 * Frames are freed on sno_exit in LIFO order. */

typedef struct frame_header {
    struct frame_header *prev;
    size_t               size;
} frame_header_t;

void *sno_enter(void **frame_ptr, size_t frame_size) {
    frame_header_t *hdr = (frame_header_t *)malloc(sizeof(frame_header_t) + frame_size);
    if (!hdr) { fputs("sno_enter: out of memory\n", stderr); exit(1); }
    hdr->prev  = (frame_header_t *)*frame_ptr;
    hdr->size  = frame_size;
    *frame_ptr = (void *)(hdr + 1);
    return *frame_ptr;
}

void sno_exit(void **frame_ptr) {
    if (!*frame_ptr) return;
    frame_header_t *hdr = ((frame_header_t *)*frame_ptr) - 1;
    *frame_ptr = (void *)hdr->prev;
    free(hdr);
}
