#!/usr/bin/env bash
# test_prolog_rung_24.sh — PL-9 string/IO builtins
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
CORPUS=/home/claude/corpus/programs/prolog/rung24
PASS=0; FAIL=0
echo "=== rung24 ==="
for f in "$CORPUS"/*.pl; do
    ref="${f%.pl}.ref"; [ -f "$ref" ] || continue
    actual=$(timeout 8 "$SCRIP" --interp "$f" 2>/dev/null)
    expected=$(cat "$ref")
    if [ "$actual" = "$expected" ]; then echo "  PASS $(basename "$f")"; PASS=$((PASS+1))
    else echo "  FAIL $(basename "$f")"; FAIL=$((FAIL+1)); fi
done
echo ""; echo "PASS=$PASS FAIL=$FAIL"; [ "$FAIL" -eq 0 ]
