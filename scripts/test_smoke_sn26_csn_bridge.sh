#!/usr/bin/env bash
# test_smoke_sn26_csn_bridge.sh — smoke test for SN-26-csn-bridge-a.
#
# Validates that csnobol4's monitor_ipc_runtime.c is correctly linked into
# xsnobol4 AND that the C entry points emit valid wire records when the
# MONITOR_READY_PIPE / MONITOR_GO_PIPE / MONITOR_NAMES_OUT env vars are set.
#
# This script does NOT require v311.sil to be patched (that's SN-26-csn-bridge-b).
# It builds a tiny standalone harness that links monitor_ipc_runtime.c with a
# C main that exercises the three public entry points directly, then reads
# the wire from a Python controller.
#
# Per RULES.md self-contained scripts: paths derived from $0; corpus path
# hardcoded; oracle paths hardcoded; SKIP cleanly if dependencies missing.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CSNOBOL4_DIR="${CSNOBOL4_DIR:-/home/claude/csnobol4}"
MONITOR_DIR="${MONITOR_DIR:-$HERE/monitor}"

EXPECTED_RECORDS=6   # 3×VALUE + 1×CALL + 1×RETURN + 1×END

# --- preflight -----------------------------------------------------------
if [ ! -f "$CSNOBOL4_DIR/monitor_ipc_runtime.c" ]; then
    echo "SKIP $CSNOBOL4_DIR/monitor_ipc_runtime.c missing — clone csnobol4 with SN-26-csn-bridge-a"
    exit 0
fi
if [ ! -f "$CSNOBOL4_DIR/test_monitor_ipc_runtime.c" ]; then
    echo "SKIP $CSNOBOL4_DIR/test_monitor_ipc_runtime.c missing"
    exit 0
fi
if [ ! -x "$MONITOR_DIR/read_one_wire.py" ]; then
    echo "SKIP $MONITOR_DIR/read_one_wire.py missing"
    exit 0
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# --- build standalone harness -------------------------------------------
HARNESS="$WORK/mir_smoke"
gcc -Wall -O2 -o "$HARNESS" \
    "$CSNOBOL4_DIR/test_monitor_ipc_runtime.c" \
    "$CSNOBOL4_DIR/monitor_ipc_runtime.c"
if [ ! -x "$HARNESS" ]; then
    echo "FAIL standalone harness build"
    exit 1
fi

# --- end-to-end wire test -----------------------------------------------
RFIFO="$WORK/r.fifo"
GFIFO="$WORK/g.fifo"
NAMES="$WORK/names.out"
CTRL_LOG="$WORK/ctrl.log"

# Launch controller (creates FIFOs, opens read end of READY which blocks)
( python3 "$MONITOR_DIR/read_one_wire.py" "$RFIFO" "$GFIFO" "$NAMES" \
        > "$CTRL_LOG" 2>&1 ) &
CTRL_PID=$!
# Wait for FIFOs to be created
for _ in 1 2 3 4 5 6 7 8 9 10; do
    [ -p "$RFIFO" ] && [ -p "$GFIFO" ] && break
    sleep 0.1
done

# Run participant with monitoring on
MONITOR_READY_PIPE="$RFIFO" MONITOR_GO_PIPE="$GFIFO" MONITOR_NAMES_OUT="$NAMES" \
    timeout 8 "$HARNESS" > /dev/null 2>&1
PART_RC=$?

wait "$CTRL_PID"
CTRL_RC=$?

if [ "$PART_RC" -ne 0 ] || [ "$CTRL_RC" -ne 0 ]; then
    echo "FAIL participant rc=$PART_RC controller rc=$CTRL_RC"
    cat "$CTRL_LOG"
    exit 1
fi

N_REC=$(grep -c '^\[ctrl\] #' "$CTRL_LOG" || true)
if [ "$N_REC" -ne "$EXPECTED_RECORDS" ]; then
    echo "FAIL expected $EXPECTED_RECORDS records, got $N_REC"
    cat "$CTRL_LOG"
    exit 1
fi

# Verify each expected record appears
for kind in "kind=VALUE.*STRING(5)=b'hello'" \
            "kind=VALUE.*INTEGER(42)" \
            "kind=VALUE.*REAL(3.14)" \
            "kind=CALL.*name_id=3" \
            "kind=RETURN.*INTEGER(49)" \
            "kind=END"; do
    if ! grep -q "$kind" "$CTRL_LOG"; then
        echo "FAIL missing record: $kind"
        cat "$CTRL_LOG"
        exit 1
    fi
done

# Verify names sidecar
if [ ! -f "$NAMES" ]; then
    echo "FAIL names sidecar not written"
    exit 1
fi
N_NAMES=$(wc -l < "$NAMES")
if [ "$N_NAMES" -ne 4 ]; then
    echo "FAIL expected 4 names in sidecar, got $N_NAMES"
    cat "$NAMES"
    exit 1
fi

# --- no-op fallback test (env vars unset) -------------------------------
unset MONITOR_READY_PIPE MONITOR_GO_PIPE MONITOR_NAMES_OUT
if ! timeout 4 "$HARNESS" > /dev/null 2>&1; then
    echo "FAIL silent no-op fallback (env vars unset) returned non-zero"
    exit 1
fi

echo "PASS  $N_REC records, $N_NAMES names, no-op fallback OK"
echo "PASS=1 FAIL=0"
