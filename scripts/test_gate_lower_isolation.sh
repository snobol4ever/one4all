#!/usr/bin/env bash
# test_gate_lower_isolation.sh — parse->lower boundary firewall.
#
# Invariant: src/lower/ must not reach into src/frontend/ except through a
# small, explicit allowlist of headers that contain shared infrastructure
# currently misfiled under frontend/.  Every allowlist entry is a known
# misfile with an owning relocation goal; the allowlist is a ratchet, not
# a permanent license.
#
# A future commit may shrink the allowlist (by moving a header out of
# frontend/) but must never grow it.  CI will reject any new include
# under src/lower/ that names src/frontend/.
#
# Run: bash scripts/test_gate_lower_isolation.sh
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
cd "$ROOT"

# Allowlist: exact include strings currently present under src/lower/.
# Each entry includes a tag describing why it exists and what would
# need to happen to remove it.
#
# Format: one substring per line, matched literally against the
# "include "frontend/foo.h"" portion (after the path-prefix is stripped).
ALLOW=(
    # scrip_cc.h: defines STMT_t, CODE_t, LANG_* enums, tree_t helper macros.
    #   This is universal AST/language infrastructure misfiled under
    #   frontend/snobol4/ for historical reasons (snobol4 was the first
    #   frontend).  54 files include it tree-wide.
    #   Owning relocation goal: move scrip_cc.h to src/include/scrip_lang.h.
    "frontend/snobol4/scrip_cc.h"

    # icon_lex.h: defines IcnTkKind enum (TK_AUG*, TK_PLUS, TK_MINUS, ...).
    #   lower.c reads these enum values when lowering Icon TT_AUGOP nodes
    #   to SM/BB.  The enum is a lex artifact but its values are used as
    #   stable opcode tags downstream.
    #   Owning relocation goal: extract IcnTkKind to src/include/icon_tk.h.
    "frontend/icon/icon_lex.h"

    # icon_gen.h: codegen types used by lower_icn.c during Icon procedure
    #   skeleton emission.  Closer to runtime than to lex/parse.
    #   Owning relocation goal: move to src/runtime/interp/.
    "frontend/icon/icon_gen.h"

    # raku_driver.h: declares the raku runtime API (raku_match, raku_compile,
    #   raku_grep, raku_capture, raku_meth_register, raku_die, raku_exception,
    #   raku_print_fh, ~20 functions).  These are runtime/builtin functions
    #   that happen to live under frontend/raku/ because they were authored
    #   alongside the Raku parser.
    #   Owning relocation goal: split raku_driver.h into raku_parse.h
    #   (frontend-only) and raku_runtime.h (relocated to src/runtime/).
    "frontend/raku/raku_driver.h"

    # term.h, prolog_runtime.h, prolog_atom.h: Prolog runtime data
    #   structures (Term, atom table, unification scaffolding).  Used by
    #   ir_exec.c for Prolog choice-point execution.  Belong under
    #   src/runtime/interp/ alongside pl_runtime.h.
    #   Owning relocation goal: relocate to src/runtime/interp/.
    "frontend/prolog/term.h"
    "frontend/prolog/prolog_runtime.h"
    "frontend/prolog/prolog_atom.h"
)

violations=0
new_violations=()

# Find every include directive under src/lower/ that points into src/frontend/.
while IFS= read -r line; do
    [ -z "$line" ] && continue
    # line is like: src/lower/lower.c:15:#include "../../frontend/icon/icon_lex.h"
    file="${line%%:*}"
    rest="${line#*:}"
    lineno="${rest%%:*}"
    # Extract the include path (between quotes or angle brackets).
    inc_path=$(echo "$rest" | sed -E 's/.*include[[:space:]]+["<]([^">]+)[">].*/\1/')
    # Normalize: strip leading ../ chains, keep "frontend/..." suffix.
    normalized=$(echo "$inc_path" | sed -E 's|^(\.\./)+||')
    if [[ "$normalized" != frontend/* ]]; then
        continue
    fi

    # Is it allowed?
    ok=0
    for allowed in "${ALLOW[@]}"; do
        if [ "$normalized" = "$allowed" ]; then
            ok=1
            break
        fi
    done
    if [ $ok -eq 0 ]; then
        new_violations+=("$file:$lineno: $normalized")
        violations=$((violations + 1))
    fi
done < <(grep -rn "include.*frontend/" src/lower/ 2>/dev/null || true)

# Count expected (allowlisted) entries actually present.
present=$(grep -rn "include.*frontend/" src/lower/ 2>/dev/null | wc -l)

if [ $violations -gt 0 ]; then
    echo "FAIL parse->lower firewall: $violations new include(s) into src/frontend/ not on allowlist:"
    for v in "${new_violations[@]}"; do echo "  $v"; done
    echo ""
    echo "If the new include is legitimate, add the header to the ALLOW list in"
    echo "this script with a comment explaining why and the relocation plan."
    echo "Prefer moving the header out of src/frontend/ instead."
    exit 1
fi

echo "OK  parse->lower firewall: $present include(s) under src/lower/ into src/frontend/, all allowlisted"
echo "    (allowlist size: ${#ALLOW[@]} entries — see top of script for relocation goals)"
