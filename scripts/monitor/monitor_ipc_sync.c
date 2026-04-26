/*
 * monitor_ipc_sync.c — SNOBOL4 LOAD()able sync-step IPC module.
 *
 * Wire protocol — RS/US delimiters (ASCII 0x1E / 0x1F):
 *   KIND \x1E name \x1F value \x1E
 *
 *   \x1E (RS, Record Separator) terminates each record — readline on \x1E
 *   \x1F (US, Unit Separator)   separates name from value within a record
 *
 *   Neither character appears in normal SNOBOL4 string data.
 *   Newlines, quotes, backslashes in values pass through unescaped.
 *   The controller reads up to \x1E for a complete record, splits on \x1F.
 *
 * Two FIFOs per participant:
 *   MONITOR_READY_PIPE     — ready pipe  (participant writes, controller reads)
 *   MONITOR_GO_PIPE — go pipe    (controller writes, participant reads)
 *
 * Barrier protocol per trace event:
 *   1. participant writes "KIND\x1Ename\x1Fvalue\x1E" to ready pipe
 *   2. participant blocks read() on go pipe
 *   3. controller reads one record from each of all 5 ready pipes
 *   4. consensus rule applied (oracle = participant 0 = CSNOBOL4)
 *   5. controller writes 'G' (go) or 'S' (stop) to each go pipe
 *   6. 'G' → MON_SEND returns success; 'S' → MON_SEND returns FAIL → :F(END)
 *
 * Build:
 *   gcc -shared -fPIC -O2 -Wall -o monitor_ipc_sync.so monitor_ipc_sync.c
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/uio.h>

typedef long      int_t;
typedef double    real_t;

struct descr {
    union { int_t i; real_t f; } a;
    char         f;
    unsigned int v;
};

#define DESCR_SZ   ((int)sizeof(struct descr))
#define BCDFLD     (4 * DESCR_SZ)

#define LOAD_PROTO  struct descr *retval, unsigned nargs, struct descr *args
#define LA_ALIST    LOAD_PROTO
typedef int lret_t;

#define TRUE  1
#define FALSE 0

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

extern void retstring(struct descr *retval, const char *cp, int len);

#define RETSTR(CP, LEN) \
    do { retstring(retval, (CP), (LEN)); return TRUE; } while (0)
#define RETNULL \
    do { retval->a.i = 0; retval->f = 0; retval->v = 0; return TRUE; } while (0)
#define RETFAIL return FALSE

static int copy_str_arg(int n, char *buf, int bufsz, struct descr *args) {
    int len = _len(n, args);
    const char *ptr = _ptr(n, args);
    if (len < 0 || len >= bufsz) return -1;
    if (ptr && len > 0) memcpy(buf, ptr, len);
    buf[len] = '\0';
    return 0;
}

/* Module state */
static int mon_ready_fd = -1;   /* write end of ready pipe */
static int mon_go_fd   = -1;   /* read end of go pipe   */

/*
 * MON_OPEN(ready_pipe_path, go_pipe_path) → ready_pipe_path or FAIL
 */
lret_t MON_OPEN(LA_ALIST) {
    char ready_path[4096];
    char go_path[4096];

    if (copy_str_arg(0, ready_path, sizeof(ready_path), args) < 0) RETFAIL;
    if (copy_str_arg(1, go_path,   sizeof(go_path),   args) < 0) RETFAIL;
    if (!ready_path[0] || !go_path[0]) RETFAIL;

    if (mon_ready_fd >= 0) { close(mon_ready_fd); mon_ready_fd = -1; }
    if (mon_go_fd   >= 0) { close(mon_go_fd);   mon_go_fd   = -1; }

    /* Event FIFO: write end — non-blocking open (reader already waiting) */
    mon_ready_fd = open(ready_path, O_WRONLY | O_NONBLOCK);
    if (mon_ready_fd < 0) mon_ready_fd = open(ready_path, O_WRONLY);
    if (mon_ready_fd < 0) RETFAIL;

    /* Ack FIFO: read end — open O_NONBLOCK so we don't deadlock waiting for
     * the controller's write side. Clear O_NONBLOCK after open so the
     * blocking read() in MON_SEND works correctly. */
    mon_go_fd = open(go_path, O_RDONLY | O_NONBLOCK);
    if (mon_go_fd < 0) { close(mon_ready_fd); mon_ready_fd = -1; RETFAIL; }
    { int fl = fcntl(mon_go_fd, F_GETFL);
      fcntl(mon_go_fd, F_SETFL, fl & ~O_NONBLOCK); }

    RETSTR(ready_path, (int)strlen(ready_path));
}

/*
 * MON_SEND(kind, body) → kind on GO, FAIL on STOP or error
 *
 * Wire format: KIND \x1E body \x1E
 *   where body = name \x1F value  (for VALUE/CALL/RETURN events)
 *
 * \x1E (RS 0x1E) = record terminator — controller reads until \x1E
 * \x1F (US 0x1F) = field separator  — separates name from value
 *
 * Newlines and all other bytes in name/value pass through unescaped.
 *
 * Writes event then BLOCKS waiting for 1-byte ack from controller.
 * 'G' → return success (participant continues)
 * 'S' → return FAIL   (participant should branch to END)
 */
#define RS "\x1e"
#define US "\x1f"

lret_t MON_SEND(LA_ALIST) {
    if (mon_ready_fd < 0) RETNULL;  /* not open — silent no-op */

    /* Extract kind and body as raw byte strings (may contain any byte) */
    const char *kptr = _ptr(0, args); int klen = _len(0, args);
    const char *bptr = _ptr(1, args); int blen = _len(1, args);
    if (!kptr) kptr = ""; if (klen < 0) klen = 0;
    if (!bptr) bptr = ""; if (blen < 0) blen = 0;

    /* Build record: KIND RS body RS  (body already contains US between name/value) */
    struct iovec iov[4];
    iov[0].iov_base = (void*)kptr;  iov[0].iov_len = (size_t)klen;
    iov[1].iov_base = RS;           iov[1].iov_len = 1;
    iov[2].iov_base = (void*)bptr;  iov[2].iov_len = (size_t)blen;
    iov[3].iov_base = RS;           iov[3].iov_len = 1;

    ssize_t total = (ssize_t)(klen + 1 + blen + 1);
    ssize_t written = writev(mon_ready_fd, iov, 4);
    if (written != total) RETFAIL;

    /* Block waiting for ack */
    if (mon_go_fd < 0) RETFAIL;
    char ack[1];
    ssize_t r = read(mon_go_fd, ack, 1);
    if (r != 1) RETFAIL;
    if (ack[0] == 'S') RETFAIL;

    RETSTR(kptr, klen);
}

/*
 * MON_CLOSE() → ""
 */
lret_t MON_CLOSE(LA_ALIST) {
    if (mon_ready_fd >= 0) { close(mon_ready_fd); mon_ready_fd = -1; }
    if (mon_go_fd   >= 0) { close(mon_go_fd);   mon_go_fd   = -1; }
    RETNULL;
}
