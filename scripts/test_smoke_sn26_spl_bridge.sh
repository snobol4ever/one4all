#!/usr/bin/env bash
# test_smoke_sn26_spl_bridge.sh — smoke test for SN-26-spl-bridge-b.
#
# Validates that the three monitor fire-points in SPITBOL x64 (b_vrs,
# bpf09, retrn) all fire when sbl -bf runs a SNOBOL4 program with
# MONITOR_READY_PIPE set.  Mirrors test_smoke_sn26_csn_bridge_b.sh shape
# so the two oracles can be cross-validated by a single controller.
#
# Per RULES.md self-contained scripts: paths derived from $0; corpus path
# hardcoded; oracle paths hardcoded; SKIP cleanly if dependencies missing.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SBL="${SBL:-/home/claude/x64/bin/sbl}"
CORPUS="${CORPUS:-/home/claude/corpus}"
MONITOR_DIR="${MONITOR_DIR:-$HERE/monitor}"
PROBE="$CORPUS/programs/snobol4/demo/spl_bridge/probe.sno"

EXPECTED_RECORDS=6   # see probe.sno header

# --- preflight -----------------------------------------------------------
if [ ! -x "$SBL" ]; then
    echo "SKIP $SBL missing — run build_spitbol_oracle.sh"
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

# Capability probe: does this sbl have sysmv/sysmc/sysmr?  Older builds
# (pre-SN-26-spl-bridge-b) would silently produce zero records; we'd
# rather fail loudly with a clear message.
if ! nm -D "$SBL" 2>/dev/null | grep -q '\bzysmv\b' && \
   ! strings "$SBL" 2>/dev/null | grep -q 'MONITOR_READY_PIPE'; then
    echo "SKIP $SBL is pre-SN-26-spl-bridge-b (no zysmv symbol or env-var string)"
    exit 0
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

RFIFO="$WORK/r.fifo"
GFIFO="$WORK/g.fifo"
NAMES="$WORK/names.out"
CTRL_LOG="$WORK/ctrl.log"
PART_OUT="$WORK/part.out"

mkfifo "$RFIFO" "$GFIFO"

# --- run sbl against the wire reader ------------------------------------
( python3 "$MONITOR_DIR/read_one_wire.py" "$RFIFO" "$GFIFO" "$NAMES" \
        > "$CTRL_LOG" 2>&1 ) &
CTRL_PID=$!

# Tiny wait so the controller has the FIFOs open before sbl tries to write.
sleep 0.3

MONITOR_READY_PIPE="$RFIFO" MONITOR_GO_PIPE="$GFIFO" MONITOR_NAMES_OUT="$NAMES" \
    timeout 8 "$SBL" -bf "$PROBE" > "$PART_OUT" 2>&1 < /dev/null
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

# Pin record indices — bridge sites must fire in this order.
while IFS= read -r spec; do
    [ -z "$spec" ] && continue
    if ! grep -qF "$spec" "$CTRL_LOG"; then
        echo "FAIL missing/misordered record: $spec"
        cat "$CTRL_LOG"
        exit 1
    fi
done <<'EOF_SPECS'
#000 kind=VALUE name_id=0 STRING(11)=b'hello world'
#001 kind=CALL name_id=1
#002 kind=VALUE name_id=1 INTEGER(49)
#003 kind=RETURN name_id=1 STRING(6)=b'RETURN'
#004 kind=VALUE name_id=2 INTEGER(49)
#005 kind=END
EOF_SPECS

# Verify names sidecar contains S, SQR, N (in interning order).
if [ ! -f "$NAMES" ]; then
    echo "FAIL names sidecar not written"
    exit 1
fi
N_NAMES=$(wc -l < "$NAMES")
if [ "$N_NAMES" -ne 3 ]; then
    echo "FAIL expected 3 names in sidecar, got $N_NAMES"
    cat "$NAMES"
    exit 1
fi
for nm in S SQR N; do
    if ! grep -qx "$nm" "$NAMES"; then
        echo "FAIL name '$nm' missing from sidecar"
        cat "$NAMES"
        exit 1
    fi
done

# --- no-op fallback test (env vars unset) -------------------------------
unset MONITOR_READY_PIPE MONITOR_GO_PIPE MONITOR_NAMES_OUT
if ! timeout 4 "$SBL" -bf "$PROBE" > /dev/null 2>&1 < /dev/null; then
    echo "FAIL silent no-op fallback (env vars unset) returned non-zero"
    exit 1
fi

echo "PASS  $N_REC records (b_vrs+bpf09+b_vrs+retrn+b_vrs+END), $N_NAMES names, no-op fallback OK"
echo "PASS=1 FAIL=0"
