#!/usr/bin/env bash
# test_prolog_rung34_bridge_setof.sh — PR-19d driver tests for findall/3, setof/3, bagof/3
# with goal-as-variable. Extends the v3 Term→EXPR bridge to generator builtins.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
CORPUS=/home/claude/corpus/programs/prolog/rung34_bridge_setof
PASS=0; FAIL=0
echo "=== rung34_bridge_setof: findall/3 with goal-as-variable (PR-19d driver) ==="
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
