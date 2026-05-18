#!/usr/bin/env bash
# test_prolog_rung35_bridge_setup.sh — PR-19e driver tests for setup_call_cleanup/3
# with goal-as-variable in any position.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
CORPUS=/home/claude/corpus/programs/prolog/rung35_bridge_setup
PASS=0; FAIL=0
echo "=== rung35_bridge_setup: setup_call_cleanup/3 with goal-as-variable (PR-19e driver) ==="
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
        echo "    expected: $(echo "$expected" | head -3)"
        echo "    actual:   $(echo "$actual"   | head -3)"
        FAIL=$((FAIL+1))
    fi
done
echo ""
echo "PASS=$PASS FAIL=$FAIL"; [ "$FAIL" -eq 0 ]
