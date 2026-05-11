#!/usr/bin/env bash
# test_isolation_ir_sm.sh — RS-15 grep gate.
#
# Verifies that SM-mode runtime files contain no references to the IR-only
# entry points. Comment-only references are allowed.
#
# IR-only symbols: execute_program, interp_eval, interp_eval_pat,
# interp_eval_ref, call_user_function. label_lookup is allowed since shared
# files (interp_label.c) define it; SM files just must not call it.
#
# Returns 0 on PASS (zero leaks), 1 on FAIL.

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

SM_FILES=(
    "$ROOT/src/runtime/x86/lower.c"
    "$ROOT/src/runtime/x86/sm_interp.c"
    "$ROOT/src/runtime/x86/sm_codegen.c"
    "$ROOT/src/runtime/x86/sm_prog.c"
    "$ROOT/src/runtime/x86/bb_broker.c"
    "$ROOT/src/runtime/x86/bb_boxes.c"
    "$ROOT/src/runtime/x86/bb_build.c"
    "$ROOT/src/runtime/x86/bb_emit.c"
    "$ROOT/src/runtime/x86/bb_flat.c"
    "$ROOT/src/runtime/x86/bb_pool.c"
    "$ROOT/src/runtime/x86/name_t.c"
    "$ROOT/src/runtime/x86/stmt_exec.c"
    "$ROOT/src/runtime/x86/snobol4_invoke.c"
    "$ROOT/src/runtime/x86/snobol4_pattern.c"
    "$ROOT/src/runtime/x86/snobol4_argval.c"
    "$ROOT/src/runtime/x86/eval_code.c"
    "$ROOT/src/runtime/interp/coro_runtime.c"
    "$ROOT/src/runtime/interp/pl_runtime.c"
    "$ROOT/src/runtime/interp/coro_value.c"
    "$ROOT/src/runtime/interp/coro_stmt.c"
)
# RS-17 / RS-18 / RS-19: coro_runtime.c and pl_runtime.c are full members
# of the SM-mode runtime gate.  Icon and Prolog Byrd-box drive go through
# bb_eval_value (coro_value.c) and bb_exec_stmt (coro_stmt.c).
# RS-23e (session 2026-05-05) promoted both BB adapters into this gate
# after the RS-23a/b/c/d/extra rungs and the diag binary
# (`scrip-rs23-diag` with `-Wl,--wrap=interp_eval`) verified zero
# `interp_eval` calls reach from any BB-adapter ancestor across smoke +
# unified_broker + full Icon corpus 263.  The two physical fallthroughs
# at coro_value.c:1382 and coro_stmt.c:269 were hardened to abort with a
# diagnostic; the `extern DESCR_t interp_eval(...)` declarations were
# removed from both files.  RS-23 (2026-05-03) had attempted this
# promotion prematurely after RS-22f-stmt; that attempt regressed
# smoke_icon/raku/unified_broker because indirect call paths went
# unenumerated.  The diag tooling enumerates them empirically — that is
# what closed RS-23 properly this time.

IR_SYMS=(
    "execute_program"
    "interp_eval"
    "interp_eval_pat"
    "interp_eval_ref"
    "call_user_function"
)

leaks=0
for f in "${SM_FILES[@]}"; do
    [ -f "$f" ] || continue
    for sym in "${IR_SYMS[@]}"; do
        # Look for the symbol followed by '(' (a call), ignoring lines that
        # are pure comments (start with /* or //, or are inside a /* ... */
        # block). Simple filter: skip lines whose code-portion is entirely
        # within a // or /* ... */ comment.
        hits=$(grep -n -E "\\b${sym}\\b[[:space:]]*\\(" "$f" 2>/dev/null \
               | grep -vE '^[0-9]+:[[:space:]]*(/\*|\*|//)' \
               || true)
        if [ -n "$hits" ]; then
            echo "FAIL  $f calls IR-only $sym:"
            echo "$hits" | sed 's/^/    /'
            leaks=$((leaks+1))
        fi
    done
done

if [ $leaks -gt 0 ]; then
    echo
    echo "FAIL  $leaks IR-only symbol leak(s) in SM runtime files"
    exit 1
fi

# CHUNKS-step06: structural rule — forbid EXPR_t* casts and EXPR_t field
# accesses in files that should now read pure SM/chunk, not raw IR.
#
# Initial scope: snobol4_invoke.c and snobol4_argval.c.  Both are zero-hit
# today (verified manually session #65) so the gate enforces the post-Step-4
# reality.
#
# Deferred to a later step (recorded in GOAL-CHUNKS Step 6 deferral note):
#   - snobol4_pattern.c — still contains the legacy DT_E thaw block reachable
#     via CONVERT(s,"EXPRESSION"), plus compile_to_expression itself.  Closing
#     this requires migrating CONVERT EXPRESSION to emit a chunk, not an
#     EXPR_t*.
#   - eval_code.c — contains eval_node, the IR walker itself.
#
# Match the five field accesses called out in GOAL-CHUNKS Step 6
# (->kind, ->children, ->nchildren, ->sval, ->ival) plus EXPR_t* casts.
SM_STRUCTURAL_FILES=(
    "$ROOT/src/runtime/x86/snobol4_invoke.c"
    "$ROOT/src/runtime/x86/snobol4_argval.c"
)
IR_FIELDS_RE='(\(EXPR_t[[:space:]]*\*\)|->kind\b|->children\b|->nchildren\b|->sval\b|->ival\b)'
struct_leaks=0
for f in "${SM_STRUCTURAL_FILES[@]}"; do
    [ -f "$f" ] || continue
    hits=$(grep -nE "$IR_FIELDS_RE" "$f" 2>/dev/null \
           | grep -vE '^[0-9]+:[[:space:]]*(/\*|\*|//)' \
           || true)
    if [ -n "$hits" ]; then
        echo "FAIL  $f contains EXPR_t cast or IR field access (CHUNKS-step06 structural rule):"
        echo "$hits" | sed 's/^/    /'
        struct_leaks=$((struct_leaks+1))
    fi
done
if [ $struct_leaks -gt 0 ]; then
    echo
    echo "FAIL  $struct_leaks structural leak(s) in SM runtime files"
    exit 1
fi

echo "PASS  no IR-only symbol leaks in SM runtime files"
exit 0
