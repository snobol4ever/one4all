#ifndef SYNC_MONITOR_H
#define SYNC_MONITOR_H
#include "snobol4.h"
#include "runtime/interp/icn_runtime.h"
#include "runtime/interp/pl_runtime.h"
typedef struct {
    NvPair  *nv_pairs;
    int      nv_count;
    int64_t  kw_stcount;
    int64_t  kw_stlimit;
    int64_t  kw_anchor;
    int      frame_depth;
    int      pl_trail_mark;
    int      last_ok;
    const char **label_path;
    int          label_path_n;
    int          label_path_cap;
    NvPair  *frame_locals;
    int      frame_locals_count;
    struct PlLocalPair {
        char *name;
        char *val_str;
    } *pl_locals;
    int      pl_locals_count;
} ExecSnapshot;
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void exec_snapshot_take(ExecSnapshot *s);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void exec_snapshot_restore(const ExecSnapshot *s);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void exec_snapshot_free(ExecSnapshot *s);
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int sync_monitor_run(const tree_t *prog, int verbose, const char *sno_path);
#endif
