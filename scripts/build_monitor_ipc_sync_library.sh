#!/usr/bin/env bash
# build_monitor_ipc_sync_library.sh — build monitor_ipc_sync.so for the
# 2-way sync-step monitor harness (CSNOBOL4 + SPITBOL).
#
# Idempotent.  Source: scripts/monitor/monitor_ipc_sync.c (committed).
# Output:  scripts/monitor/monitor_ipc_sync.so  (gitignored).
#
# Used by: scripts/test_monitor_2way_sync_step.sh
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$HERE/monitor/monitor_ipc_sync.c"
OUT="$HERE/monitor/monitor_ipc_sync.so"
[[ -f "$SRC" ]] || { echo "FAIL source missing: $SRC"; exit 1; }
if [[ -f "$OUT" && "$OUT" -nt "$SRC" ]]; then
    echo "SKIP $OUT up to date"
    exit 0
fi
gcc -shared -fPIC -O2 -Wall -o "$OUT" "$SRC"
echo "OK  $OUT"
