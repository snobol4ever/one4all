#!/usr/bin/env bash
# test_sn4_js_ladder.sh — SNOBOL4 → JS ladder test driver
# Runs all .sno files with .ref files in csnobol4-suite and snobol4/demo
# Compares output to .ref
# Usage: bash scripts/test_sn4_js_ladder.sh [--verbose]
# Exit: 0 if no regression from FLOOR, 1 otherwise
# Gate: PASS >= FLOOR (set below)

set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/scrip}"
CORPUS="/home/claude/corpus"
RT="$HERE/../src/runtime/js/sno_runtime.js"
TIMEOUT=10
VERBOSE="${1:-}"
FLOOR=70

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; RESET='\033[0m'
PASS=0; FAIL=0; SKIP=0
TMPD=$(mktemp -d); trap "rm -rf $TMPD" EXIT

if [[ ! -x "$SCRIP" ]]; then echo "ERROR: scrip not found: $SCRIP"; exit 1; fi
if [[ ! -f "$RT" ]]; then echo "ERROR: sno_runtime.js not found: $RT"; exit 1; fi
if [[ ! -d "$CORPUS" ]]; then echo "SKIP: corpus not found"; exit 0; fi

run_one() {
    local sno="$1"
    local base; base=$(basename "$sno" .sno)
    local dir;  dir=$(dirname "$sno")
    local ref="$dir/$base.ref"
    [[ -f "$ref" ]] || return 0
    local js="$TMPD/$base.js"
    local out="$TMPD/$base.out"
    local err="$TMPD/$base.err"
    local inp="$dir/$base.input"
    # Emit
    if ! timeout "$TIMEOUT" "$SCRIP" --target=js "$sno" > "$js" 2>"$err"; then
        [[ "$VERBOSE" == "--verbose" ]] && echo -e "${RED}FAIL${RESET} $base [emit error]"
        FAIL=$((FAIL+1)); return 0
    fi
    # Run
    local stdin_arg="< /dev/null"
    [[ -f "$inp" ]] && stdin_arg="< \"$inp\""
    if ! eval "timeout $TIMEOUT node \"$js\" $stdin_arg > \"$out\" 2>\"$err\""; then
        [[ "$VERBOSE" == "--verbose" ]] && { echo -e "${RED}FAIL${RESET} $base [node error]"; head -3 "$err"; }
        FAIL=$((FAIL+1)); return 0
    fi
    # Diff
    if diff -q "$out" "$ref" > /dev/null 2>&1; then
        [[ "$VERBOSE" == "--verbose" ]] && echo -e "${GREEN}PASS${RESET} $base"
        PASS=$((PASS+1))
    else
        [[ "$VERBOSE" == "--verbose" ]] && { echo -e "${RED}FAIL${RESET} $base [output mismatch]"; diff "$out" "$ref" | head -6; }
        FAIL=$((FAIL+1))
    fi
}

echo "=== SNOBOL4 → JS ladder ==="
for sno in "$CORPUS/programs/csnobol4-suite/"*.sno; do run_one "$sno"; done
for sno in "$CORPUS/programs/snobol4/demo/"*.sno; do run_one "$sno"; done
for sno in "$CORPUS/programs/snobol4/feat/"*.sno; do run_one "$sno"; done

TOTAL=$((PASS+FAIL))
echo "PASS=$PASS FAIL=$FAIL TOTAL=$TOTAL"
if [[ $PASS -ge $FLOOR ]]; then
    echo "Gate: PASS ✅ ($PASS >= $FLOOR)"
    exit 0
else
    echo "Gate: FAIL ❌ ($PASS < $FLOOR)"
    exit 1
fi
