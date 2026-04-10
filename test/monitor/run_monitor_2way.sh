#!/bin/bash
# run_monitor_2way.sh <sno_file> [tracepoints_conf]
# Two-way sync-step monitor: SPITBOL x64 (oracle) vs scrip --ir-run.
# No compilation step. No JVM/NET/ASM participants.
# Exit 0 = agree. Exit 1 = diverge. Exit 2 = timeout.

set -uo pipefail
SNO=${1:?usage: run_monitor_2way.sh <sno_file> [tracepoints_conf]}
CONF=${2:-$(dirname "$0")/tracepoints.conf}
MDIR=$(cd "$(dirname "$0")" && pwd)
DIR=$(cd "$MDIR/../.." && pwd)
X64="${X64:-/home/claude/x64}"
INC="${INC:-/home/claude/snobol4ever_corpus/programs/snobol4/demo/inc}"
TIMEOUT="${MONITOR_TIMEOUT:-10}"
TMP=$(mktemp -d /tmp/monitor_2way_XXXXXX)
trap 'rm -rf "$TMP"' EXIT

base="$(basename "$SNO" .sno)"
echo "[2way] $base"

# Step 1: inject traces
python3 "$MDIR/inject_traces.py" "$SNO" "$CONF" > "$TMP/instr.sno"

# Step 2: FIFOs — two per participant (ready + go)
for p in spl scrip; do mkfifo "$TMP/$p.ready"; mkfifo "$TMP/$p.go"; done

# Step 3: launch SPITBOL (uses LOAD → monitor_ipc_spitbol.so)
STDIN="${SNO%.sno}.input"; [ -f "$STDIN" ] || STDIN=/dev/null
(cd "$INC" && SNOLIB="$X64:$INC" \
    MONITOR_READY_PIPE="$TMP/spl.ready" MONITOR_GO_PIPE="$TMP/spl.go" \
    MONITOR_SO="$X64/monitor_ipc_spitbol.so" \
    "$X64/bootsbl" "$TMP/instr.sno" < "$STDIN" \
    > "$TMP/spl.out" 2>"$TMP/spl.err") &
SPL_PID=$!

# Step 4: launch scrip --ir-run (uses C-native monitor in snobol4.c)
SNO_LIB="$INC" \
    MONITOR_READY_PIPE="$TMP/scrip.ready" MONITOR_GO_PIPE="$TMP/scrip.go" \
    "$DIR/scrip" --ir-run "$TMP/instr.sno" < "$STDIN" \
    > "$TMP/scrip.out" 2>"$TMP/scrip.err" &
SCRIP_PID=$!

# Step 5: controller
python3 "$MDIR/monitor_sync.py" \
    "$TIMEOUT" "spl,scrip" \
    "$TMP/spl.ready,$TMP/scrip.ready" \
    "$TMP/spl.go,$TMP/scrip.go" > "$TMP/ctrl.out" 2>&1 &
CTRL_PID=$!

wait $CTRL_PID; RC=$?
kill $SPL_PID $SCRIP_PID 2>/dev/null || true
wait 2>/dev/null || true
cat "$TMP/ctrl.out"
[ -s "$TMP/spl.err"   ] && echo "=== spl stderr ===" && cat "$TMP/spl.err"
[ -s "$TMP/scrip.err" ] && echo "=== scrip stderr ===" && cat "$TMP/scrip.err"
exit $RC
