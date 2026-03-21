/*
 * monitor_ipc.c — SNOBOL4 LOAD()able IPC module for the 5-way monitor.
 *
 * Three functions, identical ABI for CSNOBOL4 and SPITBOL x64:
 *
 *   LOAD("MON_OPEN(STRING)STRING",        "./monitor_ipc.so")
 *   LOAD("MON_SEND(STRING,STRING)STRING", "./monitor_ipc.so")
 *   LOAD("MON_CLOSE()STRING",             "./monitor_ipc.so")
 *
 * MON_OPEN(fifo_path)  — open named FIFO O_WRONLY, store fd in module state
 * MON_SEND(kind, body) — write "KIND body\n" atomically (< PIPE_BUF)
 * MON_CLOSE()          — close FIFO fd
 *
 * ABI: lret_t fn(LA_ALIST)
 *      = int fn(struct descr *retval, unsigned nargs, struct descr *args)
 *
 * CSNOBOL4 string descriptor layout (64-bit, NO_BITFIELDS):
 *   struct descr { union{long i; double f;} a; char f; unsigned int v; }
 *   sizeof(struct descr) = 16, BCDFLD = 4*16 = 64
 *   For a STRING arg N:
 *     args[N].a.i  = pointer to string block (the "spec")
 *     *((struct descr*)args[N].a.i).v = string byte length
 *     (char*)args[N].a.i + BCDFLD = pointer to actual string bytes (NOT NUL-terminated)
 *
 * Build (standalone, no CSNOBOL4 headers required):
 *   gcc -shared -fPIC -O2 -Wall -o monitor_ipc.so monitor_ipc.c
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/* -----------------------------------------------------------------------
 * ABI definitions matching CSNOBOL4 2.3.3 on x86-64, NO_BITFIELDS build.
 * Verified empirically: sizeof(struct descr)=16, BCDFLD=64.
 * ----------------------------------------------------------------------- */

typedef long      int_t;
typedef double    real_t;

struct descr {
    union { int_t i; real_t f; } a;   /* address / integer / real */
    char         f;                    /* flags (char, not bitfield) */
    unsigned int v;                    /* type tag for outer spec; length in block hdr */
};

#define DESCR_SZ   ((int)sizeof(struct descr))  /* 16 */
#define BCDFLD     (4 * DESCR_SZ)               /* 64 — string data offset in block */

#define LOAD_PROTO  struct descr *retval, unsigned nargs, struct descr *args
#define LA_ALIST    LOAD_PROTO
typedef int lret_t;

#define TRUE  1
#define FALSE 0

/*
 * String arg N extraction:
 *   args[N].a.i  = ptr to string block (NULL → empty string)
 *   block hdr[0].v = byte length of string data
 *   (char*)block + BCDFLD = string bytes (NOT NUL-terminated)
 */
static inline void *_blk(int n, struct descr *args) {
    return (void *)(args[n].a.i);
}
static inline int _len(int n, struct descr *args) {
    void *blk = _blk(n, args);
    if (!blk) return 0;
    return (int)((struct descr *)blk)->v;
}
static inline const char *_ptr(int n, struct descr *args) {
    void *blk = _blk(n, args);
    if (!blk) return NULL;
    return (const char *)blk + BCDFLD;
}

/* retstring() is exported from the snobol4 binary; dynamic linker resolves it. */
extern void retstring(struct descr *retval, const char *cp, int len);

#define RETSTR(CP, LEN) \
    do { retstring(retval, (CP), (LEN)); return TRUE; } while (0)

#define RETNULL \
    do { retval->a.i = 0; retval->f = 0; retval->v = 0; return TRUE; } while (0)

#define RETFAIL return FALSE

/* Copy string arg N into NUL-terminated buf[bufsz]. Returns 0 ok, -1 overflow. */
static int copy_str_arg(int n, char *buf, int bufsz, struct descr *args) {
    int len = _len(n, args);
    const char *ptr = _ptr(n, args);
    if (len < 0 || len >= bufsz) return -1;
    if (ptr && len > 0) memcpy(buf, ptr, len);
    buf[len] = '\0';
    return 0;
}

/* -----------------------------------------------------------------------
 * Module state
 * ----------------------------------------------------------------------- */

static int mon_fd = -1;

/* -----------------------------------------------------------------------
 * MON_OPEN(fifo_path) STRING → returns fifo_path on success, FAIL on error
 * ----------------------------------------------------------------------- */
lret_t MON_OPEN(LA_ALIST) {
    char path[4096];
    if (copy_str_arg(0, path, sizeof(path), args) < 0) RETFAIL;
    if (!path[0]) RETFAIL;

    if (mon_fd >= 0) { close(mon_fd); mon_fd = -1; }

    /* Try non-blocking first (reader already present), fall back to blocking */
    mon_fd = open(path, O_WRONLY | O_NONBLOCK);
    if (mon_fd < 0) mon_fd = open(path, O_WRONLY);
    if (mon_fd < 0) RETFAIL;

    RETSTR(path, (int)strlen(path));
}

/* -----------------------------------------------------------------------
 * MON_SEND(kind, body) STRING → returns kind on success, FAIL on error
 *
 * Writes "KIND body\n" as a single atomic write() < PIPE_BUF (4096 bytes).
 * ----------------------------------------------------------------------- */
lret_t MON_SEND(LA_ALIST) {
    char kind[64];
    char body[3900];
    char line[4096];

    if (mon_fd < 0) RETNULL;   /* silently ignore if not open */

    if (copy_str_arg(0, kind, sizeof(kind), args) < 0) RETFAIL;
    if (copy_str_arg(1, body, sizeof(body), args) < 0) RETFAIL;

    int n = snprintf(line, sizeof(line), "%s %s\n", kind, body);
    if (n <= 0 || n >= (int)sizeof(line)) RETFAIL;

    ssize_t written = write(mon_fd, line, (size_t)n);
    if (written != (ssize_t)n) RETFAIL;

    int klen = (int)strlen(kind);
    RETSTR(kind, klen);
}

/* -----------------------------------------------------------------------
 * MON_CLOSE() STRING → returns "" on success
 * ----------------------------------------------------------------------- */
lret_t MON_CLOSE(LA_ALIST) {
    if (mon_fd >= 0) { close(mon_fd); mon_fd = -1; }
    RETNULL;
}
