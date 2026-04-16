#!/usr/bin/env bash
# util_patch_plunit.sh — patch corpus/programs/prolog/plunit.pl to add
# determinism cuts that prevent double-run when a later suite fails mid-run.
#
# Root cause: pj_run_list/pj_run_suite/pj_run_tests leave choice points;
# backtracking re-enters earlier suites. Cuts make each step deterministic.
#
# Idempotent — checks for already-patched sentinel before applying.
# After patching, commits corpus repo.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORPUS=/home/claude/corpus
PLUNIT=$CORPUS/programs/prolog/plunit.pl

[ -f "$PLUNIT" ] || { echo "ERROR: $PLUNIT not found"; exit 1; }

# Sentinel: already patched?
if grep -q 'PATCHED:determinism-cuts' "$PLUNIT"; then
    echo "SKIP: plunit.pl already patched"
    exit 0
fi

echo "Patching $PLUNIT ..."

# We use Python for reliable multi-line sed-equivalent
python3 - "$PLUNIT" << 'PYEOF'
import sys, re

path = sys.argv[1]
src = open(path).read()

# 1. pj_run_list/2 — add cut after pj_run_suite succeeds
src = src.replace(
    'pj_run_list([H|T]) :- pj_run_suite(H), pj_run_list(T).',
    'pj_run_list([H|T]) :- pj_run_suite(H), !, pj_run_list(T).'
)

# 2. pj_run_suite/1 — add cut after pj_suite_verdict
old = 'pj_suite_verdict(Suite, SF).'
new = 'pj_suite_verdict(Suite, SF), !.'
src = src.replace(old, new, 1)  # only first occurrence (the call site)

# 3. pj_run_tests/2 — add cut after pj_run_one succeeds
src = src.replace(
    'pj_run_tests(Suite, [t(N,O,G)|Rest]) :-\n    pj_run_one(Suite,N,O,G), pj_run_tests(Suite,Rest).',
    'pj_run_tests(Suite, [t(N,O,G)|Rest]) :-\n    pj_run_one(Suite,N,O,G), !, pj_run_tests(Suite,Rest).'
)

# 4. pj_suite_verdict/2 — add cut inside the if-then-else to prevent backtrack
src = src.replace(
    'pj_suite_verdict(Suite, SF) :-\n    ( SF =:= 0 -> format(\'PASS ~w~n\',[Suite]) ; format(\'FAIL ~w~n\',[Suite]) ).',
    'pj_suite_verdict(Suite, SF) :- !\n    ( SF =:= 0 -> format(\'PASS ~w~n\',[Suite]) ; format(\'FAIL ~w~n\',[Suite]) ).'
)

# Add sentinel comment at top
src = '/* PATCHED:determinism-cuts */\n' + src

open(path, 'w').write(src)
print("OK")
PYEOF

echo "Committing corpus ..."
cd "$CORPUS"
git config user.name "LCherryholmes"
git config user.email "lcherryh@yahoo.com"
git add programs/prolog/plunit.pl
git commit -m "PL-12: plunit.pl determinism cuts — prevent double-run on backtrack"
echo "DONE"
