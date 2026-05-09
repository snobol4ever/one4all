/*============================================================================
 * rs23_diag.c — RS-23 diagnostic wrapper for interp_eval
 *
 * Linked into scrip via `-Wl,--wrap=interp_eval`.  Every interp_eval call
 * goes through __wrap_interp_eval first.  We use backtrace(3) to inspect
 * the call chain; if any ancestor frame is a BB-adapter symbol
 * (bb_eval_value, bb_exec_stmt, coro_call, coro_eval, coro_drive,
 * coro_drive_fnc, coro_bb_every) we log the (kind, immediate-caller) pair
 * once and then defer to __real_interp_eval.
 *
 * Output: one line per unique (kind, caller) pair to stderr, prefixed
 * RS23DIAG: so a smoke harness can grep for it.  The dedup table is
 * fixed-size; if it overflows we just stop logging.
 *
 * This file is NOT shipped — it is a diagnostic build only, gated by
 * the -DRS23_DIAG compile flag and the wrap link flag.  Revert before
 * RS-23 lands.
 *==========================================================================*/

#ifdef RS23_DIAG

#define _GNU_SOURCE
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "descr.h"     /* DESCR_t */
#include "scrip_cc.h"   /* AST_t, AST_e, ast_e_name[] via IR_DEFINE_NAMES */
#include "ir/ir.h"

DESCR_t __real_interp_eval(AST_t *e);

/* Fixed-size dedup: (kind << 24) | (hash(caller) & 0xFFFFFF). */
#define DEDUP_CAP 1024
static unsigned long g_seen[DEDUP_CAP];
static int g_seen_n = 0;

static int seen_check_add(unsigned long key) {
    for (int i = 0; i < g_seen_n; i++) if (g_seen[i] == key) return 1;
    if (g_seen_n < DEDUP_CAP) { g_seen[g_seen_n++] = key; return 0; }
    return 1;  /* full — pretend seen */
}

/* Walk frames, return non-zero if any frame's symbol contains one of the
 * BB-adapter substrings.  Also fills out_caller with the symbol name of
 * the immediate caller of __wrap_interp_eval (the function that called
 * interp_eval), and out_bbcaller with the BB-adapter ancestor name. */
static int has_bb_ancestor(char *out_caller, size_t cap,
                           char *out_bb, size_t bb_cap) {
    void *frames[40];
    int n = backtrace(frames, 40);
    if (n <= 0) return 0;

    out_caller[0] = '\0';
    out_bb[0] = '\0';

    int found = 0;
    /* frame 0 = backtrace itself, frame 1 = has_bb_ancestor, frame 2 =
     * __wrap_interp_eval, frame 3 = the immediate caller (logged), frame
     * 4+ = ancestors we scan for BB markers. */
    for (int i = 3; i < n; i++) {
        Dl_info info;
        if (!dladdr(frames[i], &info) || !info.dli_sname) continue;
        const char *s = info.dli_sname;
        if (i == 3) {
            strncpy(out_caller, s, cap - 1);
            out_caller[cap - 1] = '\0';
        }
        if (strstr(s, "bb_eval_value") ||
            strstr(s, "bb_exec_stmt")  ||
            strstr(s, "coro_call")     ||
            strstr(s, "coro_eval")     ||
            strstr(s, "coro_drive")    ||
            strstr(s, "coro_bb_every")) {
            if (!found) { strncpy(out_bb, s, bb_cap - 1); out_bb[bb_cap - 1] = '\0'; found = 1; }
        }
    }
    return found;
}

/* Cheap string hash for dedup. */
static unsigned long shash(const char *s) {
    unsigned long h = 5381;
    if (!s) return 0;
    while (*s) { h = ((h << 5) + h) + (unsigned char)*s++; }
    return h;
}

static FILE *g_diag_fp = NULL;
static unsigned long g_wrap_calls = 0;

static void diag_init_once(void) {
    if (g_diag_fp) return;
    const char *p = getenv("RS23_DIAG_LOG");
    if (!p) p = "/tmp/rs23_diag.log";
    g_diag_fp = fopen(p, "a");
    if (!g_diag_fp) g_diag_fp = stderr;
}

DESCR_t __wrap_interp_eval(AST_t *e) {
    diag_init_once();
    g_wrap_calls++;
    char caller[128] = {0};
    char bbanc[128]  = {0};
    if (e && has_bb_ancestor(caller, sizeof caller, bbanc, sizeof bbanc)) {
        int k = (int)e->kind;
        const char *kname =
            (k >= 0 && k < AST_KIND_COUNT && ast_e_name[k])
                ? ast_e_name[k] : "?";
        unsigned long key = ((unsigned long)k << 32) ^ shash(caller) ^ (shash(bbanc) << 1);
        if (!seen_check_add(key)) {
            fprintf(g_diag_fp,
                    "RS23DIAG: kind=%-18s caller=%-32s via=%s\n",
                    kname, caller, bbanc);
            fflush(g_diag_fp);
        }
    }
    return __real_interp_eval(e);
}

#endif  /* RS23_DIAG */
