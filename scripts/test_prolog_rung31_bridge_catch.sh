#!/usr/bin/env bash
# test_prolog_rung31_bridge_catch.sh — PR-19a driver tests for catch/3 with goal-as-variable.
# This is the driving rung for the v3 Term→EXPR bridge. Bridge lands when this passes 5/5.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
CORPUS=/home/claude/corpus/programs/prolog/rung31_bridge_catch
PASS=0; FAIL=0
echo "=== rung31_bridge_catch: catch/3 with goal-as-variable (PR-19a driver) ==="
if [ ! -d "$CORPUS" ]; then
    echo "SKIP: corpus dir not present at $CORPUS"
    exit 0
fi
for f in "$CORPUS"/*.pl; do
    ref="${f%.pl}.ref"; [ -f "$ref" ] || continue
    actual=$(timeout 8 "$SCRIP" --interp "$f" < /dev/null 2>/dev/null)
    expected=$(cat "$ref")
    if [ "$actual" = "$expected" ]; then
        echo "  PASS $(basename "$f")"; PASS=$((PASS+1))
    else
        echo "  FAIL $(basename "$f")"
        echo "    expected: $(echo "$expected" | head -2)"
        echo "    actual:   $(echo "$actual"   | head -2)"
        FAIL=$((FAIL+1))
    fi
done
echo ""
echo "PASS=$PASS FAIL=$FAIL"; [ "$FAIL" -eq 0 ]
