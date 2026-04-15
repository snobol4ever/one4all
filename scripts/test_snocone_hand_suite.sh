#!/usr/bin/env bash
# test_snocone_hand_suite.sh -- SC-18: run hand-crafted Snocone tests (SC-13..SC-17)
# Gate: PASS=5 FAIL=0 under all three modes
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
TESTDIR="$HERE/../test/snocone"
TIMEOUT="${TIMEOUT:-8}"
GREEN='\033[0;32m'; RED='\033[0;31m'; RESET='\033[0m'

TESTS=(fibonacci palindrome wordcount quicksort pattern_suite)
MODES=(--ir-run --sm-run --jit-run)

PASS=0; FAIL=0

run_test() {
    local name="$1" mode="$2"
    local sc="$TESTDIR/${name}.sc"
    local ref="$TESTDIR/${name}.ref"
    if [[ ! -f "$sc" ]]; then echo -e "${RED}FAIL${RESET}  $name $mode (no .sc)"; FAIL=$((FAIL+1)); return; fi
    if [[ ! -f "$ref" ]]; then echo -e "${RED}FAIL${RESET}  $name $mode (no .ref)"; FAIL=$((FAIL+1)); return; fi
    local got; got=$(timeout "$TIMEOUT" "$SCRIP" "$mode" "$sc" < /dev/null 2>/dev/null) || true
    local exp; exp=$(cat "$ref")
    if [[ "$got" == "$exp" ]]; then
        echo -e "${GREEN}PASS${RESET}  $name $mode"; PASS=$((PASS+1))
    else
        echo -e "${RED}FAIL${RESET}  $name $mode"
        diff <(echo "$exp") <(echo "$got") | head -10
        FAIL=$((FAIL+1))
    fi
}

echo "=== Snocone hand suite ==="
for name in "${TESTS[@]}"; do
    for mode in "${MODES[@]}"; do
        run_test "$name" "$mode"
    done
done

echo ""
echo "PASS=$PASS FAIL=$FAIL"
[[ $FAIL -eq 0 ]]
