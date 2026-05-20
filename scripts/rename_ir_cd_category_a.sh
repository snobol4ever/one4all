#!/usr/bin/env bash
# rename_ir_cd_category_a.sh — IR-CD-RENAME Category A.
# Rename BB-graph-table machinery misnamed dcg_* → bb_*.
# Idempotent: re-running produces zero changes after first run.
# DOES NOT touch Category B (Prolog DCG grammar in prolog_parse.c/prolog_lower.c)
# or Category C (icn_bb_dcg, pl_bb_dcg, *_dcg_state_t, lower_pat_dcg).
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="$(cd "$HERE/.." && pwd)"
cd "$ONE4ALL"

# Order: longest / most-prefix-specific first.
# Whole-word boundaries (\b) ensure no substring captures.
SUBS=(
    's/\bPL_DCG_TABLE_MAX\b/PL_BB_TABLE_MAX/g'
    's/\bSM_seq_dcg_add\b/SM_seq_bb_add/g'
    's/\bpl_dcg_register\b/pl_bb_register/g'
    's/\bpl_dcg_lookup\b/pl_bb_lookup/g'
    's/\bg_dcg_table\b/g_pl_bb_table/g'
    's/\bdcg_table\b/bb_table/g'
    's/\bdcg_count\b/bb_count/g'
    's/\bdcg_cap\b/bb_cap/g'
    's/\bdcg_idx\b/bb_idx/g'
    's/\bpat_dcg\b/pat_bb/g'
)

FILES=$(grep -rln '\b\(PL_DCG_TABLE_MAX\|SM_seq_dcg_add\|pl_dcg_register\|pl_dcg_lookup\|g_dcg_table\|dcg_table\|dcg_count\|dcg_cap\|dcg_idx\|pat_dcg\)\b' src/ --include="*.c" --include="*.h" 2>/dev/null || true)

if [ -z "$FILES" ]; then
    echo "OK no Category A occurrences found in src/ — already renamed or never present"
    exit 0
fi

echo "Files to edit:"
echo "$FILES" | sed 's/^/  /'

for f in $FILES; do
    for s in "${SUBS[@]}"; do
        sed -i "$s" "$f"
    done
done

echo "OK Category A rename complete in $(echo "$FILES" | wc -l) files"
