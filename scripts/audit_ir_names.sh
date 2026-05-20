#!/usr/bin/env bash
# audit_ir_names.sh — count every identifier the IR rename will touch.
# Self-contained. Reports per-needle counts across one4all + corpus.
# Usage: bash scripts/audit_ir_names.sh
#
# Reads source from both repos.  Writes a sorted summary to stdout.
# Exits 0 always — this is a measurement, not a gate.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONE4ALL="$HERE/.."
CORPUS="${CORPUS:-/home/claude/corpus}"
roots=("$ONE4ALL/src" "$ONE4ALL/include")
[ -d "$CORPUS" ] && roots+=("$CORPUS")
echo "Audit roots:"
for r in "${roots[@]}"; do echo "  $r"; done
echo
# Build the file list once — C/H sources plus markdown docs.
files_code=$(find "${roots[@]}" -type f \( -name '*.c' -o -name '*.h' -o -name '*.inc' \) 2>/dev/null)
files_docs=$(find "${roots[@]}" -type f -name '*.md' 2>/dev/null)
# count_needle PATTERN NAME [extra-grep-flag]
# Counts whole-word occurrences across code files only.
count_needle() {
    local pattern="$1" name="$2"
    local n
    n=$(echo "$files_code" | xargs -r grep -woE "$pattern" 2>/dev/null | wc -l)
    printf "  %-40s %s\n" "$name" "$n"
}
# count_prefix PATTERN NAME
# Counts identifiers matching a prefix pattern (e.g. IR_LIT_*).
count_prefix() {
    local pattern="$1" name="$2"
    local n
    n=$(echo "$files_code" | xargs -r grep -woE "$pattern" 2>/dev/null | wc -l)
    printf "  %-40s %s\n" "$name" "$n"
}
echo "=== Category 1 — Array IR data types (rename) ==="
count_needle 'SM_Instr'                          'SM_Instr -> SM_t'
count_needle 'SM_Program'                        'SM_Program -> SM_sequence_t'
count_needle 'sm_opcode_t'                       'sm_opcode_t -> SM_op_t'
count_needle 'sm_operand_t'                      'sm_operand_t -> SM_arg_t'
count_needle 'SmExpression_t'                    'SmExpression_t -> SM_expr_t'
echo
echo "=== Category 2 — Array IR helpers (sm_prog_* -> sm_seq_*) ==="
count_prefix 'sm_prog_[A-Za-z_]+'                'sm_prog_* (function family)'
count_needle 'g_current_sm_prog'                 'g_current_sm_prog -> g_current_sm_seq'
echo
echo "=== Category 3 — Graph IR data types (rename) ==="
count_needle 'IR_t'                              'IR_t -> BB_t'
count_needle 'IR_block_t'                        'IR_block_t -> BB_graph_t'
count_needle 'IR_e'                              'IR_e -> BB_op_t'
echo
echo "=== Category 4 — Graph IR node-kind tags (IR_* -> BB_*) ==="
count_prefix 'IR_LIT_[A-Z]+'                     'IR_LIT_* (literal kinds)'
count_prefix 'IR_PAT_[A-Z_]+'                    'IR_PAT_* (pattern kinds)'
count_prefix 'IR_PL_[A-Z_]+'                     'IR_PL_* (Prolog kinds)'
count_prefix 'IR_ICN_[A-Z_]+'                    'IR_ICN_* (Icon kinds)'
echo
echo "=== Category 5 — Graph IR functions (IR_* -> bb_*) ==="
count_needle 'IR_node_alloc'                     'IR_node_alloc -> bb_node_alloc'
count_needle 'IR_alloc'                          'IR_alloc -> bb_alloc'
count_needle 'IR_free'                           'IR_free -> bb_free'
count_needle 'IR_reset'                          'IR_reset -> bb_reset'
count_needle 'IR_print'                          'IR_print -> bb_print'
count_needle 'IR_exec_once'                      'IR_exec_once -> bb_exec_once'
count_needle 'IR_exec_pump'                      'IR_exec_pump -> bb_exec_pump'
count_needle 'IR_exec_node'                      'IR_exec_node -> bb_exec_node'
count_needle 'IR_lower_pat'                      'IR_lower_pat -> bb_lower_pat'
echo
echo "=== Category 6 — Headers ==="
n=$(echo "$files_code" | xargs -r grep -l '#include[[:space:]]*"IR.h"' 2>/dev/null | wc -l)
printf "  %-40s %s files\n" 'IR.h -> BB.h (#include sites)' "$n"
n=$(echo "$files_code" | xargs -r grep -l '#include[[:space:]]*"sm_prog.h"' 2>/dev/null | wc -l)
printf "  %-40s %s files\n" 'sm_prog.h -> SM.h (#include sites)' "$n"
echo
echo "=== Category 7 — RESERVED (must NOT match a rename pattern) ==="
n=$(echo "$files_code" | xargs -r grep -woE 'IR_LANG_[A-Z_]+' 2>/dev/null | wc -l)
printf "  %-40s %s (must stay unchanged)\n" 'IR_LANG_*' "$n"
n=$(echo "$files_code" | xargs -r grep -woE 'SM_INTERP_[A-Z_]+' 2>/dev/null | wc -l)
printf "  %-40s %s (must stay unchanged)\n" 'SM_INTERP_*' "$n"
n=$(echo "$files_code" | xargs -r grep -woE 'SM_CALL_STACK_MAX|SM_GEN_LOCAL_MAX|SM_MAX_OPERANDS' 2>/dev/null | wc -l)
printf "  %-40s %s (must stay unchanged)\n" 'SM_*_MAX constants' "$n"
echo
echo "=== Category 8 — Prose mentions in markdown ==="
for needle in 'SM_Instr' 'SM_Program' 'IR_t' 'IR_block_t' 'sm_opcode_t'; do
    n=$(echo "$files_docs" | xargs -r grep -wo "$needle" 2>/dev/null | wc -l)
    printf "  %-40s %s in *.md\n" "$needle" "$n"
done
echo
echo "Done.  No files modified."
