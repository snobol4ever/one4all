#ifndef DESCR_H
#define DESCR_H
#include <stdint.h>
#include <stddef.h>
typedef enum {
    DT_SNUL =  0,
    DT_S    =  1,   /* STRING  — char*                          */
    DT_P    =  3,   /* PATTERN — PATND_t*                       */
    DT_A    =  4,   /* ARRAY   — ARBLK_t*                       */
    DT_T    =  5,   /* TABLE   — TBBLK_t*                       */
    DT_I    =  6,
    DT_R    =  7,
    DT_C    =  8,
    DT_N    =  9,
    DT_K    = 10,
    DT_E    = 11,
    DT_FH   = 12,
    DT_FAIL = 99,
    DT_DATA = 100,
} DTYPE_t;
struct _PATND_t;
struct _ARBLK_t;
struct _TBBLK_t;
struct _DATINST_t;
typedef struct DESCR_t {
    DTYPE_t  v;
    uint32_t slen;
    union {
        char              *s;
        int64_t            i;
        double             r;
        struct _PATND_t   *p;
        struct _ARBLK_t   *arr;
        struct _TBBLK_t   *tbl;
        struct _DATINST_t *u;
        void              *ptr;
    };
} DESCR_t;
#define FAILDESCR    ((DESCR_t){ .v = DT_FAIL, .i = 0 })
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline int IS_FAIL_fn(DESCR_t v) { return v.v == DT_FAIL; }
#define FHVAL(idx_) ((DESCR_t){ .v = DT_FH, .i = (int64_t)(idx_) })
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline int IS_FH_fn(DESCR_t v) { return v.v == DT_FH; }
#endif
