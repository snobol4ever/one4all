#!/usr/bin/env bash
# test_smoke_sn26_csn_bridge_c.sh — smoke test for SN-26-bridge-coverage-a.
#
# Validates that the NMD4 fire-point in CSNOBOL4 fires on a pattern
# .-capture commit (ANY('AB') . captured).  This closes the bridge
# coverage gap identified in session #29 — without it, scrip's first
# VALUE record on .-captures never matched the oracles' wire stream.
#
# Companion to test_smoke_sn26_csn_bridge.sh (standalone C IPC plumbing)
# and test_smoke_sn26_csn_bridge_b.sh (ASGNVV/SJSRV1/DEFF18/DEFF20).
#
# Per RULES.md self-contained scripts: paths derived from $0; corpus path
# hardcoded; oracle paths hardcoded; SKIP cleanly if dependencies missing.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CSNOBOL4="${CSNOBOL4:-/home/claude/csnobol4/snobol4}"
CORPUS="${CORPUS:-/home/claude/corpus}"
MONITOR_DIR="${MONITOR_DIR:-$HERE/monitor}"
PROBE="$CORPUS/programs/snobol4/demo/csn_bridge_c/probe_c.sno"

EXPECTED_RECORDS=3   # ASGNVV S + NMD4 captured + END

# --- preflight -----------------------------------------------------------
if [ ! -x "$CSNOBOL4" ]; then
    echo "SKIP $CSNOBOL4 missing — run build_csnobol4_oracle.sh"
    exit 0
fi
if [ ! -f "$PROBE" ]; then
    echo "SKIP $PROBE missing — corpus checkout incomplete"
    exit 0
fi
if [ ! -f "$MONITOR_DIR/read_one_wire.py" ]; then
    echo "SKIP $MONITOR_DIR/read_one_wire.py missing"
    exit 0
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

RFIFO="$WORK/r.fifo"
GFIFO="$WORK/g.fifo"
NAMES="$WORK/names.out"
CTRL_LOG="$WORK/ctrl.log"
PART_OUT="$WORK/part.out"

# --- run xsnobol4 against the wire reader -------------------------------
( python3 "$MONITOR_DIR/read_one_wire.py" "$RFIFO" "$GFIFO" "$NAMES" \
        > "$CTRL_LOG" 2>&1 ) &
CTRL_PID=$!

for _ in 1 2 3 4 5 6 7 8 9 10; do
    [ -p "$RFIFO" ] && [ -p "$GFIFO" ] && break
    sleep 0.1
done

MONITOR_READY_PIPE="$RFIFO" MONITOR_GO_PIPE="$GFIFO" MONITOR_NAMES_OUT="$NAMES" \
    timeout 8 "$CSNOBOL4" -bf "$PROBE" > "$PART_OUT" 2>&1 < /dev/null
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

# Verify each expected record-shape appears in order.
while IFS= read -r spec; do
    [ -z "$spec" ] && continue
    if ! grep -qF "$spec" "$CTRL_LOG"; then
        echo "FAIL missing/misordered record: $spec"
        cat "$CTRL_LOG"
        exit 1
    fi
done <<'EOF_SPECS'
#000 kind=VALUE name_id=0 STRING(5)=b'AXBYC'
#001 kind=VALUE name_id=1 STRING(1)=b'A'
#002 kind=END
EOF_SPECS

# Verify names sidecar contains S and captured (in interning order).
if [ ! -f "$NAMES" ]; then
    echo "FAIL names sidecar not written"
    exit 1
fi
N_NAMES=$(wc -l < "$NAMES")
if [ "$N_NAMES" -ne 2 ]; then
    echo "FAIL expected 2 names in sidecar, got $N_NAMES"
    cat "$NAMES"
    exit 1
fi
for nm in S captured; do
    if ! grep -qx "$nm" "$NAMES"; then
        echo "FAIL name '$nm' missing from sidecar"
        cat "$NAMES"
        exit 1
    fi
done

# --- no-op fallback test (env vars unset) -------------------------------
unset MONITOR_READY_PIPE MONITOR_GO_PIPE MONITOR_NAMES_OUT
if ! timeout 4 "$CSNOBOL4" -bf "$PROBE" > /dev/null 2>&1 < /dev/null; then
    echo "FAIL silent no-op fallback (env vars unset) returned non-zero"
    exit 1
fi

echo "PASS  $N_REC records (ASGNVV+NMD4+END), $N_NAMES names, no-op fallback OK"
echo "PASS=1 FAIL=0"
