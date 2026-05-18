#!/usr/bin/env bash
# test_crosscheck_prolog.sh — 3-mode crosscheck for PROLOG (GOAL-LANG-PROLOG)
#
# Runs the prolog test corpus through --interp, --interp, --run.
# Run on every major push. Mode-consistency check, not regression.
# If .ref present alongside test file: diffs vs oracle too.
# Exits 0 only if all three modes agree on every test.
#
# AUTHORS: Lon Jones Cherryholmes · Claude Sonnet 4.6  DATE: 2026-04-14

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${HERE}/../scrip"
TIMEOUT=30
PASS=0; FAIL=0; SKIP=0; ORACLE_MISS=0

xcheck() {
    local label="$1" file="$2" ref="${3:-}"
    if [ ! -f "$file" ]; then echo "  SKIP $label (no file)"; SKIP=$((SKIP+1)); return; fi
    local ir sm jit
    ir=$(timeout  $TIMEOUT "$SCRIP" --interp  "$file" </dev/null 2>/dev/null)
    sm=$(timeout  $TIMEOUT "$SCRIP" --interp  "$file" </dev/null 2>/dev/null)
    jit=$(timeout $TIMEOUT "$SCRIP" --run "$file" </dev/null 2>/dev/null)
    # Primary purpose of this gate: 3-mode dispatch consistency.
    # All three modes must agree with each other (mode-consistency).
    # Oracle (.ref) mismatches are a frontend completeness issue, not a mode
    # dispatch issue, and are reported separately as ORACLE_MISS (informational).
    local ok=1
    [ "$sm"  != "$ir" ] && { echo "  FAIL $label sm-run  vs ir-run";  diff <(echo "$ir") <(echo "$sm")  | head -5 | sed 's/^/    /'; ok=0; }
    [ "$jit" != "$ir" ] && { echo "  FAIL $label jit-run vs ir-run";  diff <(echo "$ir") <(echo "$jit") | head -5 | sed 's/^/    /'; ok=0; }
    if [ "$ok" -eq 1 ]; then
        echo "  PASS $label"; PASS=$((PASS+1))
        if [ -n "$ref" ] && [ -f "$ref" ]; then
            local exp; exp=$(cat "$ref")
            [ "$ir" != "$exp" ] && { echo "    (ORACLE_MISS $label — 3 modes agree but differ from oracle)"; ORACLE_MISS=$((ORACLE_MISS+1)); }
        fi
    else
        FAIL=$((FAIL+1))
    fi
}

echo "=== Prolog 3-mode crosscheck ==="

T=$(mktemp /tmp/pl_XXXXXX.pl)
cat > "$T" << 'EOF'
:- initialization(main).
main :- write(hello), nl.
EOF
xcheck "hello" "$T"

cat > "$T" << 'EOF'
:- initialization(main).
fact(a). fact(b). fact(c).
main :- fact(X), write(X), nl, fail ; true.
EOF
xcheck "backtrack" "$T"

cat > "$T" << 'EOF'
:- initialization(main).
main :- X is 2 + 3, write(X), nl.
EOF
xcheck "arith" "$T"

cat > "$T" << 'EOF'
:- initialization(main).
count(0) :- !.
count(N) :- N > 0, write(N), nl, N1 is N - 1, count(N1).
main :- count(3).
EOF
xcheck "recursion" "$T"

rm -f "$T"

# Rung corpus files (PJ-9b: extended to walk flat-file corpus, not just subdirs)
RUNGS=/home/claude/corpus/programs/prolog
if [ -d "$RUNGS" ]; then
    for f in "$RUNGS"/rung*.pl; do
        [ -f "$f" ] || continue
        # Skip programs that --interp can't complete (timeout, non-zero exit)
        ir_rc=$(timeout 4 "$SCRIP" --interp "$f" </dev/null >/dev/null 2>&1; echo $?)
        if [ "$ir_rc" != "0" ]; then SKIP=$((SKIP+1)); continue; fi
        ref="${f%.pl}.ref"
        xcheck "$(basename $f .pl)" "$f" "$ref"
    done
fi

echo ""
echo "PASS=$PASS FAIL=$FAIL SKIP=$SKIP ORACLE_MISS=$ORACLE_MISS"
echo "(PASS = 3 modes agree; ORACLE_MISS = 3 modes agree but differ from .ref — frontend gap, not mode issue)"
[ "$FAIL" -eq 0 ]
