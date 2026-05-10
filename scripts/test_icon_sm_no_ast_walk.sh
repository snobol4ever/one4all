#!/bin/bash
# test_icon_sm_no_ast_walk.sh — run Icon corpus under SCRIP_NO_AST_WALK=1 --sm-run.
# Reports PASS=N where N grows as GOAL-ICON-BB-COMPLETE Phase A/B/C rungs land.
# A PASS here means "honest mode 3": SM dispatch did not fall back to the AST walker.
# A FAIL/ABORT here means the program still cheats (coro_eval reached from SM dispatch).
#
# A0 (GOAL-ICON-BB-COMPLETE): initial script; baseline N is small.
# Usage: bash scripts/test_icon_sm_no_ast_walk.sh [--corpus PATH] [--scrip PATH]

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIP="${SCRIP:-$HERE/../scrip}"
CORPUS="${CORPUS:-/home/claude/corpus/programs/icon}"

if [ ! -x "$SCRIP" ]; then echo "SKIP scrip not found at $SCRIP"; exit 0; fi
if [ ! -d "$CORPUS" ]; then echo "SKIP corpus not found at $CORPUS"; exit 0; fi

pass=0; fail=0; abort=0

for f in "$CORPUS"/rung*.icn; do
    name=$(basename "$f" .icn)
    ir_out=$(timeout 8 "$SCRIP" --ir-run "$f" < /dev/null 2>&1)
    ir_rc=$?

    # Only test programs that pass --ir-run (the oracle)
    [ $ir_rc -ne 0 ] && continue

    sm_out=$(timeout 8 bash -c "SCRIP_NO_AST_WALK=1 '$SCRIP' --sm-run '$f' < /dev/null 2>&1")
    sm_rc=$?

    if [ $sm_rc -eq 134 ] || echo "$sm_out" | grep -q "FATAL: .*reached from SM dispatch"; then
        abort=$((abort + 1))
    elif [ "$sm_out" = "$ir_out" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
    fi
done

echo "=== honest mode-3 (SCRIP_NO_AST_WALK=1 --sm-run) ==="
echo "PASS=$pass FAIL=$fail ABORT=$abort"
echo "(PASS = honest; ABORT = cheating via AST walker; FAIL = wrong output)"
[ $fail -eq 0 ] && [ $abort -eq 0 ] && exit 0 || exit 1
