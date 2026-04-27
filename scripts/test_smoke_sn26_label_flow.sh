#!/usr/bin/env bash
# test_smoke_sn26_label_flow.sh — smoke test for SN-26-bridge-coverage-f.
#
# Validates that all three runtimes (CSNOBOL4, SPITBOL x64, scrip) emit
# MWK_LABEL records on every statement entry.  Each runtime is checked
# independently — this is a coverage gate, not a cross-runtime
# comparison.  Cross-runtime symmetry is checked by the 3-way harness
# (test_monitor_3way_sync_step_auto.sh) at sub-rung -h.
#
# Per RULES.md: paths derived from $0; corpus path hardcoded; oracle
# paths hardcoded; SKIP cleanly if dependencies missing.
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="${ONE4ALL:-$(cd "$HERE/.." && pwd)}"
CORPUS="${CORPUS:-/home/claude/corpus}"
SBL="${SBL:-/home/claude/x64/bin/sbl}"
CSN="${CSN:-/home/claude/csnobol4/snobol4}"
SCRIP="${SCRIP:-$ONE4ALL/scrip}"
MONITOR_DIR="${MONITOR_DIR:-$HERE/monitor}"
PROBE="$CORPUS/programs/snobol4/demo/label_flow/probe_label.sno"

# --- preflight -----------------------------------------------------------
if [ ! -f "$PROBE" ]; then
    echo "SKIP $PROBE missing — corpus checkout incomplete"
    exit 0
fi
if [ ! -x "$MONITOR_DIR/read_one_wire.py" ]; then
    echo "SKIP $MONITOR_DIR/read_one_wire.py missing"
    exit 0
fi

run_one() {
    local label="$1"
    local cmd_args="$2"   # full command with args, prefixed by env
    local expected_labels="$3"   # number of LABEL records expected

    local WORK
    WORK=$(mktemp -d)
    local RFIFO="$WORK/r.fifo"
    local GFIFO="$WORK/g.fifo"
    local NAMES="$WORK/names.out"
    local CTRL_LOG="$WORK/ctrl.log"
    local PART_OUT="$WORK/part.out"

    ( python3 "$MONITOR_DIR/read_one_wire.py" "$RFIFO" "$GFIFO" "$NAMES" \
            > "$CTRL_LOG" 2>&1 ) &
    local CTRL_PID=$!
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        [ -p "$RFIFO" ] && [ -p "$GFIFO" ] && break
        sleep 0.1
    done

    eval MONITOR_READY_PIPE='"$RFIFO"' MONITOR_GO_PIPE='"$GFIFO"' \
         MONITOR_NAMES_OUT='"$NAMES"' \
         "$cmd_args" \"$PROBE\" '> "$PART_OUT" 2>&1 < /dev/null'
    wait "$CTRL_PID" 2>/dev/null || true

    local n_label
    n_label=$(grep -c "kind=LABEL" "$CTRL_LOG" 2>/dev/null || true)
    if [ "$n_label" -ne "$expected_labels" ]; then
        echo "FAIL $label expected $expected_labels LABEL records, got $n_label"
        cat "$CTRL_LOG"
        rm -rf "$WORK"
        return 1
    fi
    echo "OK   $label: $n_label LABEL records"
    rm -rf "$WORK"
    return 0
}

PASS=0
FAIL=0

# CSNOBOL4: 3 user stmts -> 3 LABELs.
if [ -x "$CSN" ]; then
    if run_one "csn -bf"             "timeout 8 \"$CSN\" -bf"          3; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
else
    echo "SKIP csn missing"
fi

# SPITBOL: counts END as a stmt -> 4 LABELs.
if [ -x "$SBL" ]; then
    if run_one "sbl -bf"             "timeout 8 \"$SBL\" -bf"          4; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
else
    echo "SKIP sbl missing"
fi

# scrip --ir-run: matches CSN -> 3 LABELs.
if [ -x "$SCRIP" ]; then
    if run_one "scrip --ir-run"      "timeout 8 env MONITOR_BIN=1 \"$SCRIP\" --ir-run"   3; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
    # scrip --sm-run: matches SPL on blank-line counting (post -k: 1 blank + 3 stmts + END = 5 LABELs).
    if run_one "scrip --sm-run"      "timeout 8 env MONITOR_BIN=1 \"$SCRIP\" --sm-run"   5; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
    # scrip --jit-run: same as sm-run (post -k).
    if run_one "scrip --jit-run"     "timeout 8 env MONITOR_BIN=1 \"$SCRIP\" --jit-run"  5; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
else
    echo "SKIP scrip missing"
fi

if [ "$FAIL" -ne 0 ]; then
    echo "PASS=$PASS FAIL=$FAIL"
    exit 1
fi
echo "PASS=$PASS FAIL=$FAIL"
