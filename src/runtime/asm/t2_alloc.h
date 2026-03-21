/*
 * t2_alloc.h — Technique 2 dynamic memory allocation API
 */
#ifndef T2_ALLOC_H
#define T2_ALLOC_H

#include <stddef.h>

void *t2_alloc(size_t sz);
void  t2_free(void *p, size_t sz);
void  t2_flush_icache(void *p, size_t sz);
int   t2_mprotect_rx(void *p, size_t sz);
int   t2_mprotect_rw(void *p, size_t sz);

#endif /* T2_ALLOC_H */
