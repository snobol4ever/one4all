#!/bin/bash
# build_monitor.sh — build monitor_ipc.so
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MDIR="$ROOT/test/monitor"
gcc -shared -fPIC -o "$MDIR/monitor_ipc.so" "$MDIR/monitor_ipc.c"
echo "Built: $MDIR/monitor_ipc.so"
