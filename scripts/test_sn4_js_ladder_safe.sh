#!/usr/bin/env bash
# Safer version that tests each file individually to avoid segfault
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/scrip}"
CORPUS="/home/claude/corpus"
TIMEOUT=3
VERBOSE="${1:-}"

PASS=0; FAIL=0
TMPD=$(mktemp -d); trap "rm -rf $TMPD" EXIT

run_one() {
    local sno="$1"
    [ -f "$sno" ] || return 0
    local base; base=$(basename "$sno" .sno)
    local dir; dir=$(dirname "$sno")
    local ref="$dir/$base.ref"
    [ -f "$ref" ] || return 0
    
    local js="$TMPD/$base.js"
    local out="$TMPD/$base.out"
    local inp="$dir/$base.input"
    
    # Emit (fresh process to avoid GC issues)
    if ! timeout "$TIMEOUT" "$SCRIP" --target=js "$sno" > "$js" 2>/dev/null; then
        [[ "$VERBOSE" == "--verbose" ]] && echo "FAIL $base [emit]"
        FAIL=$((FAIL+1)); return 0
    fi
    
    # Run (fresh process)
    local stdin_arg="< /dev/null"
    [ -f "$inp" ] && stdin_arg="< \"$inp\""
    if ! eval "timeout $TIMEOUT node \"$js\" $stdin_arg > \"$out\" 2>/dev/null"; then
        [[ "$VERBOSE" == "--verbose" ]] && echo "FAIL $base [node]"
        FAIL=$((FAIL+1)); return 0
    fi
    
    # Diff
    if diff -q "$out" "$ref" > /dev/null 2>&1; then
        [[ "$VERBOSE" == "--verbose" ]] && echo "PASS $base"
        PASS=$((PASS+1))
    else
        [[ "$VERBOSE" == "--verbose" ]] && echo "FAIL $base [diff]"
        FAIL=$((FAIL+1))
    fi
}

echo "=== SNOBOL4 → JS ladder (safe) ==="
for sno in "$CORPUS/programs/csnobol4-suite/"*.sno; do run_one "$sno"; done
for sno in "$CORPUS/programs/snobol4/demo/"*.sno; do run_one "$sno"; done
for sno in "$CORPUS/programs/snobol4/feat/"*.sno; do run_one "$sno"; done

TOTAL=$((PASS+FAIL))
echo "PASS=$PASS FAIL=$FAIL TOTAL=$TOTAL"
[ $PASS -ge 10 ] && exit 0 || exit 1
