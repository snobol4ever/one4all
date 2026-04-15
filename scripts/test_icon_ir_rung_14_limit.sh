#!/usr/bin/env bash
# test_icon_ir_rung_14_limit.sh — rung14 Icon limit (E \ N) tests (IC-4)
# Gate: PASS=5 FAIL=0
# Authors: LCherryholmes · Claude Sonnet 4.6   DATE: 2026-04-15
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
CORPUS="/home/claude/corpus/programs/icon"
PASS=0; FAIL=0

if [ ! -x "$SCRIP" ]; then echo "SKIP scrip not found at $SCRIP"; exit 0; fi
if [ ! -d "$CORPUS" ]; then echo "SKIP corpus not found at $CORPUS"; exit 0; fi

run() {
    local base="$CORPUS/$1"
    local got want
    got=$(timeout 8 "$SCRIP" --ir-run "${base}.icn" < /dev/null 2>/dev/null) || true
    want=$(cat "${base}.expected")
    if [ "$got" = "$want" ]; then
        echo "  PASS $1"; PASS=$((PASS+1))
    else
        echo "  FAIL $1"
        echo "    want: $(echo "$want" | tr '\n' '|')"
        echo "    got:  $(echo "$got"  | tr '\n' '|')"
        FAIL=$((FAIL+1))
    fi
}

echo "=== rung14: Icon limit (E \\ N) ==="
run rung14_limit_limit_to
run rung14_limit_limit_alt
run rung14_limit_limit_zero
run rung14_limit_limit_large
run rung14_limit_limit_str

echo ""
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
