#!/usr/bin/env bash
# test_prolog_swi_suite.sh — run SWI plunit conformance suite under --ir-run
# Iterates corpus/programs/prolog/swi_tests/test_*.pl, loads each with
# plunit.pl shim + a main wrapper, compares PASS/FAIL per suite against .ref.
#
# Matching: set-based (order-independent, deduped — ignores double-run artefacts).
# Exit 0 if coverage >= 80%, exit 1 otherwise.
# Options:
#   --verbose       show raw scrip output for failing files
#   --file NAME     run only NAME.pl  (e.g. --file test_bips)
#   --mode MODE     --ir-run | --sm-run | --jit-run  (default: --ir-run)
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
CORPUS=/home/claude/corpus/programs/prolog
SWIT=$CORPUS/swi_tests
PLUNIT=$CORPUS/plunit.pl
MATCH_PY="${HERE}/util_swi_match.py"
REPORT_PY="${HERE}/util_swi_report.py"
WRAP=$(mktemp /tmp/pl_wrap_XXXXXX.pl)
ACTUAL_TMP=$(mktemp /tmp/pl_actual_XXXXXX.txt)
trap 'rm -f "$WRAP" "$ACTUAL_TMP"' EXIT

VERBOSE=0; ONLY_FILE=""; MODE="--ir-run"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --verbose)  VERBOSE=1; shift ;;
        --file)     ONLY_FILE="$2"; shift 2 ;;
        --mode)     MODE="$2"; shift 2 ;;
        --ir-run|--sm-run|--jit-run) MODE="$1"; shift ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
done

[ -d "$SWIT" ]   || { echo "SKIP: $SWIT missing"; exit 0; }
[ -f "$PLUNIT" ] || { echo "SKIP: $PLUNIT missing"; exit 0; }
[ -x "$SCRIP" ]  || { echo "SKIP: scrip not built"; exit 0; }

printf 'main :- run_tests.\n' > "$WRAP"

PASS=0; FAIL=0; TOTAL=0

for f in "$SWIT"/test_*.pl; do
    base=$(basename "$f" .pl)
    ref="$SWIT/${base}.ref"
    [ -f "$ref" ] || continue
    [ -z "$ONLY_FILE" ] || [ "$base" = "$ONLY_FILE" ] || continue

    suite_total=$(wc -l < "$ref")
    TOTAL=$((TOTAL + suite_total))

    timeout 30 "$SCRIP" "$MODE" "$PLUNIT" "$f" "$WRAP" < /dev/null 2>/dev/null \
        | grep -E '^(PASS|FAIL) ' > "$ACTUAL_TMP" || true

    matched=$(python3 "$MATCH_PY" "$ref" "$ACTUAL_TMP")
    PASS=$((PASS + matched))
    FAIL=$((FAIL + suite_total - matched))

    if [ "$matched" -eq "$suite_total" ]; then
        echo "  PASS $base ($suite_total suite-lines)"
    else
        echo "  FAIL $base  match=$matched/$suite_total"
        python3 "$REPORT_PY" "$ref" "$ACTUAL_TMP"
        if [ "$VERBOSE" -eq 1 ]; then
            echo "  --- raw output ---"
            cat "$ACTUAL_TMP"
            echo "  ---"
        fi
    fi
done

echo ""
echo "Suite totals: PASS=$PASS FAIL=$FAIL TOTAL=$TOTAL  mode=$MODE"
[ "$TOTAL" -gt 0 ] || { echo "SKIP: no test files found"; exit 0; }
pct=$((PASS * 100 / TOTAL))
echo "Coverage: ${pct}%  (gate: >=80%)"
[ "$pct" -ge 80 ]
