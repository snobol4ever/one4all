#!/usr/bin/env bash
# test_prolog_rung32_bridge_negation.sh — PR-19b driver tests for negation builtins
# (\+/1, not/1, once/1) with goal-as-variable. Extends the v3 Term→EXPR bridge
# from catch/3 (PR-19a) to these three sites in pl_runtime.c.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
CORPUS=/home/claude/corpus/programs/prolog/rung32_bridge_negation
PASS=0; FAIL=0
echo "=== rung32_bridge_negation: \\+/1, not/1, once/1 with goal-as-variable (PR-19b driver) ==="
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
