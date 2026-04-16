#!/usr/bin/env bash
# util_diagnose_prolog_swi.sh — show per-test detail for one SWI test file
# Prints full raw scrip output, then diff vs .ref, then a summary.
#
# Usage:
#   bash scripts/util_diagnose_prolog_swi.sh test_bips
#   bash scripts/util_diagnose_prolog_swi.sh test_arith --mode --sm-run
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
CORPUS=/home/claude/corpus/programs/prolog
SWIT=$CORPUS/swi_tests
PLUNIT=$CORPUS/plunit.pl
WRAP=$(mktemp /tmp/pl_wrap_XXXXXX.pl)
trap 'rm -f "$WRAP"' EXIT

BASE="${1:-}"
MODE="--ir-run"
shift || true
while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode) MODE="$2"; shift 2 ;;
        --ir-run|--sm-run|--jit-run) MODE="$1"; shift ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
done

[ -n "$BASE" ] || { echo "Usage: $0 <test_name>  e.g. test_bips"; exit 1; }
f="$SWIT/${BASE}.pl"
ref="$SWIT/${BASE}.ref"
[ -f "$f" ]    || { echo "ERROR: $f not found"; exit 1; }
[ -f "$ref" ]  || { echo "ERROR: $ref not found"; exit 1; }
[ -f "$PLUNIT" ] || { echo "SKIP: $PLUNIT missing"; exit 0; }
[ -x "$SCRIP" ]  || { echo "SKIP: scrip not built"; exit 0; }

printf 'main :- run_tests.\n' > "$WRAP"

echo "=== Raw scrip output ($MODE) ==="
raw=$(timeout 30 "$SCRIP" "$MODE" "$PLUNIT" "$f" "$WRAP" < /dev/null 2>/dev/null)
printf '%s\n' "$raw"

actual=$(printf '%s\n' "$raw" | grep -E '^(PASS|FAIL) ')
expected=$(cat "$ref")

echo ""
echo "=== Diff (< expected  > actual) ==="
diff <(printf '%s\n' "$expected") <(printf '%s\n' "$actual") || true

echo ""
echo "=== Summary ==="
suite_total=$(wc -l < "$ref")
matched=$(comm -12 <(printf '%s\n' "$expected" | sort) \
                   <(printf '%s\n' "$actual"   | sort) | wc -l)
echo "match=$matched/$suite_total  file=$BASE  mode=$MODE"
