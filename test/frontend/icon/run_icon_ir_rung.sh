#!/bin/bash
# run_icon_ir_rung.sh — Icon --ir-run rung ladder runner
# Usage: bash run_icon_ir_rung.sh [--rung RUNG] [--scrip PATH] [--corpus PATH]
#
# Runs rung01–rung11 (or a specific rung) of the Icon corpus against
# scrip --ir-run and reports PASS/FAIL vs .expected files.
#
# Authors: LCherryholmes · Claude Sonnet 4.6

set -euo pipefail

SCRIP="${SCRIP:-$(cd "$(dirname "$0")/../../.." && pwd)/scrip}"
CORPUS="${CORPUS_REPO:-$(cd "$(dirname "$0")/../../.." && pwd)/corpus}/programs/icon"
RUNG=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --rung)   RUNG="$2"; shift 2 ;;
        --scrip)  SCRIP="$2"; shift 2 ;;
        --corpus) CORPUS="$2"; shift 2 ;;
        *) echo "Usage: $0 [--rung RUNG] [--scrip PATH] [--corpus PATH]"; exit 1 ;;
    esac
done

if [ ! -x "$SCRIP" ]; then
    echo "ERROR: scrip binary not found at $SCRIP" >&2
    exit 1
fi
if [ ! -d "$CORPUS" ]; then
    echo "ERROR: corpus dir not found at $CORPUS" >&2
    exit 1
fi

PASS=0; FAIL=0

run_one() {
    local icn="$1"
    local exp="${icn%.icn}.expected"
    [ -f "$exp" ] || return 0
    local got want name
    name=$(basename "$icn" .icn)
    got=$(timeout 5 "$SCRIP" --ir-run "$icn" 2>/dev/null) || true
    want=$(cat "$exp")
    if [ "$got" = "$want" ]; then
        echo "PASS $name"
        PASS=$((PASS+1))
    else
        echo "FAIL $name"
        echo "  want: $(echo "$want" | tr '\n' '|')"
        echo "  got:  $(echo "$got"  | tr '\n' '|')"
        FAIL=$((FAIL+1))
    fi
}

if [ -n "$RUNG" ]; then
    # Run a specific rung prefix
    for icn in "$CORPUS"/${RUNG}_*.icn; do
        [ -f "$icn" ] || continue
        run_one "$icn"
    done
else
    # Run rung01–rung11
    for icn in "$CORPUS"/rung0[1-9]_*.icn \
               "$CORPUS"/rung10_*.icn \
               "$CORPUS"/rung11_*.icn; do
        [ -f "$icn" ] || continue
        run_one "$icn"
    done
fi

echo "--- Icon --ir-run: PASS=$PASS FAIL=$FAIL TOTAL=$((PASS+FAIL)) ---"
[ "$FAIL" -eq 0 ]
