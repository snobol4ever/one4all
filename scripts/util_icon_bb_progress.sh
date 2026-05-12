#!/usr/bin/env bash
# util_icon_bb_progress.sh — show GOAL-ICON-BB-NATIVE rung completion banner
# Usage: bash scripts/util_icon_bb_progress.sh [/path/to/.github]

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GITHUB="${1:-/home/claude/.github}"
GOAL="$GITHUB/GOAL-ICON-BB-NATIVE.md"

if [[ ! -f "$GOAL" ]]; then
    echo "SKIP  GOAL-ICON-BB-NATIVE.md not found at $GOAL"
    exit 0
fi

DONE=0
TOTAL=0
CURRENT=""

while IFS= read -r line; do
    if [[ "$line" =~ ^###\ (IB-[0-9]+) ]]; then
        rung="${BASH_REMATCH[1]}"
        # Count checkboxes in this rung
        done_in_rung=$(grep -c '^\- \[x\]' "$GOAL" 2>/dev/null || true)
        CURRENT="$rung"
    fi
    if [[ "$line" =~ ^\-\ \[x\] ]]; then DONE=$((DONE+1)); fi
    if [[ "$line" =~ ^\-\ \[.?\] ]];  then TOTAL=$((TOTAL+1)); fi
done < "$GOAL"

RUNGS_DONE=0
RUNGS_TOTAL=0
CURRENT_RUNG=""
in_ladder=0
while IFS= read -r line; do
    if [[ "$line" == "## Rung ladder" ]]; then in_ladder=1; continue; fi
    if [[ $in_ladder -eq 1 && "$line" =~ ^###\ (IB-[0-9]+) ]]; then
        RUNGS_TOTAL=$((RUNGS_TOTAL+1))
        rung="${BASH_REMATCH[1]}"
        # Check if all its checkboxes are done — look for any unchecked
        has_unchecked=0
        capture=0
        next_rung=0
        while IFS= read -r inner; do
            if [[ "$inner" =~ ^###\ IB- && "$inner" != "### $rung"* ]]; then
                if [[ $capture -eq 1 ]]; then next_rung=1; break; fi
            fi
            if [[ "$inner" == "### $rung"* ]]; then capture=1; continue; fi
            if [[ $capture -eq 1 && "$inner" =~ ^\-\ \[\ \] ]]; then has_unchecked=1; fi
        done < "$GOAL"
        if [[ $has_unchecked -eq 0 && $capture -eq 0 ]]; then
            # simpler: grep for unchecked in the rung block
            :
        fi
    fi
done < "$GOAL"

# Simpler approach: count ### IB-N sections and - [x] / - [ ] per section
python3 - "$GOAL" << 'PYEOF'
import sys, re
txt = open(sys.argv[1]).read()
sections = re.split(r'(?=^### IB-)', txt, flags=re.MULTILINE)
rungs = [(m.group(1), s) for s in sections if (m := re.match(r'### (IB-\d+)', s))]
done_rungs, total_rungs, current = 0, len(rungs), None
for name, body in rungs:
    unchecked = len(re.findall(r'^\- \[ \]', body, re.MULTILINE))
    checked   = len(re.findall(r'^\- \[x\]', body, re.MULTILINE))
    if unchecked == 0 and checked > 0:
        done_rungs += 1
    elif current is None:
        current = name

bar_len = 20
filled = int(bar_len * done_rungs / total_rungs) if total_rungs else 0
bar = '█' * filled + '░' * (bar_len - filled)

print()
print('╔══════════════════════════════════════════╗')
print('║      GOAL-ICON-BB-NATIVE  progress       ║')
print(f'║  [{bar}]  {done_rungs}/{total_rungs} rungs  ║')
print(f'║  Next rung: {(current or "COMPLETE"):<30}║')
print('╚══════════════════════════════════════════╝')
print()
PYEOF
