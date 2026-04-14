/*============================================================= sync_monitor.h
 * In-process sync monitor — IM-2
 *
 * ExecSnapshot captures all mutable global state that the three executors
 * (IR, SM, JIT) share. exec_snapshot_take() captures; exec_snapshot_restore()
 * resets the world to that state so the next executor starts clean.
 *
 * In production: nothing here is called — zero overhead.
 *==========================================================================*/
#ifndef SYNC_MONITOR_H
#define SYNC_MONITOR_H

#include "runtime/x86/snobol4.h"          /* NvPair, DESCR_t */
#include "runtime/interp/icn_runtime.h"   /* IcnFrame, icn_frame_depth */
#include "runtime/interp/pl_runtime.h"    /* Trail, trail_mark */

/*------------------------------------------------------------------------
 * ExecSnapshot — flat capture of all mutable inter-executor state
 *----------------------------------------------------------------------*/
typedef struct {
    /* NV store snapshot */
    NvPair  *nv_pairs;
    int      nv_count;

    /* Keyword globals */
    int64_t  kw_stcount;
    int64_t  kw_stlimit;
    int64_t  kw_anchor;

    /* ICN frame stack depth (frame locals snapshotted in IM-10) */
    int      icn_frame_depth;

    /* Prolog trail mark — restore by unwinding to this position */
    int      pl_trail_mark;
} ExecSnapshot;

/*------------------------------------------------------------------------
 * Public interface
 *----------------------------------------------------------------------*/
void exec_snapshot_take(ExecSnapshot *s);
void exec_snapshot_restore(const ExecSnapshot *s);
void exec_snapshot_free(ExecSnapshot *s);

#endif /* SYNC_MONITOR_H */
