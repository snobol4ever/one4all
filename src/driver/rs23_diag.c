#ifdef RS23_DIAG
#define _GNU_SOURCE
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "descr.h"
#include "scrip_cc.h"
#include "ast/ast.h"
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t __real_interp_eval(tree_t *e);
#define DEDUP_CAP 1024
static unsigned long g_seen[DEDUP_CAP];
static int g_seen_n = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int seen_check_add(unsigned long key) {
    for (int i = 0; i < g_seen_n; i++) if (g_seen[i] == key) return 1;
    if (g_seen_n < DEDUP_CAP) { g_seen[g_seen_n++] = key; return 0; }
    return 1;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static int has_bb_ancestor(char *out_caller, size_t cap,
                           char *out_bb, size_t bb_cap) {
    void *frames[40];
    int n = backtrace(frames, 40);
    if (n <= 0) return 0;
    out_caller[0] = '\0';
    out_bb[0] = '\0';
    int found = 0;
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
            strstr(s, "icn_bb_build")     ||
            strstr(s, "coro_drive")    ||
            strstr(s, "icn_bb_every")) {
            if (!found) { strncpy(out_bb, s, bb_cap - 1); out_bb[bb_cap - 1] = '\0'; found = 1; }
        }
    }
    return found;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static unsigned long shash(const char *s) {
    unsigned long h = 5381;
    if (!s) return 0;
    while (*s) { h = ((h << 5) + h) + (unsigned char)*s++; }
    return h;
}
static FILE *g_diag_fp = NULL;
static unsigned long g_wrap_calls = 0;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static void diag_init_once(void) {
    if (g_diag_fp) return;
    const char *p = getenv("RS23_DIAG_LOG");
    if (!p) p = "/tmp/rs23_diag.log";
    g_diag_fp = fopen(p, "a");
    if (!g_diag_fp) g_diag_fp = stderr;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
DESCR_t __wrap_interp_eval(tree_t *e) {
    diag_init_once();
    g_wrap_calls++;
    char caller[128] = {0};
    char bbanc[128]  = {0};
    if (e && has_bb_ancestor(caller, sizeof caller, bbanc, sizeof bbanc)) {
        int k = (int)e->t;
        const char *kname =
            (k >= 0 && k < TT_KIND_COUNT && tt_e_name[k])
                ? tt_e_name[k] : "?";
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
#endif
