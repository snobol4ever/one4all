/*============================================================= csnobol4_shim.c
 * csnobol4_shim.c — CSNOBOL4 in-process executor shim (IM-15b).
 *
 * Stub implementation: csnobol4_run_steps() and csn_nv_snapshot_free()
 * are no-ops until the CSNOBOL4 archive is built and the hook is wired
 * into snobol4.c. The build links cleanly; the CSN leg is simply disabled.
 *
 * Real implementation (IM-15b):
 *   - build_csnobol4_archive.sh: compile CSNOBOL4 with -Dmain=csnobol4_main
 *     -fPIC, produce csnobol4/libcsnobol4.a
 *   - add per-statement hook in CSNOBOL4 snobol4.c main dispatch loop
 *   - csnobol4_run_steps(): write src to tmpfile, arm hook, call
 *     csnobol4_main(), longjmp out at step N, snapshot variable table
 *===========================================================================*/

#include <stdlib.h>
#include <string.h>
#include "sync_monitor.h"   /* CsnNvPair */

/* Stub: CSNOBOL4 not yet linked in-process. Returns -1 (no snapshot). */
int
csnobol4_run_steps(const char *sno_path, int step_limit,
                   CsnNvPair **out_pairs, int *out_count)
{
    (void)sno_path; (void)step_limit;
    *out_pairs = NULL;
    *out_count = 0;
    return -1;
}

/* Free a CsnNvPair array (no-op for NULL). */
void
csn_nv_snapshot_free(CsnNvPair *pairs, int n)
{
    if (!pairs) return;
    for (int i = 0; i < n; i++) {
        free(pairs[i].name);
        free(pairs[i].val_str);
    }
    free(pairs);
}
