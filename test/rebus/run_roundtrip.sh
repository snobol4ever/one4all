#!/usr/bin/env bash
# test/rebus/run_roundtrip.sh — Rebus round-trip test harness
# For each .reb file: emit → run under CSNOBOL4 → diff against oracle
# Exit 0 = all pass, 1 = any failure

set -euo pipefail

REBUS="$(cd "$(dirname "$0")/../.." && pwd)/src/rebus/rebus"
TESTDIR="$(cd "$(dirname "$0")" && pwd)"
TMPDIR_LOCAL="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_LOCAL"' EXIT

PASS=0
FAIL=0
ERRORS=()

run_test() {
    local name="$1"
    local reb="$TESTDIR/${name}.reb"
    local expected="$TESTDIR/${name}.expected"
    local input_file="$TESTDIR/${name}.input"
    local sno="$TMPDIR_LOCAL/${name}.sno"
    local got="$TMPDIR_LOCAL/${name}.got"

    if [[ ! -f "$reb" ]]; then
        echo "SKIP  $name  (no .reb file)"
        return
    fi
    if [[ ! -f "$expected" ]]; then
        echo "SKIP  $name  (no .expected oracle)"
        return
    fi

    # Emit
    if ! "$REBUS" "$reb" > "$sno" 2>/dev/null; then
        echo "FAIL  $name  (rebus emit failed)"
        FAIL=$((FAIL+1))
        ERRORS+=("$name: emit failed")
        return
    fi

    # Run under CSNOBOL4
    if [[ -f "$input_file" ]]; then
        if ! snobol4 "$sno" < "$input_file" > "$got" 2>/dev/null; then
            echo "FAIL  $name  (snobol4 runtime error)"
            FAIL=$((FAIL+1))
            ERRORS+=("$name: runtime error")
            return
        fi
    else
        if ! snobol4 "$sno" < /dev/null > "$got" 2>/dev/null; then
            echo "FAIL  $name  (snobol4 runtime error)"
            FAIL=$((FAIL+1))
            ERRORS+=("$name: runtime error")
            return
        fi
    fi

    # Diff against oracle
    if diff -q "$expected" "$got" > /dev/null 2>&1; then
        echo "PASS  $name"
        PASS=$((PASS+1))
    else
        echo "FAIL  $name  (output mismatch)"
        diff "$expected" "$got" | head -20
        FAIL=$((FAIL+1))
        ERRORS+=("$name: output mismatch")
    fi
}

echo "=== Rebus round-trip tests ==="
echo ""

run_test word_count
run_test binary_trees

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [[ $FAIL -gt 0 ]]; then
    echo ""
    echo "Failures:"
    for e in "${ERRORS[@]}"; do
        echo "  - $e"
    done
    exit 1
fi

exit 0
