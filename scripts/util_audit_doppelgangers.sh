#!/usr/bin/env bash
# util_audit_doppelgangers.sh — EDP-1 audit script
# Reports every SM opcode / XKIND_t box emission site outside
# sm_templates.c and bb_templates.c.
#
# Output format:
#   FILE:LINE  TYPE:NAME  (match text)
#
# Usage: bash scripts/util_audit_doppelgangers.sh [--brief]
#   --brief  print only summary counts, no per-line detail

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${HERE}/.."
SRC="${ROOT}/src/runtime/x86"

BRIEF=0
[[ "${1:-}" == "--brief" ]] && BRIEF=1

TRUTH_SM="${SRC}/sm_templates.c"
TRUTH_BB="${SRC}/bb_templates.c"

# ── Collect SM opcode names from sm_prog.h ──────────────────────────────────
SM_OPCODES=()
while IFS= read -r line; do
    name=$(echo "$line" | grep -oE '\bSM_[A-Z_]+\b' | head -1)
    [[ -n "$name" ]] && SM_OPCODES+=("$name")
done < <(grep -E '^\s+SM_[A-Z_]+\s*[,=]' "${ROOT}/src/runtime/x86/sm_prog.h")

# ── Collect XKIND names from snobol4_patnd.h ────────────────────────────────
BB_KINDS=()
while IFS= read -r line; do
    name=$(echo "$line" | grep -oE '\bX[A-Z]+\b' | head -1)
    [[ -n "$name" ]] && BB_KINDS+=("$name")
done < <(grep -E '^\s+X[A-Z]+\s*[,=]' "${ROOT}/src/runtime/x86/snobol4_patnd.h")

# ── Files to search (all x86 C files except the two truth files) ─────────────
SEARCH_FILES=()
while IFS= read -r f; do
    [[ "$f" != "$TRUTH_SM" && "$f" != "$TRUTH_BB" ]] && SEARCH_FILES+=("$f")
done < <(find "${SRC}" -name '*.c' -not -path '*/templates/*')

SM_HITS=0
BB_HITS=0

report_hit() {
    local file="$1" line_no="$2" kind="$3" name="$4" text="$5"
    if [[ $BRIEF -eq 0 ]]; then
        printf "  %-60s  %s:%s\n" "${file#$ROOT/}:${line_no}" "${kind}" "${name}"
        printf "      %s\n" "$text"
    fi
}

echo "=== EDP-1 Doppelganger Audit ==="
echo "Truth files:"
echo "  SM: ${TRUTH_SM#$ROOT/}"
echo "  BB: ${TRUTH_BB#$ROOT/}"
echo ""

echo "--- SM Opcode doppelgangers ---"
for op in "${SM_OPCODES[@]}"; do
    # Skip meta-names
    [[ "$op" == "SM_OPCODE_COUNT" ]] && continue
    # lowercase emit function name
    lop="${op#SM_}"
    lop="${lop,,}"
    emit_fn="emit_sm_${lop}"
    # Also look for case SM_X: patterns (inline switch emission)
    while IFS=: read -r file lineno text; do
        SM_HITS=$((SM_HITS + 1))
        report_hit "$file" "$lineno" "SM" "$op" "$text"
    done < <(grep -rn "case ${op}:\|${emit_fn}[^_]" "${SEARCH_FILES[@]}" 2>/dev/null | \
             grep -v '^\s*//' | grep -v 'sm_templates\.c\|bb_templates\.c')
done

echo ""
echo "--- BB Kind doppelgangers ---"
for kind in "${BB_KINDS[@]}"; do
    lkind="${kind,,}"
    emit_fn="emit_bb_${lkind}"
    # Look for: case KIND:, flat_emit_KIND, emit_bb_KIND, bb_KIND function def
    while IFS=: read -r file lineno text; do
        BB_HITS=$((BB_HITS + 1))
        report_hit "$file" "$lineno" "BB" "$kind" "$text"
    done < <(grep -rn "case ${kind}:\|${emit_fn}[^_]\|flat_emit_${lkind}\b" \
             "${SEARCH_FILES[@]}" 2>/dev/null | \
             grep -v '^\s*//' | grep -v 'sm_templates\.c\|bb_templates\.c')
done

echo ""
echo "=== Summary ==="
echo "SM doppelganger hits: ${SM_HITS}"
echo "BB doppelganger hits: ${BB_HITS}"
echo "Total: $((SM_HITS + BB_HITS))"
echo ""
echo "Target: 0 (all emission via sm_templates.c / bb_templates.c only)"
