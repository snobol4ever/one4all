#!/usr/bin/env bash
# test/regression.sh — scrip regression: all known-good programs vs .ref
# Usage: CORPUS=/home/claude/corpus bash test/regression.sh
# From:  /home/claude/one4all/
#
# Sections:
#   1. crosscheck corpus (patterns, capture, assign, arith, control, etc.)
#   2. beauty library drivers (19 subsystems)
#   3. demo programs
#   4. CSNOBOL4 Budne suite (116 tests)
#   5. FENCE crosscheck tests (10 tests)

set -uo pipefail
INTERP="${INTERP:-./scrip}"
CORPUS="${CORPUS:-/home/claude/corpus}"
TIMEOUT="${TIMEOUT:-15}"
INC="${INC:-$CORPUS/programs/snobol4/demo/inc}"
BEAUTY="${BEAUTY:-$CORPUS/programs/snobol4/beauty}"
DEMO="${DEMO:-$CORPUS/programs/snobol4/demo}"
SUITE="${SUITE:-$CORPUS/programs/csnobol4-suite}"
FENCE="${FENCE:-$CORPUS/crosscheck/patterns}"

PASS=0; FAIL=0
FAILURES=""

SKIP_LIST="bench breakline genc k ndbm sleep time line2"
STDIN_TESTS="atn crlf longrec rewind1 sudoku trim0 trim1 uneval2"

is_excluded() { for s in $SKIP_LIST; do [ "$1" = "$s" ] && return 0; done; return 1; }
is_stdin_test() { for s in $STDIN_TESTS; do [ "$1" = "$s" ] && return 0; done; return 1; }

split_at_end() {
    python3 - "$1" "$2" "$3" << 'PY'
import sys, re
src = open(sys.argv[1], 'r', errors='replace').read()
lines = src.split('\n')
end_idx = next((i for i, l in enumerate(lines) if re.match(r'^END\s*$', l)), None)
if end_idx is not None:
    open(sys.argv[2], 'w').write('\n'.join(lines[:end_idx+1]) + '\n')
    open(sys.argv[3], 'w').write('\n'.join(lines[end_idx+1:]))
else:
    open(sys.argv[2], 'w').write(src)
    open(sys.argv[3], 'w').write('')
PY
}

run_test() {
    local label="$1" sno="$2" ref="$3"
    [ -f "$ref" ] || return
    local got exp name
    name=$(basename "$sno" .sno)
    if is_stdin_test "$name"; then
        local pt st
        pt=$(mktemp /tmp/scrip_prog_XXXXXX.sno); st=$(mktemp /tmp/scrip_stdin_XXXXXX)
        split_at_end "$sno" "$pt" "$st"
        got=$(SNO_LIB="$INC" timeout "$TIMEOUT" $INTERP "$pt" < "$st" 2>/dev/null || true)
        rm -f "$pt" "$st"
    else
        got=$(SNO_LIB="$INC" timeout "$TIMEOUT" $INTERP "$sno" 2>/dev/null || true)
    fi
    exp=$(cat "$ref")
    if [ "$got" = "$exp" ]; then
        PASS=$((PASS+1))
    else
        FAIL=$((FAIL+1))
        FAILURES="${FAILURES}  FAIL ${label}\n"
    fi
}

echo "=== scrip regression ==="
echo ""

# 1. crosscheck corpus
echo "── crosscheck corpus ──"
while IFS= read -r sno; do
    ref="${sno%.sno}.ref"; [ -f "$ref" ] || continue
    run_test "$(basename "$sno" .sno)" "$sno" "$ref"
done < <(find "$CORPUS/crosscheck" -name "*.sno" | sort)

# 2. beauty drivers
echo "── beauty drivers ──"
for sno in "$BEAUTY"/beauty_*_driver.sno; do
    [ -f "$sno" ] || continue
    name=$(basename "$sno" .sno)
    run_test "$name" "$sno" "$BEAUTY/${name}.ref"
done

# 3. demo programs
echo "── demos ──"
run_test "demo_wordcount" "$DEMO/wordcount.sno" "$DEMO/wordcount.ref"
run_test "demo_treebank"  "$DEMO/treebank.sno"  "$DEMO/treebank.ref"
run_test "demo_claws5"    "$DEMO/claws5.sno"    "$DEMO/claws5.ref"
TIMEOUT=30 run_test "demo_roman" "$DEMO/roman.sno" "$DEMO/roman.ref"

# 4. CSNOBOL4 Budne suite
echo "── csnobol4 suite ──"
for sno in "$SUITE"/*.sno; do
    [ -f "$sno" ] || continue
    name=$(basename "$sno" .sno)
    is_excluded "$name" && continue
    run_test "$name" "$sno" "${sno%.sno}.ref"
done

# 5. FENCE tests
echo "── fence tests ──"
for sno in "$FENCE"/*_pat_fence*.sno; do
    [ -f "$sno" ] || continue
    run_test "$(basename "$sno" .sno)" "$sno" "${sno%.sno}.ref"
done

echo ""
echo "PASS=$PASS FAIL=$FAIL  ($(( PASS + FAIL )) total)"
[ -n "$FAILURES" ] && printf "$FAILURES" | head -40
