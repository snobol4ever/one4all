#!/usr/bin/env bash
# rename_ir_to_bb_and_sm.sh — bulk rename for IR Rename rung (IR-RN-0).
# Applies word-boundary sed across one4all + corpus C/H/INC sources.
# Per-identifier explicit rules.  Idempotent.  Longer needles before shorter.
# Usage: bash scripts/rename_ir_to_bb_and_sm.sh
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="$HERE/.."
CORPUS="${CORPUS:-/home/claude/corpus}"
roots=("$ONE4ALL/src" "$ONE4ALL/include")
[ -d "$CORPUS" ] && roots+=("$CORPUS")
echo "Rename roots:"
for r in "${roots[@]}"; do echo "  $r"; done
# Gather files once.  C/H/INC only.  Skip generated parser/lexer outputs.
files=$(find "${roots[@]}" -type f \( -name '*.c' -o -name '*.h' -o -name '*.inc' \) \
        ! -name '*.tab.c' ! -name '*.tab.h' ! -name '*.lex.c' ! -name 'snobol4.c' 2>/dev/null)
file_count=$(echo "$files" | wc -l)
echo "Files to scan: $file_count"
# Build a single sed program with all word-boundary substitutions.
# Each rule is `s/\bOLD\b/NEW/g`.  Order: longer needles first within each cluster.
SED_PROG=$(cat <<'SED_RULES'
# --- Graph IR data types (longer first) ---
s/\bIR_block_t\b/BB_graph_t/g
s/\bIR_node_alloc\b/bb_node_alloc/g
s/\bIR_exec_once\b/bb_exec_once/g
s/\bIR_exec_pump\b/bb_exec_pump/g
s/\bIR_exec_node\b/bb_exec_node/g
s/\bIR_lower_pat\b/bb_lower_pat/g
s/\bIR_alloc\b/bb_alloc/g
s/\bIR_free\b/bb_free/g
s/\bIR_reset\b/bb_reset/g
s/\bIR_print\b/bb_print/g
s/\bIR_t\b/BB_t/g
s/\bIR_e\b/BB_op_t/g
# --- Graph IR tag families (explicit per-tag) ---
s/\bIR_LIT_I\b/BB_LIT_I/g
s/\bIR_LIT_S\b/BB_LIT_S/g
s/\bIR_LIT_F\b/BB_LIT_F/g
s/\bIR_LIT_NUL\b/BB_LIT_NUL/g
s/\bIR_PAT_ABORT\b/BB_PAT_ABORT/g
s/\bIR_PAT_ALT\b/BB_PAT_ALT/g
s/\bIR_PAT_ANY\b/BB_PAT_ANY/g
s/\bIR_PAT_ARB\b/BB_PAT_ARB/g
s/\bIR_PAT_ARBNO\b/BB_PAT_ARBNO/g
s/\bIR_PAT_ASSIGN_COND\b/BB_PAT_ASSIGN_COND/g
s/\bIR_PAT_ASSIGN_IMM\b/BB_PAT_ASSIGN_IMM/g
s/\bIR_PAT_BREAK\b/BB_PAT_BREAK/g
s/\bIR_PAT_CALLOUT\b/BB_PAT_CALLOUT/g
s/\bIR_PAT_CAT\b/BB_PAT_CAT/g
s/\bIR_PAT_EPS\b/BB_PAT_EPS/g
s/\bIR_PAT_FAIL\b/BB_PAT_FAIL/g
s/\bIR_PAT_FENCE\b/BB_PAT_FENCE/g
s/\bIR_PAT_LEN\b/BB_PAT_LEN/g
s/\bIR_PAT_LIT\b/BB_PAT_LIT/g
s/\bIR_PAT_NOTANY\b/BB_PAT_NOTANY/g
s/\bIR_PAT_POS\b/BB_PAT_POS/g
s/\bIR_PAT_REM\b/BB_PAT_REM/g
s/\bIR_PAT_SPAN\b/BB_PAT_SPAN/g
s/\bIR_PAT_TAB\b/BB_PAT_TAB/g
s/\bIR_PL_ALT\b/BB_PL_ALT/g
s/\bIR_PL_ARITH\b/BB_PL_ARITH/g
s/\bIR_PL_ATOM\b/BB_PL_ATOM/g
s/\bIR_PL_BUILTIN\b/BB_PL_BUILTIN/g
s/\bIR_PL_CALL\b/BB_PL_CALL/g
s/\bIR_PL_CHOICE\b/BB_PL_CHOICE/g
s/\bIR_PL_CUT\b/BB_PL_CUT/g
s/\bIR_PL_SEQ\b/BB_PL_SEQ/g
s/\bIR_PL_UNIFY\b/BB_PL_UNIFY/g
s/\bIR_PL_VAR\b/BB_PL_VAR/g
s/\bIR_ICN_ALTERNATE\b/BB_ICN_ALTERNATE/g
s/\bIR_ICN_BINOP\b/BB_ICN_BINOP/g
s/\bIR_ICN_EVERY\b/BB_ICN_EVERY/g
s/\bIR_ICN_FIELD_GET\b/BB_ICN_FIELD_GET/g
s/\bIR_ICN_FIELD_SET\b/BB_ICN_FIELD_SET/g
s/\bIR_ICN_FIND_GEN\b/BB_ICN_FIND_GEN/g
s/\bIR_ICN_IDX\b/BB_ICN_IDX/g
s/\bIR_ICN_IDX_SET\b/BB_ICN_IDX_SET/g
s/\bIR_ICN_ITERATE\b/BB_ICN_ITERATE/g
s/\bIR_ICN_KEYWORD\b/BB_ICN_KEYWORD/g
s/\bIR_ICN_KEY_GEN\b/BB_ICN_KEY_GEN/g
s/\bIR_ICN_LCONCAT\b/BB_ICN_LCONCAT/g
s/\bIR_ICN_LIMIT\b/BB_ICN_LIMIT/g
s/\bIR_ICN_LIST_BANG\b/BB_ICN_LIST_BANG/g
s/\bIR_ICN_PROC_GEN\b/BB_ICN_PROC_GEN/g
s/\bIR_ICN_RECORD_DEF\b/BB_ICN_RECORD_DEF/g
s/\bIR_ICN_SCAN\b/BB_ICN_SCAN/g
s/\bIR_ICN_SECTION\b/BB_ICN_SECTION/g
s/\bIR_ICN_SEQ_GEN\b/BB_ICN_SEQ_GEN/g
s/\bIR_ICN_TO_BY\b/BB_ICN_TO_BY/g
s/\bIR_ICN_TO_NESTED\b/BB_ICN_TO_NESTED/g
s/\bIR_ICN_TO\b/BB_ICN_TO/g
s/\bIR_ICN_UPTO\b/BB_ICN_UPTO/g
# --- Array IR data types ---
s/\bSM_Instr\b/SM_t/g
s/\bSM_Program\b/SM_sequence_t/g
s/\bsm_opcode_t\b/SM_op_t/g
s/\bsm_operand_t\b/SM_arg_t/g
s/\bSmExpression_t\b/SM_expr_t/g
# --- Array IR helpers (sm_prog_* -> sm_seq_*) ---
s/\bsm_prog_dcg_add\b/sm_seq_dcg_add/g
s/\bsm_prog_free\b/sm_seq_free/g
s/\bsm_prog_new\b/sm_seq_new/g
s/\bsm_prog_print\b/sm_seq_print/g
s/\bg_current_sm_prog\b/g_current_sm_seq/g
# --- Header include path rewrites ---
s|#include "IR\.h"|#include "BB.h"|g
s|#include "sm_prog\.h"|#include "SM.h"|g
s|#include <IR\.h>|#include <BB.h>|g
s|#include <sm_prog\.h>|#include <SM.h>|g
SED_RULES
)
# Apply.  Iterate files; sed -i with the entire program at once.
modified=0
echo "$files" | while read -r f; do
    [ -z "$f" ] && continue
    before=$(md5sum "$f" 2>/dev/null | awk '{print $1}')
    sed -i "$SED_PROG" "$f" 2>/dev/null
    after=$(md5sum "$f" 2>/dev/null | awk '{print $1}')
    if [ "$before" != "$after" ]; then
        modified=$((modified + 1))
        echo "  modified: $f"
    fi
done
# Rename the two header files.
IR_H="$ONE4ALL/src/include/IR.h"
SM_PROG_H="$ONE4ALL/src/include/sm_prog.h"
if [ -f "$IR_H" ]; then
    mv "$IR_H" "$ONE4ALL/src/include/BB.h"
    echo "  renamed: $IR_H -> BB.h"
fi
if [ -f "$SM_PROG_H" ]; then
    mv "$SM_PROG_H" "$ONE4ALL/src/include/SM.h"
    echo "  renamed: $SM_PROG_H -> SM.h"
fi
echo "Done."
