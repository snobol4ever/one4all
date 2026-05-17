#!/bin/bash
# test_prolog_bb_honest.sh — run Prolog corpus under SCRIP_NO_AST_WALK=1 --interp.
# Reports PASS=N where N grows as GOAL-PROLOG-BB-COMPLETE Phase A rungs land.
# A PASS here means "honest mode 3": SM dispatch did not fall back to the AST walker.
# A FAIL/ABORT here means the program still cheats (coro_eval reached from SM dispatch).
#
# PB-0 (GOAL-PROLOG-BB-COMPLETE): initial script; baseline N=17 (--ir-run oracle, 2026-05-12).
# Note: PLAN.md carve estimate of 31 was measured against .expected files (includes programs
# that --ir-run itself cannot run). This script uses --ir-run as oracle (stricter, correct).
# Usage: bash scripts/test_prolog_bb_honest.sh [--corpus PATH] [--scrip PATH]

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
CORPUS="${CORPUS:-/home/claude/corpus/programs/prolog}"

if [ ! -x "$SCRIP" ]; then echo "SKIP scrip not found at $SCRIP"; exit 0; fi
if [ ! -d "$CORPUS" ]; then echo "SKIP corpus not found at $CORPUS"; exit 0; fi

pass=0; fail=0; abort=0

for f in "$CORPUS"/rung*.pl; do
    name=$(basename "$f" .pl)
    base="${f%.pl}"

    # Use --ir-run as the oracle for expected output
    ir_out=$(timeout 8 "$SCRIP" --ir-run "$f" < /dev/null 2>&1)
    ir_rc=$?

    # Only test programs that pass --ir-run
    [ $ir_rc -ne 0 ] && continue

    sm_out=$(timeout 8 bash -c "SCRIP_NO_AST_WALK=1 '$SCRIP' --interp '$f' < /dev/null 2>&1")
    sm_rc=$?

    if [ $sm_rc -eq 134 ] || echo "$sm_out" | grep -q "FATAL: .*reached from SM dispatch"; then
        abort=$((abort + 1))
    elif [ "$sm_out" = "$ir_out" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
    fi
done

echo "=== honest mode-3 (SCRIP_NO_AST_WALK=1 --interp) ==="
echo "PASS=$pass FAIL=$fail ABORT=$abort"
echo "(PASS = honest; ABORT = cheating via AST walker; FAIL = wrong output)"
[ $fail -eq 0 ] && [ $abort -eq 0 ] && exit 0 || exit 1
