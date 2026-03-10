/* runtime.h — SNOBOL4-tiny static runtime
 *
 * All match state is statically allocated. Zero allocation during matching.
 * CODE/EVAL dynamic patterns use heap (two-tier: static fast path + heap).
 */

#ifndef SNOBOL4_TINY_RUNTIME_H
#define SNOBOL4_TINY_RUNTIME_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ---------- string type ------------------------------------------- */

typedef struct {
    const char *ptr;
    int64_t     len;
} str_t;

#define STR_EMPTY  ((str_t){ "", 0 })
#define STR_LIT(s) ((str_t){ (s), (int64_t)(sizeof(s)-1) })

/* ---------- match state ------------------------------------------- */

typedef struct {
    const char *subject;
    int64_t     subject_len;
    int64_t     cursor;
} match_state_t;

/* ---------- output ------------------------------------------------- */

void sno_output(str_t s);
void sno_output_cstr(const char *s);

/* ---------- entry / exit frame ------------------------------------ */
/* Used by recursive patterns (test_sno_2.c calling convention)     */

void *sno_enter(void **frame_ptr, size_t frame_size);
void  sno_exit(void **frame_ptr);

#endif /* SNOBOL4_TINY_RUNTIME_H */
