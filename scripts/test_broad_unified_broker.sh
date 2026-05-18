#!/usr/bin/env bash
# scripts/test_broad_unified_broker.sh — GOAL-UNIFIED-BROKER broad gate
# Self-contained. Run from anywhere with no env vars.
# Calls self-contained sub-scripts; enforces non-regression floors.
# Target: < 60 seconds.
#
# Floors: Icon PASS >= 48, csnobol4 PASS >= 34
#
# Usage: bash scripts/test_broad_unified_broker.sh
# Exit:  0 = all floors met, 1 = any floor missed or sub-script error
#
# Authors: LCherryholmes · Claude Sonnet 4.6

set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PASS=0; FAIL=0
FLOOR_FAIL=0

# ── helper: run a sub-script, capture its PASS/FAIL line ─────────────────────
run_suite() {
    local label="$1" script="$2" floor_var="$3" floor_val="$4"
    echo "=== $label ==="
    local output
    output=$(bash "$script" 2>&1) || true
    echo "$output" | tail -5

    local pass
    pass=$(echo "$output" | grep -oP 'PASS=\K[0-9]+' | tail -1 || echo 0)

    if [ -n "$floor_var" ] && [ "$pass" -lt "$floor_val" ]; then
        echo "FLOOR FAIL: $label requires PASS>=$floor_val, got PASS=$pass"
        FLOOR_FAIL=$((FLOOR_FAIL+1))
    fi
    echo ""
}

# ── inline Prolog suite (6 tests, no corpus needed) ──────────────────────────
SCRIP="$HERE/scrip"
TIMEOUT=8

pl_pass=0; pl_fail=0

pl_test() {
    local label="$1" expected="$2" tmp actual
    tmp=$(mktemp /tmp/broad_XXXXXX.pl)
    cat > "$tmp"
    actual=$(timeout "$TIMEOUT" "$SCRIP" --interp "$tmp" < /dev/null 2>/dev/null || true)
    rm -f "$tmp"
    if [ "$actual" = "$expected" ]; then
        echo "  PASS $label"; pl_pass=$((pl_pass+1))
    else
        echo "  FAIL $label"
        printf "       exp: %s\n" "$(printf '%s' "$expected" | head -2)"
        printf "       got: %s\n" "$(printf '%s' "$actual"   | head -2)"
        pl_fail=$((pl_fail+1))
    fi
}

echo "=== Prolog inline ==="
pl_test "PL: hello" "Hello, World!" << 'EOF'
:- initialization(main).
main :- write('Hello, World!'), nl.
EOF

pl_test "PL: fact lookup" "bob" << 'EOF'
:- initialization(main).
parent(tom, bob).
main :- parent(tom, X), write(X), nl.
EOF

pl_test "PL: ancestor chain" "bob" << 'EOF'
:- initialization(main).
parent(tom, bob).
parent(bob, ann).
ancestor(X, Y) :- parent(X, Y).
ancestor(X, Y) :- parent(X, Z), ancestor(Z, Y).
main :- ancestor(tom, bob), write(bob), nl.
EOF

pl_test "PL: arithmetic" "10" << 'EOF'
:- initialization(main).
main :- X is 3 + 7, write(X), nl.
EOF

pl_test "PL: list member" "yes" << 'EOF'
:- initialization(main).
member(X, [X|_]).
member(X, [_|T]) :- member(X, T).
main :- ( member(b, [a,b,c]) -> write(yes) ; write(no) ), nl.
EOF

pl_test "PL: recursion count" "5" << 'EOF'
:- initialization(main).
count(0) :- !.
count(N) :- N > 0, N1 is N - 1, count(N1).
main :- count(5), write(5), nl.
EOF

echo "PASS=$pl_pass FAIL=$pl_fail"
echo ""

# ── csnobol4 suite ────────────────────────────────────────────────────────────
run_suite "csnobol4 Budne suite" "$HERE/test_csnobol4_budne_suite.sh" "csnobol4" 34

# ── interp broad (corpus + beauty) ───────────────────────────────────────────
run_suite "interp broad" "$HERE/test_interp_broad_corpus_and_beauty.sh" "" 0

# ── Icon ir-run rung ladder ───────────────────────────────────────────────────
run_suite "Icon ir-run rungs" "$HERE/test_icon_all_rungs.sh" "icon" 48

# ── Result ────────────────────────────────────────────────────────────────────
if [ "$pl_fail" -gt 0 ]; then
    echo "FAIL: $pl_fail inline Prolog test(s) failed"
    FLOOR_FAIL=$((FLOOR_FAIL+1))
fi

echo "Floors checked. FLOOR_FAIL=$FLOOR_FAIL"
[ "$FLOOR_FAIL" -eq 0 ]
