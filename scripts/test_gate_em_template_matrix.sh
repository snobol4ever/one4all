#!/usr/bin/env bash
# scripts/test_gate_em_template_matrix.sh — EC-UNI matrix gate
#
# Machine-check the EC-UNI invariant: every template fn carries an arm or
# `n/a` annotation for each backend in the 5-column matrix (X86, JVM, JS,
# NET, WASM).
#
# Per AXIS CORRECTION (GOAL-HEADQUARTERS, 2026-05-19): text-vs-binary is a
# serializer choice INSIDE each backend's output layer, NOT a matrix column.
#
# Files in SM_templates/ and BB_templates/ may contain MULTIPLE grouped fns
# (per EC-UNI-8.1 regrouping); the gate walks each fn body individually using
# string-literal-aware brace matching.
#
# Pass rules per fn (top-level non-static `void`/`int`/`long` definition):
#   - The fn body contains either `IS_<BACKEND>` (arm implemented) OR
#     the n/a sentinel `<BACKEND>: n/a` (cell intentionally absent).
#   - BB_templates files never need IS_X86 cells (BB x86 emission goes
#     through emit_flat_body, not bb_node); X86 cells are auto-skipped in BB.
#
# Cell count: 57 SM × 5 + 16 BB × 4 (X86 skipped) = 285 + 64 = 349 minimum;
# helper-fn pickups add a few more.  Current target: 365.
#
# Exit 0 on full coverage, 1 with `[MATRIX-MISS]` lines on failure.
#
# Self-contained per RULES.md.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

exec python3 "$HERE/test_gate_em_template_matrix.py" "$ROOT"
