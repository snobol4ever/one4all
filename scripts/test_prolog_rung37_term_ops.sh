#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
CORPUS=/home/claude/corpus/programs/prolog
RUNG="$CORPUS/rung37_term_ops"

echo "=== rung37_term_ops: ISO §7.6 term ops — =.., functor, arg, copy_term (PR-14 driver) ==="

if [ ! -d "$RUNG" ]; then echo "SKIP  rung37 not found"; exit 0; fi
if [ ! -f "$SCRIP" ]; then echo "SKIP  scrip not found"; exit 0; fi

PASS=0; FAIL=0
for f in "$RUNG"/*.pl; do
    ref="${f%.pl}.ref"
    [ -f "$ref" ] || continue
    actual=$(timeout 8 "$SCRIP" --interp "$f" < /dev/null 2>/dev/null)
    expected=$(cat "$ref")
    if [ "$actual" = "$expected" ]; then
        echo "  PASS $(basename $f)"; PASS=$((PASS+1))
    else
        echo "  FAIL $(basename $f)"
        echo "    expected: $(echo "$expected" | head -4)"
        echo "    actual:   $(echo "$actual"   | head -4)"
        FAIL=$((FAIL+1))
    fi
done
echo ""
echo "PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ]
