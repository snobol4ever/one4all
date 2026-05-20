#!/usr/bin/env bash
# test_gate_stage2_isolation.sh — stage2 handoff firewall.
#
# Invariant: the old shim-macro names — `g_registry`, `label_table`,
# `label_count`, `g_pl_pred_table`, `proc_table`, `proc_count` — must
# appear in source ONLY as qualified field references of stage2_t, never
# as bare/unqualified identifiers.
#
# ST2-1 introduced stage2_t with those six fields and a corresponding set
# of reader shim macros (`#define proc_table (g_stage2.proc_table)` etc.).
# ST2-1b deleted all six macros across four sub-steps (g_registry,
# label_table/label_count, g_pl_pred_table, proc_table/proc_count).  Today
# every reader of those tables resolves to either:
#
#     g_stage2.<field>     (deep-dispatch sites with no s2 in scope)
#     s2-><field>          (producer sites threading stage2_t *s2)
#
# A bare `proc_table[i]` or `proc_count++` in a future commit would mean
# someone reintroduced a shim macro (or relied on one that's been deleted).
# Both are regressions of the ST2-1b decision.  This gate catches that
# regression at grep time, before the slower compile/link/test cycle.
#
# Honest scope: this is a TOKEN firewall (same shape as the parse/runtime
# include firewalls — fast, lexical, false-positive-prone).  It does NOT
# prove that reads of g_stage2 fields are reachable only from the proper
# producer/reader sites.  A link-time isolation analogous to ISO-7 would
# close that gap; see ST2-2's followup in the goal file.
#
# Run: bash scripts/test_gate_stage2_isolation.sh
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
cd "$ROOT"

# The six field names that used to be shim macros.  Each must appear in
# source only as a qualified field reference (preceded by `.` or `->`).
FIELDS=(
    g_registry
    label_table
    label_count
    g_pl_pred_table
    proc_table
    proc_count
)

# Allowlist: file:identifier pairs where a bare reference is expected.
# Format: one entry per line, "<file>:<identifier>".
# Each entry must carry a comment above it explaining why.
ALLOW=(
    # stage2.h defines the struct itself — the field names appear bare
    # there as struct member declarations.  This is the source of truth.
    "src/include/stage2.h:g_registry"
    "src/include/stage2.h:label_table"
    "src/include/stage2.h:label_count"
    "src/include/stage2.h:g_pl_pred_table"
    "src/include/stage2.h:proc_table"
    "src/include/stage2.h:proc_count"
    # ScripModule (the per-language module-registry entry) has its own
    # `nprocs` field — renamed from `proc_count` in ST2-1 specifically to
    # avoid colliding with the (now-gone) shim macro.  The struct comment
    # in stage2.h still mentions the old name as documentation history.
    # No code change here; this is doc text only.

    # interp_private.h carries a doc comment line summarizing which shim
    # macros got deleted in ST2-1b — the field names appear in prose form
    # explaining the cleanup history.  No code reference, just documentation.
    "src/driver/interp_private.h:g_registry"
    "src/driver/interp_private.h:label_table"
    "src/driver/interp_private.h:label_count"

    # scrip_sm.c's sm_resolve_proc_entry_pcs prints a [CH-17a] diagnostic
    # banner that names 'proc_table' as a literal label inside the printf
    # format string.  The format string is human-facing text; the actual
    # code reads s2->proc_count and s2->proc_table[i] (both qualified).
    "src/driver/scrip_sm.c:proc_table"
)

violations=0
new_violations=()

# Negative-lookbehind regex: a field-name word NOT preceded by '.' or '>'.
# Perl-compatible regex; grep -P is required.
#
# We also skip:
#   - comments (lines starting with optional whitespace then '*' or '//' or '/*')
#   - string literals containing the names (rare; would be a false positive)
#   - the stage2.h struct definition (already on the allowlist)
#
# The grep is deliberately permissive on false positives — the allowlist
# is the proper escape hatch for any legitimate bare appearance.

for field in "${FIELDS[@]}"; do
    # Match the field as a whole word not preceded by '.' or '>'.
    # Then filter out comment lines (very rough, but catches the
    # historical-doc references in stage2.h).
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        file="${line%%:*}"
        rest="${line#*:}"
        lineno="${rest%%:*}"
        # Skip pure comment lines (line begins with * or // or /* after
        # leading whitespace, or contains the field only inside a /* */
        # block — we approximate by skipping any line whose pre-field
        # text contains '/*' without a matching '*/' before the field).
        # Simpler: skip lines that look like comments.
        text="${rest#*:}"
        # Trim leading whitespace.
        trimmed="$(echo "$text" | sed -E 's/^[[:space:]]+//')"
        case "$trimmed" in
            \**|"//"*|"/*"*) continue ;;
        esac
        # Check allowlist.
        key="$file:$field"
        ok=0
        for allowed in "${ALLOW[@]}"; do
            if [ "$key" = "$allowed" ]; then
                ok=1
                break
            fi
        done
        if [ $ok -eq 0 ]; then
            new_violations+=("$file:$lineno: bare reference to '$field'")
            violations=$((violations + 1))
        fi
    done < <(grep -rPn "(?<![\.\>_a-zA-Z0-9])${field}\b" src/ --include="*.c" --include="*.h" 2>/dev/null | grep -v "^Binary file" || true)
done

if [ $violations -gt 0 ]; then
    echo "FAIL stage2 isolation firewall: $violations bare reference(s) to former shim-macro field name(s):"
    for v in "${new_violations[@]}"; do echo "  $v"; done
    echo ""
    echo "These field names must always be qualified with 'g_stage2.' or 's2->'."
    echo "If the bare reference is legitimate (e.g. struct member declaration,"
    echo "historical-doc comment), add a 'file:field' entry to the ALLOW list"
    echo "in this script with a comment explaining why."
    exit 1
fi

echo "OK  stage2 isolation firewall: all references to {${FIELDS[*]}} are qualified"
echo "    (allowlist size: ${#ALLOW[@]} entries — see top of script)"
