/*============================================================= sync_monitor.c
 * In-process sync monitor — IM-2
 *
 * exec_snapshot_take()    — capture all mutable executor state
 * exec_snapshot_restore() — reset world to captured state
 * exec_snapshot_free()    — release heap storage in snapshot
 *==========================================================================*/
#include <stdlib.h>
#include <string.h>

#include "sync_monitor.h"
#include "runtime/x86/snobol4.h"
#include "runtime/interp/icn_runtime.h"
#include "runtime/interp/pl_runtime.h"

/*------------------------------------------------------------------------
 * exec_snapshot_take — capture current mutable state
 *----------------------------------------------------------------------*/
void exec_snapshot_take(ExecSnapshot *s) {
    if (!s) return;

    /* NV store */
    s->nv_count = nv_snapshot(&s->nv_pairs);

    /* Keyword globals */
    s->kw_stcount      = kw_stcount;
    s->kw_stlimit      = kw_stlimit;
    s->kw_anchor       = kw_anchor;

    /* ICN frame depth (locals captured in IM-10) */
    s->icn_frame_depth = icn_frame_depth;

    /* Prolog trail mark */
    s->pl_trail_mark   = trail_mark(&g_pl_trail);
}

/*------------------------------------------------------------------------
 * exec_snapshot_restore — reset world to captured state
 *
 * Order matters:
 *   1. NV store reset + replay (nv_restore calls nv_reset internally)
 *   2. Keyword globals
 *   3. ICN frame stack unwind to depth 0
 *   4. Prolog trail unwind to captured mark
 *----------------------------------------------------------------------*/
void exec_snapshot_restore(const ExecSnapshot *s) {
    if (!s) return;

    /* 1. NV store */
    nv_restore(s->nv_pairs, s->nv_count);

    /* 2. Keywords */
    kw_stcount = s->kw_stcount;
    kw_stlimit = s->kw_stlimit;
    kw_anchor  = s->kw_anchor;

    /* 3. ICN frame stack — unwind to depth 0 */
    icn_frame_depth = 0;

    /* 4. Prolog trail — unwind to mark */
    trail_unwind(&g_pl_trail, s->pl_trail_mark);
}

/*------------------------------------------------------------------------
 * exec_snapshot_free — release heap storage
 *----------------------------------------------------------------------*/
void exec_snapshot_free(ExecSnapshot *s) {
    if (!s) return;
    free(s->nv_pairs);
    s->nv_pairs = NULL;
    s->nv_count = 0;
}
