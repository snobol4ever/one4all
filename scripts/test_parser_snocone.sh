#!/usr/bin/env bash
# test_parser_snocone.sh — smoke test for parser_snocone.sc (PST-SC-SC-5)
# Runs each .sc fixture through the SCRIP parser and diffs against .ref
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6  DATE: 2026-05-19
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/scrip}"
FIX="${FIX:-/home/claude/corpus/programs/snocone/parser-fixtures}"
SD="${SD:-/home/claude/corpus/SCRIP}"

if [ ! -f "$SCRIP" ]; then echo "SKIP scrip not found: $SCRIP"; exit 0; fi
if [ ! -d "$FIX" ];   then echo "SKIP fixtures not found: $FIX";  exit 0; fi

PASS=0; FAIL=0
for sc in "$FIX"/*.sc; do
    base="${sc%.sc}"
    ref="${base}.ref"
    [ -f "$ref" ] || continue
    name="$(basename "$base")"
    got=$(timeout 8 "$SCRIP" --interp \
        "$SD/global.sc" "$SD/case.sc" "$SD/assign.sc" "$SD/match.sc" \
        "$SD/counter.sc" "$SD/stack.sc" "$SD/tree.sc" "$SD/ShiftReduce.sc" \
        "$SD/tdump.sc" "$SD/gen.sc" "$SD/qize.sc" "$SD/semantic.sc" \
        "$SD/omega.sc" "$SD/trace.sc" \
        "$SD/parser_snocone.sc" < "$sc" 2>/dev/null)
    exp=$(cat "$ref")
    if [ "$got" = "$exp" ]; then
        PASS=$((PASS+1))
    else
        FAIL=$((FAIL+1))
        echo "  FAIL $name"
        echo "    exp: $(echo "$exp" | head -1)"
        echo "    got: $(echo "$got" | head -1)"
    fi
done
echo ""
echo "PASS=$PASS FAIL=$FAIL"
