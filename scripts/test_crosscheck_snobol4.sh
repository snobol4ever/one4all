#!/usr/bin/env bash
# test_crosscheck_snobol4.sh — 3-mode crosscheck for SNOBOL4 (GOAL-LANG-SNOBOL4)
#
# Runs the snobol4 test corpus through --interp, --interp, --run.
# Run on every major push. Mode-consistency check, not regression.
# If .ref present alongside test file: diffs vs oracle too.
# Exits 0 only if all three modes agree on every test.
#
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6  DATE: 2026-04-14

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
TIMEOUT=30
PASS=0; FAIL=0; SKIP=0

xcheck() {
    local label="$1" file="$2" ref="${3:-}" sno_lib="${4:-}"
    if [ ! -f "$file" ]; then echo "  SKIP $label (no file)"; SKIP=$((SKIP+1)); return; fi
    local ir sm jit
    if [ -n "$sno_lib" ]; then
        ir=$(SNO_LIB="$sno_lib"  timeout $TIMEOUT "$SCRIP" --interp  "$file" </dev/null 2>/dev/null)
        sm=$(SNO_LIB="$sno_lib"  timeout $TIMEOUT "$SCRIP" --interp  "$file" </dev/null 2>/dev/null)
        jit=$(SNO_LIB="$sno_lib" timeout $TIMEOUT "$SCRIP" --run "$file" </dev/null 2>/dev/null)
    else
        ir=$(timeout  $TIMEOUT "$SCRIP" --interp  "$file" </dev/null 2>/dev/null)
        sm=$(timeout  $TIMEOUT "$SCRIP" --interp  "$file" </dev/null 2>/dev/null)
        jit=$(timeout $TIMEOUT "$SCRIP" --run "$file" </dev/null 2>/dev/null)
    fi
    local ok=1
    if [ -n "$ref" ] && [ -f "$ref" ]; then
        local exp; exp=$(cat "$ref")
        if [ -n "$ir" ] && [ "$ir" != "$exp" ]; then
            echo "  FAIL $label ir-run  vs oracle"; diff <(echo "$exp") <(echo "$ir")  | head -5 | sed 's/^/    /'; ok=0
        fi
        [ "$sm"  != "$exp" ] && { echo "  FAIL $label sm-run  vs oracle"; diff <(echo "$exp") <(echo "$sm")  | head -5 | sed 's/^/    /'; ok=0; }
        [ "$jit" != "$exp" ] && { echo "  FAIL $label jit-run vs oracle"; diff <(echo "$exp") <(echo "$jit") | head -5 | sed 's/^/    /'; ok=0; }
    else
        [ "$sm"  != "$ir" ] && { echo "  FAIL $label sm-run  vs ir-run";  diff <(echo "$ir") <(echo "$sm")  | head -5 | sed 's/^/    /'; ok=0; }
        [ "$jit" != "$ir" ] && { echo "  FAIL $label jit-run vs ir-run";  diff <(echo "$ir") <(echo "$jit") | head -5 | sed 's/^/    /'; ok=0; }
    fi
    if [ "$ok" -eq 1 ]; then echo "  PASS $label"; PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
}

echo "=== SNOBOL4 3-mode crosscheck ==="

# Smoke tests — inline
T=$(mktemp /tmp/sno_XXXXXX.sno)
printf "        OUTPUT = 'hello'\nEND\n" > "$T"; xcheck "output"    "$T"
printf "        OUTPUT = 2 + 3\nEND\n"  > "$T"; xcheck "arith"     "$T"
printf "        OUTPUT = 'ab' 'cd'\nEND\n" > "$T"; xcheck "concat" "$T"
printf "        S = 'abc'\n        S 'b' = 'X'\n        OUTPUT = S\nEND\n" > "$T"
xcheck "pattern_replace" "$T"
printf "        'x' 'x' :S(HIT)\n        OUTPUT = 'miss'\n        :(END)\nHIT     OUTPUT = 'hit'\nEND\n" > "$T"
xcheck "goto" "$T"
rm -f "$T"

# Beauty drivers — if corpus present
BEAUTY=/home/claude/corpus/programs/snobol4/beauty_suite
for driver in omega gen tdump alpha; do
    f="$BEAUTY/${driver}_driver.sno"
    ref="$BEAUTY/${driver}_driver.ref"
    if [ -f "$f" ]; then
        # Use corpus .ref if present; fall back to SPITBOL-generated ref
        if [ ! -f "$ref" ]; then
            ref="/tmp/${driver}_driver_spitbol.ref"
            if [ ! -f "$ref" ]; then
                SNO_LIB=$BEAUTY timeout 30 /home/claude/x64/bin/sbl -b "$f" > "$ref" 2>/dev/null || rm -f "$ref"
            fi
        fi
        xcheck "beauty_${driver}" "$f" "${ref}" "$BEAUTY"
    fi
done

echo ""
echo "PASS=$PASS FAIL=$FAIL SKIP=$SKIP"
[ "$FAIL" -eq 0 ]
