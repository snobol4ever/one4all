#!/usr/bin/env bash
# util_patch_plunit.sh — patch corpus/programs/prolog/plunit.pl:
#
# Fix 1 (determinism cuts): pj_run_list/pj_run_suite/pj_run_tests leave choice
#   points; backtracking re-enters earlier suites when a later suite fails.
#   Fix: ! after pj_run_suite in pj_run_list, after pj_suite_verdict in
#   pj_run_suite, after pj_run_one in pj_run_tests (body position, not head).
#
# Fix 2 (=@= undefined): test_bips/length gen_list uses =@= (structural
#   equivalence up to variable renaming). Not defined → pj_do_true crashes.
#   Fix: add X =@= Y using copy_term + ==.
#
# Fix 3 (pj_run_suite must not fail): wrap pj_run_suite call in pj_run_list
#   with (... -> true ; true) so a failing suite doesn't abort the list walk.
#
# Idempotent — checks for PATCHED:v2 sentinel before applying.
# After patching, commits corpus repo.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORPUS=/home/claude/corpus
PLUNIT=$CORPUS/programs/prolog/plunit.pl

[ -f "$PLUNIT" ] || { echo "ERROR: $PLUNIT not found"; exit 1; }

if grep -q 'PATCHED:v2' "$PLUNIT"; then
    echo "SKIP: plunit.pl already patched (v2)"
    exit 0
fi

echo "Patching $PLUNIT ..."

python3 - "$PLUNIT" << 'PYEOF'
import sys

path = sys.argv[1]
src = open(path).read()

# Remove any previous patch sentinel
src = src.replace('/* PATCHED:determinism-cuts */\n', '')

# Fix 1a: pj_run_list — use (pj_run_suite(H) -> true ; true) so a failing
#   suite never aborts the walk, then cut to prevent choice-point re-entry.
src = src.replace(
    'pj_run_list([H|T]) :- pj_run_suite(H), !, pj_run_list(T).',
    'pj_run_list([H|T]) :- ( pj_run_suite(H) -> true ; true ), !, pj_run_list(T).'
)
# Also handle the original unpatched form:
src = src.replace(
    'pj_run_list([H|T]) :- pj_run_suite(H), pj_run_list(T).',
    'pj_run_list([H|T]) :- ( pj_run_suite(H) -> true ; true ), !, pj_run_list(T).'
)

# Fix 1b: pj_run_suite — cut after verdict (body position)
src = src.replace(
    '    pj_suite_verdict(Suite, SF), !.',
    '    pj_suite_verdict(Suite, SF), !.'
)
# Handle unpatched form:
src = src.replace(
    '    pj_suite_verdict(Suite, SF).',
    '    pj_suite_verdict(Suite, SF), !.'
)

# Fix 1c: pj_run_tests — cut after pj_run_one (body position)
src = src.replace(
    'pj_run_tests(Suite, [t(N,O,G)|Rest]) :-\n    pj_run_one(Suite,N,O,G), !, pj_run_tests(Suite,Rest).',
    'pj_run_tests(Suite, [t(N,O,G)|Rest]) :-\n    pj_run_one(Suite,N,O,G), !, pj_run_tests(Suite,Rest).'
)
src = src.replace(
    'pj_run_tests(Suite, [t(N,O,G)|Rest]) :-\n    pj_run_one(Suite,N,O,G), pj_run_tests(Suite,Rest).',
    'pj_run_tests(Suite, [t(N,O,G)|Rest]) :-\n    pj_run_one(Suite,N,O,G), !, pj_run_tests(Suite,Rest).'
)

# Fix 2: add =@= (structural equivalence up to variable renaming)
# Insert after the stdlib section (after the last pj_insert line)
stdlib_anchor = 'pj_insert(X,[H|T],[H|R]) :- pj_insert(X,T,R).'
assert '=@=' not in src, "=@= already defined"
src = src.replace(
    stdlib_anchor,
    stdlib_anchor + '\nX =@= Y :- copy_term(X, X1), copy_term(Y, Y1), numbervars(X1,0,N), numbervars(Y1,0,N), X1 == Y1.'
)

# Sentinel v2
src = '/* PATCHED:v2 */\n' + src

open(path, 'w').write(src)
print("OK")
PYEOF

echo "Committing corpus ..."
cd "$CORPUS"
git config user.name "LCherryholmes"
git config user.email "lcherryh@yahoo.com"
git add programs/prolog/plunit.pl
git commit -m "PL-12: plunit.pl v2 — determinism cuts + =@= + safe suite walk"
echo "DONE"
