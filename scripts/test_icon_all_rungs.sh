#!/usr/bin/env bash
# scripts/test_icon_all_rungs.sh — Icon rung ladder runner under scrip --interp.
# Self-contained. Run from anywhere with no env vars.
# Usage: bash scripts/test_icon_all_rungs.sh [--rung RUNG] [--scrip PATH] [--corpus PATH]
#
# Runs rung01–rung36 (or a specific rung) of the Icon corpus against
# scrip --interp and reports PASS/FAIL/XFAIL vs .expected files.
# Files with a matching .xfail marker are skipped as known-unimplemented (XFAIL).
# rung36 uses timeout 30s (large JCON programs); all others use 8s.
#
# Replaces test_icon_all_rungs.sh (deleted, was --interp-based and gated on
# the now-amputated Icon AST walker). The reference path for Icon is --interp.
#
# Authors: LCherryholmes · Claude Sonnet 4.6 · Claude Opus 4.7

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
CORPUS="${CORPUS:-/home/claude/corpus/programs/icon}"
RUNG=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --rung)   RUNG="$2";   shift 2 ;;
        --scrip)  SCRIP="$2";  shift 2 ;;
        --corpus) CORPUS="$2"; shift 2 ;;
        *) echo "Usage: $0 [--rung RUNG] [--scrip PATH] [--corpus PATH]" >&2; exit 1 ;;
    esac
done

if [ ! -x "$SCRIP" ]; then
    echo "SKIP scrip binary not found at $SCRIP" >&2
    exit 0
fi
if [ ! -d "$CORPUS" ]; then
    echo "SKIP corpus not found at $CORPUS" >&2
    echo "     clone snobol4ever/corpus to /home/claude/corpus to run this suite" >&2
    exit 0
fi

PASS=0; FAIL=0; XFAIL=0

run_one() {
    local icn="$1"
    local tmo="${2:-8}"
    local exp="${icn%.icn}.expected"
    [ -f "$exp" ] || return 0
    local base="${icn%.icn}"
    local name
    name=$(basename "$icn" .icn)
    if [ -f "${base}.xfail" ]; then
        echo "XFAIL $name"
        XFAIL=$((XFAIL+1))
        return 0
    fi
    local stdin_file="${base}.stdin"
    local got want
    if [ -f "$stdin_file" ]; then
        got=$(timeout "$tmo" "$SCRIP" --interp "$icn" < "$stdin_file" 2>/dev/null) || true
    else
        got=$(timeout "$tmo" "$SCRIP" --interp "$icn" < /dev/null     2>/dev/null) || true
    fi
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
    for icn in "$CORPUS"/${RUNG}_*.icn; do
        [ -f "$icn" ] || continue
        run_one "$icn"
    done
else
    for icn in "$CORPUS"/rung0[1-9]_*.icn \
               "$CORPUS"/rung1[0-9]_*.icn \
               "$CORPUS"/rung2[0-9]_*.icn \
               "$CORPUS"/rung3[0-5]_*.icn; do
        [ -f "$icn" ] || continue
        run_one "$icn" 8
    done
    for icn in "$CORPUS"/rung36_*.icn; do
        [ -f "$icn" ] || continue
        run_one "$icn" 30
    done
fi

echo "--- Icon --interp: PASS=$PASS FAIL=$FAIL XFAIL=$XFAIL TOTAL=$((PASS+FAIL+XFAIL)) ---"
[ "$FAIL" -eq 0 ]
