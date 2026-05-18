#!/usr/bin/env bash
# test_icon_ir_rung_32.sh — rung32: string return values — IC-7
# Gate: PASS=5 FAIL=0 XFAIL=0
# Authors: LCherryholmes · Claude Sonnet 4.6   DATE: 2026-04-16
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
CORPUS="${CORPUS:-/home/claude/corpus/programs/icon}"
PASS=0; FAIL=0; XFAIL=0

if [ ! -x "$SCRIP" ];  then echo "SKIP scrip not found at $SCRIP";  exit 0; fi
if [ ! -d "$CORPUS" ]; then echo "SKIP corpus not found at $CORPUS"; exit 0; fi

run() {
    local base="$CORPUS/$1"
    [ -f "${base}.xfail" ] && { echo "  XFAIL $1"; XFAIL=$((XFAIL+1)); return; }
    [ -f "${base}.expected" ] || { echo "  SKIP  $1 (no .expected)"; return; }
    local stdin_f="${base}.stdin"
    local got want
    if [ -f "$stdin_f" ]; then
        got=$(timeout 8 "$SCRIP" --interp "${base}.icn" < "$stdin_f"  2>/dev/null) || true
    else
        got=$(timeout 8 "$SCRIP" --interp "${base}.icn" < /dev/null   2>/dev/null) || true
    fi
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

echo "=== rung32: string return values ==="
run rung32_strretval_basic_strret
run rung32_strretval_chain
run rung32_strretval_strret_assign
run rung32_strretval_strret_every
run rung32_strretval_two_str_params

echo ""
echo "PASS=$PASS FAIL=$FAIL XFAIL=$XFAIL"
[ "$FAIL" -eq 0 ]
