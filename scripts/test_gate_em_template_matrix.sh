#!/usr/bin/env bash
# scripts/test_gate_em_template_matrix.sh — EC-UNI-8.3
#
# Machine-check the EC-UNI-8 invariant: every template fn carries an arm or
# `n/a` annotation for each cell of the backend × mode matrix (5 backends × 2
# modes = 10 cells, with JS_BIN as the standing n/a).
#
# Files in SM_templates/ and BB_templates/ may contain MULTIPLE grouped fns
# (per EC-UNI-8.1 regrouping); the gate walks each fn body individually using
# string-literal-aware brace matching.
#
# Pass rules per fn (top-level non-static `void`/`int`/`long` definition):
#   - The fn body contains either `IS_<BACKEND>_<MODE>` (arm implemented) OR
#     the n/a sentinel `<BACKEND>_<MODE>: n/a` (cell intentionally absent).
#   - BB_templates files never need IS_X86_* cells (BB x86 emission goes
#     through emit_flat_body, not bb_node); X86 cells are auto-skipped in BB.
#
# Exit 0 on full coverage, 1 with `[MATRIX-MISS]` lines on failure.
#
# Self-contained per RULES.md.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

exec python3 "$HERE/test_gate_em_template_matrix.py" "$ROOT"
