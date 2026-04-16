#!/usr/bin/env bash
# test_raku_ir_full_suite.sh — Full Raku rung ladder sweep (RK-27)
#
# Runs all RK-1 through RK-26 tests under --ir-run, --sm-run, and --jit-run.
# Reports per-rung PASS/FAIL for each mode. Final gate: FAIL=0 in all modes.
#
# Gate: PASS=22 FAIL=0 per mode, all three modes.
#
# AUTHORS: LCherryholmes · Claude Sonnet 4.6   DATE: 2026-04-15

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
SCRIP="${SCRIP:-$REPO/scrip}"
TESTDIR="$REPO/test/raku"

if [ ! -x "$SCRIP" ]; then
    echo "SKIP  scrip binary not found at $SCRIP" >&2
    exit 0
fi
if [ ! -d "$TESTDIR" ]; then
    echo "SKIP  test/raku dir not found at $TESTDIR" >&2
    exit 0
fi

IR_PASS=0; IR_FAIL=0
SM_PASS=0; SM_FAIL=0
JIT_PASS=0; JIT_FAIL=0
TOTAL_FAIL=0

run_mode() {
    local mode="$1" raku="$2"
    local exp="${raku%.raku}.expected"
    [ -f "$exp" ] || { echo "  SKIP $(basename "$raku" .raku) (no .expected)"; return 1; }
    local got want
    got=$(timeout 8 "$SCRIP" "$mode" "$raku" </dev/null 2>/dev/null) || true
    want=$(cat "$exp")
    if [ "$got" = "$want" ]; then
        echo "  PASS $(basename "$raku" .raku)"
        return 0
    else
        echo "  FAIL $(basename "$raku" .raku)"
        echo "    want: $(echo "$want" | head -3 | tr '\n' '|')"
        echo "    got:  $(echo "$got"  | head -3 | tr '\n' '|')"
        return 1
    fi
}

echo "=== Raku full suite — --ir-run ==="
for raku in "$TESTDIR"/*.raku; do
    [ -f "$raku" ] || continue
    if run_mode --ir-run "$raku"; then
        IR_PASS=$((IR_PASS+1))
    else
        IR_FAIL=$((IR_FAIL+1))
    fi
done
echo ""
echo "  ir-run:  PASS=$IR_PASS FAIL=$IR_FAIL"

echo ""
echo "=== Raku full suite — --sm-run ==="
for raku in "$TESTDIR"/*.raku; do
    [ -f "$raku" ] || continue
    if run_mode --sm-run "$raku"; then
        SM_PASS=$((SM_PASS+1))
    else
        SM_FAIL=$((SM_FAIL+1))
    fi
done
echo ""
echo "  sm-run:  PASS=$SM_PASS FAIL=$SM_FAIL"

echo ""
echo "=== Raku full suite — --jit-run ==="
for raku in "$TESTDIR"/*.raku; do
    [ -f "$raku" ] || continue
    if run_mode --jit-run "$raku"; then
        JIT_PASS=$((JIT_PASS+1))
    else
        JIT_FAIL=$((JIT_FAIL+1))
    fi
done
echo ""
echo "  jit-run: PASS=$JIT_PASS FAIL=$JIT_FAIL"

echo ""
echo "=== Summary ==="
echo "  ir-run:  PASS=$IR_PASS FAIL=$IR_FAIL"
echo "  sm-run:  PASS=$SM_PASS FAIL=$SM_FAIL"
echo "  jit-run: PASS=$JIT_PASS FAIL=$JIT_FAIL"

TOTAL_FAIL=$((IR_FAIL + SM_FAIL + JIT_FAIL))
if [ "$TOTAL_FAIL" -eq 0 ]; then
    echo ""
    echo "ALL PASS — PASS=$IR_PASS per mode, all three modes."
    exit 0
else
    echo ""
    echo "FAIL — $TOTAL_FAIL failures across all modes."
    exit 1
fi
