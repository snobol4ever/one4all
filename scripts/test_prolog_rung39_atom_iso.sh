#!/usr/bin/env bash
# test_prolog_rung39_atom_iso.sh — PR-16 driver: ISO atom builtins
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
CORPUS=/home/claude/corpus/programs/prolog/rung39_atom_iso
PASS=0; FAIL=0
echo "=== rung39_atom_iso: ISO §7.8 atom builtins (PR-16 driver) ==="
for f in "$CORPUS"/*.pl; do
    ref="${f%.pl}.ref"
    [ -f "$ref" ] || continue
    actual=$(timeout 8 "$SCRIP" --interp "$f" < /dev/null 2>/dev/null)
    expected=$(cat "$ref")
    if [ "$actual" = "$expected" ]; then
        echo "  PASS $(basename $f)"; PASS=$((PASS+1))
    else
        echo "  FAIL $(basename $f)"
        echo "    expected: $(cat $ref)"
        echo "    actual:   $actual"
        FAIL=$((FAIL+1))
    fi
done
echo ""
echo "PASS=$PASS FAIL=$FAIL"; [ "$FAIL" -eq 0 ]
