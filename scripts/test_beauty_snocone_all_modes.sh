#!/usr/bin/env bash
# test_beauty_snocone_all_modes.sh -- SC-19/SC-20/SC-21/SC-22
# Run all 14 beauty-sc subsystems under --ir-run, --sm-run, --jit-run
# Gate: 14 PASS + 1 SKIP (beauty, no driver.sc) per mode
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
BEAUTY_DIR="${CORPUS:-/home/claude/corpus}/programs/snocone/demo/beauty/test"
TIMEOUT="${TIMEOUT:-10}"
GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'

SUBSYSTEMS=(arith assign fence global match roman semantic ShiftReduce ReadWrite counter stack strings trace beauty tree)
MODES=(--ir-run --sm-run --jit-run)

PASS=0; FAIL=0; SKIP=0

run_one() {
    local subsys="$1" mode="$2"
    local sc="$BEAUTY_DIR/$subsys/driver.sc"
    local ref="$BEAUTY_DIR/$subsys/driver.ref"
    if [[ ! -f "$sc" ]]; then
        echo -e "${YELLOW}SKIP${RESET}  $subsys $mode (no driver.sc)"; SKIP=$((SKIP+1)); return
    fi
    if [[ ! -f "$ref" ]]; then
        echo -e "${YELLOW}SKIP${RESET}  $subsys $mode (no driver.ref)"; SKIP=$((SKIP+1)); return
    fi
    local got; got=$(timeout "$TIMEOUT" "$SCRIP" "$mode" "$sc" < /dev/null 2>/dev/null) || true
    local exp; exp=$(cat "$ref")
    if [[ "$got" == "$exp" ]]; then
        echo -e "${GREEN}PASS${RESET}  $subsys $mode"; PASS=$((PASS+1))
    else
        echo -e "${RED}FAIL${RESET}  $subsys $mode"
        diff <(echo "$exp") <(echo "$got") | head -10
        FAIL=$((FAIL+1))
    fi
}

echo "=== beauty-sc all modes ==="
for mode in "${MODES[@]}"; do
    echo "--- $mode ---"
    for subsys in "${SUBSYSTEMS[@]}"; do
        run_one "$subsys" "$mode"
    done
done

echo ""
echo "PASS=$PASS FAIL=$FAIL SKIP=$SKIP"
[[ $FAIL -eq 0 ]]
